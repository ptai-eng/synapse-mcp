#include <synapse/transport.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
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
    connection c;
    std::atomic<int> messages{0};
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

TEST_CASE("M1.stdio.backpressure_stop", "[M1][stdio]") {
    connection c;
    std::atomic<int> closes{0};
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
