#include <synapse/detail/native_stdio.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <future>
#include <pthread.h>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace {
using namespace std::chrono_literals;
using synapse::detail::native_stdio;

struct private_pipe {
    int ends[2]{-1, -1};
    private_pipe() { REQUIRE(::pipe(ends) == 0); }
    ~private_pipe() { for (int fd : ends) if (fd >= 0) ::close(fd); }
    void close_reader() { ::close(std::exchange(ends[0], -1)); }
};

std::size_t descriptor_count() {
    auto* directory = ::opendir("/proc/self/fd");
    if (!directory) return 0; // Linux leak assertion; portable ownership checks remain below.
    std::size_t count = 0;
    while (::readdir(directory)) ++count;
    ::closedir(directory);
    return count;
}

struct stop_on_exit {
    native_stdio& io;
    ~stop_on_exit() { io.request_stop(); }
};

struct blocked_sigpipe {
    sigset_t previous{}, set{};
    blocked_sigpipe() {
        ::sigemptyset(&set);
        ::sigaddset(&set, SIGPIPE);
        REQUIRE(::pthread_sigmask(SIG_BLOCK, &set, &previous) == 0);
    }
    ~blocked_sigpipe() {
        const timespec timeout{0, 0};
        while (::sigtimedwait(&set, nullptr, &timeout) < 0 && errno == EINTR) {}
        ::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    }
};

void fill_pipe(int fd) {
    std::array<char, 4096> bytes{};
    while (::write(fd, bytes.data(), bytes.size()) > 0) {}
    REQUIRE(errno == EAGAIN);
}
} // namespace

static_assert(!std::is_copy_constructible_v<native_stdio>);
static_assert(std::is_nothrow_move_constructible_v<native_stdio>);

TEST_CASE("M1.posix.cancel_idle_read", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    auto io = std::move(*opened);
    std::promise<void> entering;
    auto ready = entering.get_future();
    auto result = std::async(std::launch::async, [&] {
        std::array<char, 16> bytes{};
        entering.set_value();
        return io.read_some(bytes);
    });
    stop_on_exit cleanup{io};
    REQUIRE(ready.wait_for(1s) == std::future_status::ready);
    REQUIRE(result.wait_for(30ms) == std::future_status::timeout);
    io.request_stop();
    REQUIRE(result.wait_for(1s) == std::future_status::ready);
    REQUIRE_FALSE(result.get());
    REQUIRE(::fcntl(input.ends[1], F_GETFD) >= 0); // The peer is still open.
}

TEST_CASE("M1.posix.partial_write", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    auto io = std::move(*opened);
    std::string bytes(1024 * 1024, 'x');
    for (std::size_t index = 0; index < bytes.size(); ++index)
        bytes[index] = static_cast<char>('a' + index % 26);
    std::string received(bytes.size(), '\0');
    auto reader = std::async(std::launch::async, [&] {
        std::size_t offset = 0;
        while (offset < received.size()) {
            auto count = ::read(output.ends[0], received.data() + offset,
                                (std::min)(std::size_t{997}, received.size() - offset));
            if (count > 0) offset += static_cast<std::size_t>(count);
            else if (count < 0 && errno == EINTR) continue;
            else return false;
        }
        return true;
    });
    std::size_t offset = 0;
    std::size_t writes = 0;
    while (offset < bytes.size()) {
        auto count = io.write_some(std::span<const char>(bytes).subspan(offset));
        REQUIRE(count);
        REQUIRE(*count > 0);
        offset += *count;
        ++writes;
    }
    REQUIRE(reader.wait_for(1s) == std::future_status::ready);
    REQUIRE(reader.get());
    REQUIRE(writes > 1);
    REQUIRE(received == bytes);
}

TEST_CASE("M1.posix.closed_stdout", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    output.close_reader();
    struct sigaction before{}, after{};
    REQUIRE(::sigaction(SIGPIPE, nullptr, &before) == 0);
    sigset_t mask_before{}, mask_after{}, pending{};
    REQUIRE(::pthread_sigmask(SIG_SETMASK, nullptr, &mask_before) == 0);
    const std::string bytes = "broken pipe";
    auto result = opened->write_some(bytes);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == synapse::sdk_errc::io_error);
    REQUIRE(::pthread_sigmask(SIG_SETMASK, nullptr, &mask_after) == 0);
    REQUIRE(::sigismember(&mask_before, SIGPIPE) == ::sigismember(&mask_after, SIGPIPE));
    REQUIRE(::sigpending(&pending) == 0);
    REQUIRE(::sigismember(&pending, SIGPIPE) == 0);
    REQUIRE(::sigaction(SIGPIPE, nullptr, &after) == 0);
    REQUIRE(before.sa_handler == after.sa_handler);
}

TEST_CASE("M1.posix.cancel_blocked_write", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    fill_pipe(output.ends[1]);
    const std::string bytes = "blocked";
    auto result = std::async(std::launch::async, [&] { return opened->write_some(bytes); });
    stop_on_exit cleanup{*opened};
    REQUIRE(result.wait_for(30ms) == std::future_status::timeout);
    opened->request_stop();
    opened->cancel_pending();
    REQUIRE(result.wait_for(1s) == std::future_status::ready);
    REQUIRE_FALSE(result.get());
}

TEST_CASE("M1.posix.stop_token_and_eof", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    std::stop_source source;
    auto result = std::async(std::launch::async, [&] {
        std::array<char, 8> bytes{};
        return opened->read_some(bytes, source.get_token());
    });
    stop_on_exit cleanup{*opened};
    REQUIRE(result.wait_for(30ms) == std::future_status::timeout);
    source.request_stop();
    REQUIRE(result.wait_for(1s) == std::future_status::ready);
    REQUIRE_FALSE(result.get());
    // Token cancellation does not stop the independent writer.
    REQUIRE(opened->write_some(std::string{"ok"}) == 2);
    ::close(std::exchange(input.ends[1], -1));
    std::array<char, 8> bytes{};
    REQUIRE(opened->read_some(bytes) == 0);
}

TEST_CASE("M1.posix.owned_duplicates_restore_flags", "[M1][posix]") {
    private_pipe input, output;
    const auto before_count = descriptor_count();
    const int input_flags = ::fcntl(input.ends[0], F_GETFL);
    const int output_flags = ::fcntl(output.ends[1], F_GETFL);
    for (int cycle = 0; cycle < 100; ++cycle) {
        auto opened = native_stdio::open(input.ends[0], output.ends[1]);
        REQUIRE(opened);
        auto moved = std::move(*opened);
        REQUIRE((::fcntl(input.ends[0], F_GETFL) & O_NONBLOCK) != 0);
        REQUIRE((::fcntl(output.ends[1], F_GETFL) & O_NONBLOCK) != 0);
        moved.request_stop();
    }
    REQUIRE(::fcntl(input.ends[0], F_GETFL) == input_flags);
    REQUIRE(::fcntl(output.ends[1], F_GETFL) == output_flags);
    REQUIRE(::fcntl(input.ends[0], F_GETFD) >= 0);
    REQUIRE(::fcntl(output.ends[1], F_GETFD) >= 0);
    if (before_count) REQUIRE(descriptor_count() == before_count);
    REQUIRE_FALSE(native_stdio::open(-1, output.ends[1]));
    REQUIRE(::fcntl(output.ends[1], F_GETFL) == output_flags);
    if (before_count) REQUIRE(descriptor_count() == before_count);
    REQUIRE_FALSE(native_stdio::open(input.ends[0], -1));
    REQUIRE(::fcntl(input.ends[0], F_GETFL) == input_flags);
    if (before_count) REQUIRE(descriptor_count() == before_count);
}

TEST_CASE("M1.posix.concurrent_token_isolation", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    fill_pipe(output.ends[1]);
    std::stop_source read_stop, write_stop;
    auto reader = std::async(std::launch::async, [&] {
        std::array<char, 8> bytes{};
        return opened->read_some(bytes, read_stop.get_token());
    });
    auto writer = std::async(std::launch::async, [&] {
        return opened->write_some(std::string{"blocked"}, write_stop.get_token());
    });
    stop_on_exit cleanup{*opened};
    REQUIRE(reader.wait_for(30ms) == std::future_status::timeout);
    REQUIRE(writer.wait_for(30ms) == std::future_status::timeout);
    // A retry wake must not introduce cancellation or steal the other wake.
    opened->cancel_pending();
    REQUIRE(reader.wait_for(30ms) == std::future_status::timeout);
    REQUIRE(writer.wait_for(30ms) == std::future_status::timeout);
    SECTION("read token cancels only the read") {
        read_stop.request_stop();
        REQUIRE(reader.wait_for(1s) == std::future_status::ready);
        REQUIRE_FALSE(reader.get());
        REQUIRE(writer.wait_for(30ms) == std::future_status::timeout);
        opened->request_stop();
        REQUIRE(writer.wait_for(1s) == std::future_status::ready);
        REQUIRE_FALSE(writer.get());
    }
    SECTION("write token cancels only the write") {
        write_stop.request_stop();
        REQUIRE(writer.wait_for(1s) == std::future_status::ready);
        REQUIRE_FALSE(writer.get());
        REQUIRE(reader.wait_for(30ms) == std::future_status::timeout);
        REQUIRE(::write(input.ends[1], "x", 1) == 1);
        REQUIRE(reader.wait_for(1s) == std::future_status::ready);
        REQUIRE(reader.get() == 1);
        // A later write with no cancelled token remains usable.
        std::array<char, 4096> bytes{};
        REQUIRE(::read(output.ends[0], bytes.data(), bytes.size()) > 0);
        REQUIRE(opened->write_some(std::string{"ok"}) == 2);
    }
    SECTION("global stop cancels both operations") {
        opened->request_stop();
        REQUIRE(reader.wait_for(1s) == std::future_status::ready);
        REQUIRE(writer.wait_for(1s) == std::future_status::ready);
        REQUIRE_FALSE(reader.get());
        REQUIRE_FALSE(writer.get());
    }
}

TEST_CASE("M1.posix.retry_wake_and_existing_nonblocking_flags", "[M1][posix]") {
    private_pipe input, output;
    REQUIRE(::fcntl(input.ends[0], F_SETFL, O_NONBLOCK) == 0);
    REQUIRE(::fcntl(output.ends[1], F_SETFL, O_NONBLOCK) == 0);
    const int input_flags = ::fcntl(input.ends[0], F_GETFL);
    const int output_flags = ::fcntl(output.ends[1], F_GETFL);
    {
        auto opened = native_stdio::open(input.ends[0], output.ends[1]);
        REQUIRE(opened);
        opened->cancel_pending();
        REQUIRE(::write(input.ends[1], "x", 1) == 1);
        std::array<char, 8> bytes{};
        REQUIRE(opened->read_some(bytes) == 1);
        REQUIRE(bytes[0] == 'x');
        REQUIRE(opened->write_some(std::string{"ok"}) == 2);
        std::stop_source stopped;
        stopped.request_stop();
        REQUIRE_FALSE(opened->read_some(bytes, stopped.get_token()));
        REQUIRE_FALSE(opened->write_some(std::string{"x"}, stopped.get_token()));
    }
    REQUIRE(::fcntl(input.ends[0], F_GETFL) == input_flags);
    REQUIRE(::fcntl(output.ends[1], F_GETFL) == output_flags);
}

TEST_CASE("M1.posix.preserves_preexisting_sigpipe", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    output.close_reader();
    blocked_sigpipe mask;
    REQUIRE(::raise(SIGPIPE) == 0);
    REQUIRE_FALSE(opened->write_some(std::string{"broken"}));
    sigset_t pending{}, current{};
    REQUIRE(::sigpending(&pending) == 0);
    REQUIRE(::sigismember(&pending, SIGPIPE) == 1);
    REQUIRE(::pthread_sigmask(SIG_SETMASK, nullptr, &current) == 0);
    REQUIRE(::sigismember(&current, SIGPIPE) == 1);
}

TEST_CASE("M1.posix.drains_final_bytes_before_eof", "[M1][posix]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.ends[0], output.ends[1]);
    REQUIRE(opened);
    REQUIRE(::write(input.ends[1], "last", 4) == 4);
    ::close(std::exchange(input.ends[1], -1));
    std::array<char, 8> bytes{};
    REQUIRE(opened->read_some(bytes) == 4);
    REQUIRE(std::string(bytes.data(), 4) == "last");
    REQUIRE(opened->read_some(bytes) == 0);
}
