#ifdef _WIN32
#define SYNAPSE_WINDOWS_STDIO_TEST_HOOKS
#endif
#include <synapse/transport.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <new>
#include <stdexcept>
#include <vector>
#include <string>
#include <thread>
#include <utility>
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

namespace {
using namespace std::chrono_literals;
using synapse::stdio_transport;
using endpoint = synapse::detail::native_stdio_endpoint;
struct pipe_pair {
#ifdef _WIN32
    endpoint reader = nullptr, writer = nullptr;
    pipe_pair() { if (!::CreatePipe(&reader, &writer, nullptr, 4096)) throw std::runtime_error("pipe"); }
    static void close(endpoint& fd) { if (fd) ::CloseHandle(std::exchange(fd, nullptr)); }
#else
    endpoint reader = -1, writer = -1;
    pipe_pair() { int fds[2]; if (::pipe(fds)) throw std::runtime_error("pipe"); reader=fds[0]; writer=fds[1]; }
    static void close(endpoint& fd) { if (fd >= 0) ::close(std::exchange(fd, -1)); }
#endif
    ~pipe_pair() { close(reader); close(writer); }
    void write(std::string_view bytes) {
        while (!bytes.empty()) {
#ifdef _WIN32
            DWORD count = 0;
            if (!::WriteFile(writer, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr)) throw std::runtime_error("write");
#else
            auto count = ::write(writer, bytes.data(), bytes.size());
            if (count <= 0) throw std::runtime_error("write");
#endif
            bytes.remove_prefix(count);
        }
    }
    std::string read_line(std::chrono::milliseconds budget = 1s) {
        std::string result;
        auto deadline = std::chrono::steady_clock::now() + budget;
        while (std::chrono::steady_clock::now() < deadline) {
#ifdef _WIN32
            DWORD available = 0;
            if (!::PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr)) break;
            if (!available) { std::this_thread::sleep_for(1ms); continue; }
            char byte; DWORD count=0;
            if (!::ReadFile(reader, &byte, 1, &count, nullptr) || !count) break;
#else
            pollfd fd{reader, POLLIN, 0};
            if (::poll(&fd, 1, 1) <= 0) continue;
            char byte;
            if (::read(reader, &byte, 1) != 1) break;
#endif
            result += byte;
            if (byte == '\n') break;
        }
        return result;
    }
};
struct connection {
    pipe_pair input, output;
    stdio_transport transport;
    explicit connection(synapse::stdio_options options = {})
        : transport(input.reader, output.writer, options) {}
};
}

TEST_CASE("M1.stdio.framing", "[M1][stdio]") {
    std::atomic<int> messages{0};
    connection c;
    REQUIRE(c.transport.start([&](const nlohmann::json& j) {
        if (j == nlohmann::json{{"text", "\xE2\x82\xAC"}}) ++messages;
    }));
    c.input.write("{bad}\n{\"text\":\"\xE2");
    c.input.write("\x82");
    c.input.write("\xAC\"}\n");
    pipe_pair::close(c.input.writer);
    REQUIRE(c.transport.wait_for(1s).value());
    REQUIRE(messages == 1);
    REQUIRE(c.output.read_line() == "{\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null,\"jsonrpc\":\"2.0\"}\n");
}

TEST_CASE("M1.stdio.numeric_overflow_recovers", "[M1][stdio]") {
    const auto number = GENERATE("1e1000", "-1e1000", "1E+1000", "-1E+1000");
    std::atomic<int> messages{0}, closes{0};
    std::atomic<synapse::close_reason> reason{synapse::close_reason::io_error};
    connection c;
    CAPTURE(number);
    REQUIRE(c.transport.start([&](const nlohmann::json& message) {
        if (message == nlohmann::json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "ping"}})
            ++messages;
    }, [&](auto why) { reason = why; ++closes; }));
    c.input.write(std::string("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\",\"params\":{\"x\":") +
        number + "}}\n{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}\n");
    pipe_pair::close(c.input.writer);
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(messages == 1);
    CHECK(closes == 1);
    CHECK(reason == synapse::close_reason::eof);
    CHECK(c.output.read_line() == "{\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null,\"jsonrpc\":\"2.0\"}\n");
    CHECK(c.output.read_line(10ms).empty());
}

TEST_CASE("M1.stdio.backpressure_stop", "[M1][stdio]") {
    std::atomic<int> closes{0};
    connection c;
    REQUIRE(c.transport.start([](const auto&) {}, [&](synapse::close_reason) { ++closes; }));
    REQUIRE(c.transport.send(std::string(512 * 1024, 'x')));
    const auto before = std::chrono::steady_clock::now();
    c.transport.stop();
    CHECK(std::chrono::steady_clock::now() - before < 100ms);
    c.transport.stop();
    REQUIRE(c.transport.wait_for(2s).value());
    CHECK(closes == 1);
    REQUIRE(c.transport.wait());
}


TEST_CASE("M1.stdio.frame_limits", "[M1][stdio]") {
    synapse::stdio_options options;
    options.max_frame_bytes = 7;
    SECTION("exact frame excludes LF") {
        std::atomic<int> seen{0};
        std::atomic<synapse::close_reason> reason{synapse::close_reason::io_error};
        connection c(options);
        REQUIRE(c.transport.start([&](const auto&) { ++seen; }, [&](auto why) { reason=why; }));
        c.input.write("{\"a\":1}\n");
        pipe_pair::close(c.input.writer);
        REQUIRE(c.transport.wait_for(1s).value());
        CHECK(seen == 1);
        CHECK(reason == synapse::close_reason::eof);
    }
    SECTION("one byte beyond closes before callback") {
        std::atomic<int> seen{0};
        std::atomic<synapse::close_reason> reason{synapse::close_reason::eof};
        connection c(options);
        REQUIRE(c.transport.start([&](const auto&) { ++seen; }, [&](auto why) { reason=why; }));
        c.input.write("{\"a\":10}");
        REQUIRE(c.transport.wait_for(1s).value());
        CHECK(seen == 0);
        CHECK(reason == synapse::close_reason::protocol_limit);
    }
}

TEST_CASE("M1.stdio.depth_before_parse", "[M1][stdio]") {
    synapse::stdio_options options;
    options.max_json_depth = 1;
    std::atomic<int> seen{0};
    std::atomic<synapse::close_reason> reason{synapse::close_reason::eof};
    connection c(options);
    REQUIRE(c.transport.start([&](const auto&) { ++seen; }, [&](auto why) { reason=why; }));
    // Braces and escaped quotes inside a string do not add container levels.
    c.input.write("{\"a\":\"[{\\\"\"}\n");
    c.input.write("[[0]]\n");
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(seen == 1);
    CHECK(reason == synapse::close_reason::protocol_limit);
}

TEST_CASE("M1.stdio.encoded_output", "[M1][stdio]") {
    synapse::stdio_options options;
    options.max_frame_bytes = 8;
    options.max_outbound_bytes = 9;
    connection c(options);
    REQUIRE(c.transport.start([](const auto&) {}));
    // Three LF characters need six escape bytes and two quotation marks.
    REQUIRE(c.transport.send("\n\n\n"));
    REQUIRE(c.output.read_line() == "\"\\n\\n\\n\"\n");
    auto beyond = c.transport.send("\n\n\nx");
    REQUIRE_FALSE(beyond);
    CHECK(beyond.error().code == synapse::sdk_errc::resource_exhausted);
    REQUIRE(c.transport.send("ok"));
    REQUIRE(c.output.read_line() == "\"ok\"\n");
    std::string invalid_utf8(1, static_cast<char>(0xff));
    auto invalid = c.transport.send(invalid_utf8);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code == synapse::sdk_errc::invalid_state);
    c.transport.stop();
    REQUIRE(c.transport.wait_for(1s).value());
}

TEST_CASE("M1.stdio.queue_includes_active_frame_and_LF", "[M1][stdio]") {
    const nlohmann::json frame = std::string(512 * 1024, 'q');
    const auto encoded = frame.dump().size();
    synapse::stdio_options options;
    options.max_frame_bytes = encoded;
    options.max_outbound_bytes = encoded + 1;
    SECTION("exact queue cap includes LF and active bytes") {
        connection c(options);
        REQUIRE(c.transport.start([](const auto&) {}));
        REQUIRE(c.transport.send(frame));
        // A frame larger than either private pipe's capacity remains active.
        std::this_thread::sleep_for(20ms);
        auto full = c.transport.send(0);
        REQUIRE_FALSE(full);
        CHECK(full.error().code == synapse::sdk_errc::resource_exhausted);
        c.transport.stop();
        REQUIRE(c.transport.wait_for(1s).value());
    }
    SECTION("one byte short rejects the frame") {
        --options.max_outbound_bytes;
        connection c(options);
        REQUIRE(c.transport.start([](const auto&) {}));
        REQUIRE_FALSE(c.transport.send(frame));
        c.transport.stop();
        REQUIRE(c.transport.wait_for(1s).value());
    }
}

TEST_CASE("M1.stdio.idle_stop_and_first_close", "[M1][stdio]") {
    std::atomic<int> closes{0};
    std::atomic<synapse::close_reason> reason{synapse::close_reason::eof};
    connection c;
    REQUIRE(c.transport.start([](const auto&) {}, [&](auto why) { reason=why; ++closes; }));
    REQUIRE_FALSE(c.transport.wait_for(5ms).value());
    c.transport.stop();
    pipe_pair::close(c.input.writer);
    c.transport.stop();
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(reason == synapse::close_reason::requested_stop);
    CHECK(closes == 1);
    REQUIRE(c.transport.wait());
    REQUIRE_FALSE(c.transport.send(0));
    REQUIRE_FALSE(c.transport.start([](const auto&) {}));
}

TEST_CASE("M1.stdio.eof_closes_once_and_flushes", "[M1][stdio]") {
    std::atomic<int> closes{0};
    std::atomic<synapse::close_reason> reason{synapse::close_reason::io_error};
    connection c;
    REQUIRE(c.transport.start([&](const auto&) { (void)c.transport.send("reply"); },
        [&](auto why) { reason=why; ++closes; }));
    c.input.write("{}\n");
    pipe_pair::close(c.input.writer);
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(c.output.read_line() == "\"reply\"\n");
    c.transport.stop();
    CHECK(reason == synapse::close_reason::eof);
    CHECK(closes == 1);
}

TEST_CASE("M1.stdio.flush_deadline", "[M1][stdio]") {
    synapse::stdio_options options;
    options.flush_budget = 40ms;
    connection c(options);
    REQUIRE(c.transport.start([](const auto&) {}));
    REQUIRE(c.transport.send(std::string(512 * 1024, 'x')));
    std::this_thread::sleep_for(10ms);
    const auto before = std::chrono::steady_clock::now();
    c.transport.stop();
    REQUIRE_FALSE(c.transport.wait_for(5ms).value());
    REQUIRE(c.transport.wait_for(1s).value());
    const auto elapsed = std::chrono::steady_clock::now() - before;
    CHECK(elapsed >= 30ms);
    CHECK(elapsed < 1s);
}

TEST_CASE("M1.stdio.callback_wait_is_invalid", "[M1][stdio]") {
    std::atomic<int> rejected{0};
    connection c;
    REQUIRE(c.transport.start([&](const auto&) {
        auto waited = c.transport.wait();
        if (!waited && waited.error().code == synapse::sdk_errc::invalid_state) ++rejected;
        auto timed = c.transport.wait_for(1s);
        if (!timed && timed.error().code == synapse::sdk_errc::invalid_state) ++rejected;
        c.transport.stop();
    }, [&](auto) {
        auto waited = c.transport.wait();
        if (!waited && waited.error().code == synapse::sdk_errc::invalid_state) ++rejected;
    }));
    c.input.write("{}\n");
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(rejected == 3);
}

TEST_CASE("M1.stdio.callback_exceptions_are_contained", "[M1][stdio]") {
    const int exception_kind = GENERATE(0, 1, 2, 3);
    std::atomic<int> closes{0};
    std::atomic<synapse::close_reason> reason{synapse::close_reason::eof};
    connection c;
    CAPTURE(exception_kind);
    REQUIRE(c.transport.start([exception_kind](const auto&) {
        if (exception_kind == 1 || exception_kind == 2) {
            const auto value = nlohmann::json::parse(exception_kind == 1 ? "bad" : "1e1000");
            (void)value;
        }
        if (exception_kind == 3) throw std::bad_alloc{};
        throw std::runtime_error("private callback details");
    },
        [&](auto why) { reason=why; ++closes; throw std::runtime_error("private close details"); }));
    c.input.write("{}\n");
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(reason == synapse::close_reason::io_error);
    CHECK(closes == 1);
    CHECK(c.output.read_line(10ms).empty());
}

TEST_CASE("M1.stdio.broken_output", "[M1][stdio]") {
    std::atomic<synapse::close_reason> reason{synapse::close_reason::eof};
    connection c;
    REQUIRE(c.transport.start([](const auto&) {}, [&](auto why) { reason=why; }));
    pipe_pair::close(c.output.reader);
    REQUIRE(c.transport.send("data"));
    REQUIRE(c.transport.wait_for(1s).value());
    CHECK(reason == synapse::close_reason::io_error);
}

TEST_CASE("M1.stdio.invalid_options_and_lifecycle", "[M1][stdio]") {
    for (int invalid = 0; invalid < 5; ++invalid) {
        synapse::stdio_options options;
        if (invalid == 0) options.max_frame_bytes = 0;
        if (invalid == 1) options.max_json_depth = 0;
        if (invalid == 2) options.max_outbound_bytes = 0;
        if (invalid == 3) options.flush_budget = 0ms;
        if (invalid == 4) options.flush_budget = 251ms;
        connection c(options);
        auto result = c.transport.start([](const auto&) {});
        REQUIRE_FALSE(result);
        CHECK(result.error().code == synapse::sdk_errc::invalid_state);
        REQUIRE(c.transport.wait());
    }
    connection c;
    c.transport.stop();
    REQUIRE(c.transport.wait());
    REQUIRE_FALSE(c.transport.start([](const auto&) {}));
    REQUIRE_FALSE(c.transport.send(0));
}

#ifdef _WIN32
namespace {
struct transport_io_gate {
    bool writing;
    std::promise<void> entered, released;
    std::shared_future<void> release = released.get_future().share();
    std::atomic<bool> reached{false}, opened{false};
    synapse::detail::windows_stdio_test_hook hook{[](void* context, bool is_write) {
        auto& gate = *static_cast<transport_io_gate*>(context);
        if (is_write == gate.writing && !gate.reached.exchange(true)) {
            auto released = gate.release;
            gate.entered.set_value(); released.wait();
        }
    }, this};
    explicit transport_io_gate(bool direction) : writing(direction) {
        synapse::detail::windows_stdio_before_io.store(&hook);
    }
    void open() { if (!opened.exchange(true)) released.set_value(); }
    ~transport_io_gate() { open(); synapse::detail::windows_stdio_before_io.store(nullptr); }
};
}
TEST_CASE("M1.stdio.windows_cancel_before_io", "[M1][stdio]") {
    const bool writing = GENERATE(false, true);
    transport_io_gate gate(writing);
    auto entered = gate.entered.get_future();
    connection c;
    struct release_before_join {
        transport_io_gate& gate;
        ~release_before_join() { gate.open(); }
    } release{gate};
    REQUIRE(c.transport.start([](const auto&) {}));
    if (writing) REQUIRE(c.transport.send(std::string(512 * 1024, 'x')));
    const auto reached = entered.wait_for(1s);
    c.transport.stop();
    // Cancellation has missed the native call while the gate holds it.
    gate.open();
    const auto finished = c.transport.wait_for(1s);
    REQUIRE(reached == std::future_status::ready);
    REQUIRE(finished.value());
}

TEST_CASE("M1.stdio.handle_and_callback_lifetime", "[M1][stdio]") {
    auto exercise = [] {
        std::atomic<bool> alive{true}, observed{false};
        std::promise<void> callback;
        auto completed = callback.get_future();
        connection c;
        REQUIRE(c.transport.start([&](const auto&) {
            observed = alive.load(); callback.set_value();
        }));
        c.input.write("{}\n");
        REQUIRE(completed.wait_for(1s) == std::future_status::ready);
        c.transport.stop();
        REQUIRE(c.transport.wait_for(1s).value());
        CHECK(observed.load());
        alive = false;
    };
    exercise();
    DWORD before=0, after=0;
    REQUIRE(::GetProcessHandleCount(::GetCurrentProcess(), &before));
    for (int i=0; i<50; ++i) exercise();
    for (int i=0; i<5; ++i) {
        synapse::stdio_options options;
        options.flush_budget = 5ms;
        connection c(options);
        REQUIRE(c.transport.start([](const auto&) {}));
        REQUIRE(c.transport.send(std::string(512 * 1024, 'x')));
        // Destruction itself must interrupt both idle input and blocked output.
    }
    REQUIRE(::GetProcessHandleCount(::GetCurrentProcess(), &after));
    CHECK(after == before);
    pipe_pair output;
    stdio_transport invalid(INVALID_HANDLE_VALUE, output.writer);
    REQUIRE_FALSE(invalid.start([](const auto&) {}));
    REQUIRE(invalid.wait());
}
#endif



TEST_CASE("M1.stdio.stop_does_not_run_close_callback", "[M1][stdio]") {
    struct release_guard {
        std::promise<void> release;
        ~release_guard() { release.set_value(); }
    };
    std::atomic<bool> entered{false};
    connection c;
    release_guard gate; // Releases callback before transport cleanup on failure.
    auto released = gate.release.get_future().share();
    REQUIRE(c.transport.start([](const auto&) {}, [&, released](auto) {
        entered = true;
        released.wait();
    }));
    const auto before = std::chrono::steady_clock::now();
    c.transport.stop();
    CHECK(std::chrono::steady_clock::now() - before < 100ms);
    auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!entered && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    CHECK(entered.load());
    // A deadline does not detach the callback or release its captured state.
    REQUIRE_FALSE(c.transport.wait_for(5ms).value());
}
