# ADR-002: Owned stdio work and cooperative shutdown

- Status: Runtime, text methods and native reference implemented through T15; full M1 qualification pending
- Date: 2026-10-25
- Implementation checkpoint: [T10 review record](../../tasks/checkpoint-t10.md), 2026-10-26
- Scope: [M1 native stdio specification](../../specs/001-native-stdio-foundation/spec.md)
- Product and revision: [ADR-001](0001-product-and-protocol.md)

## Decision

Use one owned application worker, immutable registrations after start, and separate interruptible stdio reader/writer work. The server borrows its transport; it joins all SDK-owned work before destruction. M1 does not provide a thread pool or execute application callbacks on its reader thread.

Remove the experimental `server::send_request` API and its outgoing promise map from the M1 server. Sampling and other server-originated requests are outside the accepted profile. Pending M1 operations mean admitted inbound requests and queued application work, not an unimplemented outbound compatibility subsystem. Record the breaking change in the v0.2 migration guide. The separate SDK client stays experimental.

At source commit `22c2126`, `server.hpp:188–216` detaches request threads capturing `this`, `server.hpp:100–103` only forwards stop to the transport, and `transport.hpp:31,52–59` cannot interrupt blocked `getline`. The borrowed constructor at `server.hpp:36–42` can remain small if its lifetime contract is explicit and shutdown actually joins work.

## Errors and text-only handler contract

Keep three error domains separate:

```cpp
enum class sdk_errc {
    invalid_state, invalid_schema, io_error,
    unsupported_stdio, resource_exhausted
};
struct sdk_error { sdk_errc code; std::string message; };
struct handler_error { std::string public_message; };

struct request_context { json_rpc_id id; std::stop_token stop; };
struct tool_result { std::string text; };
struct resource_result { std::string text; };
enum class prompt_role { user, assistant };
struct prompt_message { prompt_role role; std::string text; };
struct prompt_result {
    std::vector<prompt_message> messages;
    std::optional<std::string> description;
};
```

`protocol.hpp::error` remains a wire JSON-RPC error, not the general SDK error type. Do not automatically turn every `sdk_error` into an RPC response. `request_context.id` is a validated, non-null request ID; callbacks must not retain references to arguments or context beyond their invocation.

Tool, resource and prompt handlers return `std::expected<Result, handler_error>`. Tool arguments are `const nlohmann::json&`; prompt arguments are `const std::map<std::string, std::string>&`; resource handlers take only `const request_context&`. Every handler receives that context as its final parameter.

Tool success becomes one text content block. A tool handler failure becomes a tool result with `isError: true`; resource/prompt handler failures become sanitized internal RPC errors. Resource URI and MIME metadata come from the registered descriptor. Only `user` and `assistant` text prompt messages belong to this profile. Arbitrary callback-supplied RPC codes and raw JSON result envelopes are not the new handler contract.

## Server and transport lifetime

Keep a template constructor accepting borrowed `Transport&`, server identity and options. Transport and application state captured by callbacks must outlive the server and its joined work. Moving or destroying a borrowed transport while its server is bound is invalid. The server is non-copyable and non-movable.

| Server operation | Contract |
|---|---|
| `register_* -> expected<void, sdk_error>` | Allowed only while configuring; reject invalid descriptors/schemas or later mutation without changing the registry |
| `start() -> expected<void, sdk_error>` | One-shot transition that freezes registration; startup failure cleans partial work and leaves the instance stopped |
| `stop() noexcept` | Thread-safe, nonblocking, idempotent request to stop admission, request callback cancellation and interrupt transport I/O |
| `wait() -> expected<void, sdk_error>` | Join from the owning application thread; reject calls from SDK-owned threads instead of self-joining |
| `wait_for(milliseconds) -> expected<bool, sdk_error>` | Return false if work has not finished; never detach or destroy live state to meet a deadline |
| Destructor | Request stop and join; it can wait for a non-cooperative application callback |

Only one application owner joins at a time. Callbacks may call `stop()`, but may not destroy the server or wait for their own worker. Destruction inside an SDK callback violates the ownership contract. `stop()` before `start()` is safe and makes the one-shot instance stopped; repeating `wait()` after completion succeeds.

The M1 stdio transport contract is:

```cpp
start(message_callback, close_callback = {}) -> expected<void, sdk_error>;
send(const nlohmann::json&) -> expected<void, sdk_error>;
stop() noexcept;
wait() -> expected<void, sdk_error>;
wait_for(std::chrono::milliseconds) -> expected<bool, sdk_error>;
```

`message_callback` receives `const nlohmann::json&`. `close_callback` receives `close_reason`, one of `eof`, `requested_stop`, `io_error`, or `protocol_limit`. A successful start publishes one terminal close event; the first terminal cause wins. The close event means closing has begun, not that every thread has joined. Parsing and callback exceptions must not escape an I/O thread.

`send()` reports successful admission to the bounded writer queue, not confirmed delivery. A later I/O failure closes the transport. The defaulted close callback keeps an old experimental `start(callback)` call source-compatible; that does not verify the old client. Experimental process/SSE transports are not promoted to the M1 contract merely because their method names resemble it.

## Scheduling, lifecycle and cancellation

Keep server execution state (`configuring`, `starting`, `running`, `stopping`, `stopped`) separate from protocol state (`await_initialize`, `await_initialized`, `ready`, `closing`). The reader handles decoding and protocol control; application handlers and custom access/output policies run on the single worker. A slow handler therefore does not prevent the reader from observing EOF, cancellation or ping.

Application work uses a bounded FIFO. Track active/queued request IDs until their terminal outcome so duplicate IDs, cancellation and completion races have defined ownership. Stop rejects new admission, cancels queued work and requests the active handler's stop token. SDK code catches callback exceptions and does not expose `exception.what()` or raw schema-engine values to the host.

Support the selected revision's optional `notifications/cancelled` utility for known queued/active inbound IDs. Never cancel initialization. Ignore malformed, unknown, completed or late cancellation notifications and send no acknowledgement. If cancellation wins before response admission to the writer queue, suppress that request's response; an already queued response may still arrive. Cancellation does not forcibly terminate application code. [Selected cancellation rules](https://modelcontextprotocol.io/specification/2025-11-25/basic/utilities/cancellation).

## Limits and local policies

These are configurable product defaults, not MCP wire constants:

| Options | Defaults |
|---|---|
| `stdio_options.max_frame_bytes` | `1048576` encoded JSON bytes, excluding line delimiters |
| `stdio_options.max_json_depth` | `64` container levels; a root container is level one |
| `stdio_options.max_outbound_bytes` | `4194304` queued encoded bytes |
| `stdio_options.flush_budget` | `250ms` |
| `server_options.max_pending_requests` | `16` queued application requests, in addition to one active request |
| `server_options.tool_burst` | `16` initial/capacity tokens |
| `server_options.tool_requests_per_second` | `16` refill tokens per second |

Use a monotonic clock injectable by tests. Authorized tool-call attempts consume a token before schema validation, so invalid input does not bypass the invocation limit. Protocol control does not consume tool quota. Reject zero/invalid limits locally with `sdk_error`; do not silently disable protection.

`allow_tool` has signature `bool(std::string_view tool_name, const request_context&)`. An empty policy permits only registered tools; a supplied policy can deny a registered tool. `filter_output` has signature `expected<std::string, handler_error>(std::string_view operation, std::string_view text, const request_context&)`. All custom policy calls run on the application worker and follow the same cooperative/lifetime contract as handlers.

Core output checks require valid UTF-8 and reject C0 control characters except TAB/LF/CR, plus DEL; ESC is therefore rejected. Recheck the encoded frame limit after JSON escaping. An empty output hook does not bypass core checks. Rejected tool output becomes a generic tool failure; rejected resource/prompt output becomes a generic internal RPC error. These checks are explicit local content constraints, not a claim to detect every sensitive value or make all domain output universally safe.

The stdio transport exposes `max_encoded_frame_bytes()` so the server can check the final response before admission. A custom transport may expose the same accessor; otherwise the server uses the default 1 MiB frame cap. If a response ID makes even the sanitized fallback exceed that bound, the server stops without admitting an oversized frame.

Use Synapse admission RPC errors `-32050` for access denied, `-32051` for rate limited and `-32052` for queue full, with stable `error.data.reason` values `access_denied`, `rate_limited` and `queue_full`. Send them only for requests with safely validated IDs. Notifications never receive admission-error responses. Keep `-32002` for resource-not-found, as described by the selected resources specification; do not reuse it for quota errors. [Resource errors](https://modelcontextprotocol.io/specification/2025-11-25/server/resources#error-handling).

## Interruptible native I/O

The verified transport mode is pipe-backed stdio launched by a host. Native resources have explicit ownership; do not close borrowed process standard handles to unblock a read. Console/device cancellation outside that tested mode is not promised.

On POSIX, poll stdin or stdout together with an SDK-owned wake pipe. Use nonblocking writes and handle partial writes, `EINTR` and `EAGAIN`. Isolate SIGPIPE handling to the writer thread rather than replacing a process-wide signal handler. Duplicated descriptors share file-status flags: if flags are changed, require exclusive stdio use and restore them after joined I/O. [Linux poll](https://man7.org/linux/man-pages/man2/poll.2.html), [write semantics](https://man7.org/linux/man-pages/man2/write.2.html).

On Windows, use dedicated reader/writer threads and real duplicated Win32 thread handles for synchronous-I/O cancellation; a MinGW `std::thread::native_handle()` is not assumed to be a Win32 `HANDLE`. Set the stop flag before cancellation, prevent subsequent I/O, and retry/check completion in the owner wait path to cover the interval between the final stop check and entering an I/O call. Wait for completion before closing owned handles. A one-shot `CancelSynchronousIo` may miss that race and does not itself wait for completion. Do not treat `PeekNamedPipe` on synchronous handles as guaranteed nonblocking. [Cancellation API](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio), [cancellation synchronization](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations), [PeekNamedPipe caveat](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe).

Stopping transport I/O must cover a host that leaves stdin open and one that stops reading stdout. Flush already admitted output for at most the configured budget, then interrupt remaining SDK I/O. Stopping server admission prevents late handler results from entering a closing transport. No deadline detaches live work or force-kills an application callback; platform tests establish the two-second M1 subprocess budget only for idle/cooperative handlers on the declared pipe-backed configurations.

## Alternatives and verification

A detached thread per request has no bounded ownership or concurrency. A configurable pool and arbitrary event-loop dispatch would enlarge the contract before basic reliability is established. Running handlers on the reader would block control messages. A single owned worker provides a smaller supported execution model; future concurrency requires a separate design.

The implementation tasks must prove:

- Malformed valid JSON returns a protocol error and a following ping still works.
- Registration after start fails without mutation; callback concurrency never exceeds one.
- Stop reaches a gated handler's token; `wait_for` times out without destroying its state; releasing the gate lets join finish.
- Cancellation handles queued/active/late IDs and never acknowledges a notification or cancels initialization.
- Explicit stop interrupts idle open stdin and blocked stdout; EOF with cooperative work exits within two seconds.
- Repeated start/stop/destruction has no leaked descriptors/handles or worker access after owner destruction.
- Rate, queue, output and policy rejection produce their declared channels; API errors are not accidentally serialized as wire errors.

This record selects the contract. Passing those tests and the M1 interoperability gates is still implementation work.
