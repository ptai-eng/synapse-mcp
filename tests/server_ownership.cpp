#include <synapse/mcp.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <condition_variable>
#include <future>
#include <limits>
#include <type_traits>
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

namespace {
struct counting_transport {
    int starts = 0;
    bool fail = false;
    std::function<void(const nlohmann::json&)> message;
    std::function<void(synapse::close_reason)> close;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<nlohmann::json> output;
    std::atomic<bool> stopped{false};
    std::expected<void, synapse::sdk_error> start(decltype(message) cb,
        decltype(close) closed = {}) {
        ++starts; message = std::move(cb); close = std::move(closed);
        if (fail) return std::unexpected(synapse::sdk_error{synapse::sdk_errc::io_error, "fixture startup failure"});
        return {};
    }
    std::expected<void, synapse::sdk_error> send(const nlohmann::json& wire) {
        std::lock_guard lock(mutex); output.push_back(wire); changed.notify_all(); return {};
    }
    void stop() noexcept { if (!stopped.exchange(true) && close) close(synapse::close_reason::requested_stop); }
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
    nlohmann::json next() {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(1), [&] { return !output.empty(); })) return nullptr;
        auto wire = output.front(); output.erase(output.begin()); return wire;
    }
    void ready() {
        message({{"jsonrpc", "2.0"}, {"id", 0}, {"method", "initialize"}, {"params", {
            {"protocolVersion", "2025-11-25"}, {"capabilities", nlohmann::json::object()},
            {"clientInfo", {{"name", "test"}, {"version", "1"}}}}}});
        REQUIRE(next().contains("result"));
        message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    }
    void read(int id) { message({{"jsonrpc", "2.0"}, {"id", id}, {"method", "resources/read"}, {"params", {{"uri", "app://test"}}}}); }
};
using namespace std::chrono_literals;
synapse::resource descriptor() { return {"app://test", "Test", "", std::nullopt}; }
auto handler() { return [](const auto&) -> std::expected<synapse::resource_result, synapse::handler_error> { return synapse::resource_result{"ok"}; }; }

struct stdio_pipe {
    using endpoint = synapse::detail::native_stdio_endpoint;
#ifdef _WIN32
    endpoint reader = nullptr, writer = nullptr;
    stdio_pipe() { if (!::CreatePipe(&reader, &writer, nullptr, 4096)) throw std::runtime_error("pipe"); }
    static void close(endpoint& value) { if (value) ::CloseHandle(std::exchange(value, nullptr)); }
#else
    endpoint reader = -1, writer = -1;
    stdio_pipe() { int values[2]; if (::pipe(values)) throw std::runtime_error("pipe"); reader = values[0]; writer = values[1]; }
    static void close(endpoint& value) { if (value >= 0) ::close(std::exchange(value, -1)); }
#endif
    ~stdio_pipe() { close(reader); close(writer); }
    void write_line(const nlohmann::json& wire) {
        auto bytes = wire.dump() + '\n';
        std::size_t offset = 0;
        while (offset < bytes.size()) {
#ifdef _WIN32
            DWORD count = 0;
            if (!::WriteFile(writer, bytes.data() + offset, static_cast<DWORD>(bytes.size() - offset), &count, nullptr) || !count) throw std::runtime_error("write");
#else
            const auto count = ::write(writer, bytes.data() + offset, bytes.size() - offset);
            if (count <= 0) throw std::runtime_error("write");
#endif
            offset += count;
        }
    }
    nlohmann::json read_line() {
        std::string line;
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
#ifdef _WIN32
            DWORD available = 0;
            if (!::PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr)) return nullptr;
            if (!available) { std::this_thread::sleep_for(1ms); continue; }
            char byte; DWORD count = 0;
            if (!::ReadFile(reader, &byte, 1, &count, nullptr) || !count) return nullptr;
#else
            pollfd fd{reader, POLLIN, 0};
            if (::poll(&fd, 1, 1) <= 0) continue;
            char byte;
            if (::read(reader, &byte, 1) != 1) return nullptr;
#endif
            if (byte == '\n') return nlohmann::json::parse(line);
            line += byte;
        }
        return nullptr;
    }
};
void initialize_pipe(stdio_pipe& input, stdio_pipe& output) {
    input.write_line({{"jsonrpc", "2.0"}, {"id", 0}, {"method", "initialize"}, {"params", {
        {"protocolVersion", "2025-11-25"}, {"capabilities", nlohmann::json::object()},
        {"clientInfo", {{"name", "pipe-test"}, {"version", "1"}}}}}});
    REQUIRE(output.read_line().at("result").at("protocolVersion") == "2025-11-25");
    input.write_line({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
}
}
TEST_CASE("M1.ownership.stop_before_start", "[M1][ownership]") {
    counting_transport transport;
    synapse::server server(transport);
    server.stop();
    CHECK_FALSE(server.start());
    CHECK(transport.starts == 0);
    REQUIRE(server.wait());
    REQUIRE(server.wait());
}

TEST_CASE("M1.ownership.frozen_and_invalid_options", "[M1][ownership]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<synapse::server>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<synapse::server>);
    counting_transport t;
    synapse::server s(t);
    CHECK_FALSE(s.register_resource({"", "bad", "", {}}, handler()));
    REQUIRE(s.register_resource(descriptor(), handler()));
    CHECK_FALSE(s.register_resource(descriptor(), handler()));
    REQUIRE(s.start());
    CHECK_FALSE(s.start());
    CHECK_FALSE(s.register_resource({"app://new", "New", "", {}}, handler()));
    t.ready();
    t.message({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "resources/list"}});
    CHECK(t.next().at("result").at("resources").size() == 1);
    s.stop(); REQUIRE(s.wait());
    for (int kind = 0; kind < 5; ++kind) {
        counting_transport invalid;
        synapse::server_options options;
        if (kind == 0) options.max_pending_requests = 0;
        if (kind == 1) options.tool_burst = 0;
        if (kind == 2) options.tool_requests_per_second = 0;
        if (kind == 3) options.tool_requests_per_second = std::numeric_limits<double>::infinity();
        if (kind == 4) options.now = {};
        synapse::server bad(invalid, {}, options);
        CHECK_FALSE(bad.start()); CHECK(invalid.starts == 0);
        CHECK_FALSE(bad.register_resource(descriptor(), handler()));
    }
}

TEST_CASE("M1.ownership.start_failure_cleanup", "[M1][ownership]") {
    for (int i = 0; i < 20; ++i) {
        counting_transport t; t.fail = true;
        synapse::server s(t);
        REQUIRE_FALSE(s.start());
        CHECK(t.stopped.load());
        REQUIRE(s.wait_for(1s).value());
        CHECK_FALSE(s.start());
        CHECK_FALSE(s.register_resource(descriptor(), handler()));
    }
}

TEST_CASE("M1.ownership.callback_stop_and_self_wait", "[M1][ownership]") {
    counting_transport t;
    std::promise<void> finished;
    std::atomic<bool> wait_rejected{false}, timed_wait_rejected{false}, stop_fast{false};
    synapse::server s(t);
    REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& ctx) -> std::expected<synapse::resource_result, synapse::handler_error> {
        wait_rejected = !s.wait(); timed_wait_rejected = !s.wait_for(1ms);
        std::stop_callback reentry(ctx.stop, [&] { s.stop(); });
        const auto before = std::chrono::steady_clock::now();
        s.stop(); s.stop();
        stop_fast = std::chrono::steady_clock::now() - before < 100ms;
        finished.set_value(); return synapse::resource_result{"suppressed"};
    }));
    REQUIRE(s.start()); t.ready(); t.read(1);
    REQUIRE(finished.get_future().wait_for(1s) == std::future_status::ready);
    CHECK(wait_rejected); CHECK(timed_wait_rejected); CHECK(stop_fast);
    REQUIRE(s.wait_for(1s).value());
    CHECK(t.output.empty());
}

TEST_CASE("M1.ownership.timeout_retains_state_and_exact_cleanup", "[M1][ownership]") {
#ifdef _WIN32
    DWORD baseline = 0; REQUIRE(::GetProcessHandleCount(::GetCurrentProcess(), &baseline));
#endif
    for (int iteration = 0; iteration < 20; ++iteration) {
        counting_transport t;
        std::mutex mutex; std::condition_variable changed;
        bool released = false;
        std::promise<void> entered;
        std::atomic<bool> saw_cancel{false};
        synapse::server s(t);
        struct release_on_exit {
            std::mutex& mutex; std::condition_variable& changed; bool& released;
            ~release_on_exit() { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
        } release{mutex, changed, released};
        REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& ctx) -> std::expected<synapse::resource_result, synapse::handler_error> {
            entered.set_value();
            std::unique_lock lock(mutex); changed.wait(lock, [&] { return released; });
            saw_cancel = ctx.stop.stop_requested(); return synapse::resource_result{"late"};
        }));
        REQUIRE(s.start()); t.ready(); t.read(1);
        REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
        s.stop(); CHECK_FALSE(s.wait_for(1ms).value());
        { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
        REQUIRE(s.wait_for(1s).value()); CHECK(saw_cancel);
        REQUIRE(s.wait()); CHECK(t.output.empty());
    }
#ifdef _WIN32
    DWORD after = 0; REQUIRE(::GetProcessHandleCount(::GetCurrentProcess(), &after)); CHECK(after == baseline);
#endif
}

TEST_CASE("M1.ownership.real_stdio_eof", "[M1][ownership][stdio-integration]") {
    stdio_pipe input, output;
    synapse::stdio_transport transport(input.reader, output.writer);
    synapse::server s(transport);
    REQUIRE(s.register_resource(descriptor(), handler()));
    REQUIRE(s.start()); initialize_pipe(input, output);
    input.write_line({{"jsonrpc", "2.0"}, {"id", "resource"}, {"method", "resources/read"}, {"params", {{"uri", "app://test"}}}});
    CHECK(output.read_line().at("result").at("contents")[0].at("text") == "ok");
    const auto before = std::chrono::steady_clock::now();
    stdio_pipe::close(input.writer);
    REQUIRE(s.wait_for(2s).value());
    CHECK(std::chrono::steady_clock::now() - before < 2s);
    CHECK_FALSE(s.start());
    REQUIRE(s.wait());
}

TEST_CASE("M1.ownership.real_stdio_eof_cancels_active", "[M1][ownership][stdio-integration]") {
    stdio_pipe input, output;
    std::promise<void> entered;
    std::atomic<bool> cancelled{false};
    synapse::stdio_transport transport(input.reader, output.writer);
    synapse::server s(transport);
    REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& context) -> std::expected<synapse::resource_result, synapse::handler_error> {
        std::mutex mutex;
        std::condition_variable_any changed;
        std::unique_lock lock(mutex);
        entered.set_value();
        changed.wait(lock, context.stop, [] { return false; });
        cancelled = context.stop.stop_requested();
        return synapse::resource_result{"suppressed"};
    }));
    REQUIRE(s.start()); initialize_pipe(input, output);
    input.write_line({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "resources/read"}, {"params", {{"uri", "app://test"}}}});
    REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
    stdio_pipe::close(input.writer);
    REQUIRE(s.wait_for(2s).value());
    CHECK(cancelled);
}

TEST_CASE("M1.ownership.destructor_joins_before_state_dies", "[M1][ownership]") {
    counting_transport t;
    std::promise<void> entered;
    std::atomic<bool> returned{false}, state_alive{true};
    {
        synapse::server s(t);
        REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& context) -> std::expected<synapse::resource_result, synapse::handler_error> {
            std::mutex mutex;
            std::condition_variable_any changed;
            std::unique_lock lock(mutex);
            entered.set_value();
            changed.wait(lock, context.stop, [] { return false; });
            returned = state_alive.load();
            return synapse::resource_result{"suppressed"};
        }));
        REQUIRE(s.start()); t.ready(); t.read(1);
        REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
    }
    state_alive = false;
    CHECK(returned);
    CHECK(t.stopped.load());
    CHECK(t.output.empty());
}
