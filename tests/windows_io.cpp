#define SYNAPSE_WINDOWS_STDIO_TEST_HOOKS
#include <synapse/detail/native_stdio.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <future>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace {
using namespace std::chrono_literals;
using synapse::detail::native_stdio;

struct private_pipe {
    HANDLE reader = nullptr;
    HANDLE writer = nullptr;
    private_pipe() { REQUIRE(::CreatePipe(&reader, &writer, nullptr, 4096)); }
    ~private_pipe() {
        if (reader) ::CloseHandle(reader);
        if (writer) ::CloseHandle(writer);
    }
};

// Hold the operation after its final stop check and before the Win32 call.
// Cancellation here necessarily misses the not-yet-pending synchronous I/O.
struct io_gate {
    std::promise<void> entered;
    std::promise<void> released;
    std::shared_future<void> release = released.get_future().share();
    synapse::detail::windows_stdio_test_hook hook{[](void* context, bool) {
        auto& gate = *static_cast<io_gate*>(context);
        gate.entered.set_value();
        gate.release.wait();
    }, this};
    io_gate() { synapse::detail::windows_stdio_before_io.store(&hook); }
    ~io_gate() { synapse::detail::windows_stdio_before_io.store(nullptr); }
};

template<class Future>
bool finish_cancel(native_stdio& io, Future& result) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    do {
        io.cancel_pending();
        if (result.wait_for(1ms) == std::future_status::ready) return true;
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

DWORD handle_count() {
    DWORD count = 0;
    REQUIRE(::GetProcessHandleCount(::GetCurrentProcess(), &count));
    return count;
}

void exercise_lifecycle(private_pipe& input, private_pipe& output) {
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    auto io = std::move(*opened);
    DWORD count = 0;
    const char value = 'x';
    REQUIRE(::WriteFile(input.writer, &value, 1, &count, nullptr));
    std::array<char, 1> bytes{};
    REQUIRE(io.read_some(bytes) == 1);
    REQUIRE(bytes[0] == value);
    REQUIRE(io.write_some(bytes) == 1);
    REQUIRE(::ReadFile(output.reader, bytes.data(), 1, &count, nullptr));
    io.request_stop();
}

void exercise_cancelled_lifecycle(private_pipe& input, private_pipe& output) {
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    auto result = std::async(std::launch::async, [&] {
        std::array<char, 1> bytes{};
        return opened->read_some(bytes);
    });
    opened->request_stop();
    REQUIRE(finish_cancel(*opened, result));
    REQUIRE_FALSE(result.get());
}
} // namespace

static_assert(!std::is_copy_constructible_v<native_stdio>);
static_assert(std::is_nothrow_move_constructible_v<native_stdio>);
static_assert(std::is_nothrow_move_assignable_v<native_stdio>);

TEST_CASE("M1.windows.cancel_before_read", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    io_gate gate;
    auto entered = gate.entered.get_future();
    std::stop_source stop;
    auto result = std::async(std::launch::async, [&] {
        std::array<char, 1> bytes{};
        return opened->read_some(bytes, stop.get_token());
    });
    const auto status = entered.wait_for(1s);
    stop.request_stop();
    gate.released.set_value();
    const bool finished = finish_cancel(*opened, result);
    REQUIRE(status == std::future_status::ready);
    REQUIRE(finished);
    REQUIRE_FALSE(result.get());
    // Reader-token cancellation leaves the writer available for its flush budget.
    synapse::detail::windows_stdio_before_io.store(nullptr);
    REQUIRE(opened->write_some(std::string{"ok"}) == 2);
}

TEST_CASE("M1.windows.cancel_idle_read", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    io_gate gate;
    auto entered = gate.entered.get_future();
    auto result = std::async(std::launch::async, [&] {
        std::array<char, 1> bytes{};
        return opened->read_some(bytes);
    });
    const auto status = entered.wait_for(1s);
    gate.released.set_value();
    // An open, empty input cannot finish until cancellation arrives.
    const auto idle = result.wait_for(20ms);
    opened->request_stop();
    const bool finished = finish_cancel(*opened, result);
    REQUIRE(status == std::future_status::ready);
    REQUIRE(idle == std::future_status::timeout);
    REQUIRE(finished);
    REQUIRE_FALSE(result.get());
    DWORD flags = 0;
    REQUIRE(::GetHandleInformation(input.writer, &flags));
}

TEST_CASE("M1.windows.cancel_blocked_write", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    io_gate gate;
    auto entered = gate.entered.get_future();
    const std::string bytes(1024 * 1024, 'w');
    auto result = std::async(std::launch::async, [&] { return opened->write_some(bytes); });
    const auto status = entered.wait_for(1s);
    gate.released.set_value();
    const auto blocked = result.wait_for(20ms);
    opened->request_stop();
    const bool finished = finish_cancel(*opened, result);
    REQUIRE(status == std::future_status::ready);
    REQUIRE(blocked == std::future_status::timeout);
    REQUIRE(finished);
    REQUIRE_FALSE(result.get());
}

TEST_CASE("M1.windows.cancel_before_write", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    io_gate gate;
    auto entered = gate.entered.get_future();
    std::stop_source stop;
    const std::string bytes(1024 * 1024, 'w');
    auto result = std::async(std::launch::async, [&] {
        return opened->write_some(bytes, stop.get_token());
    });
    const auto status = entered.wait_for(1s);
    stop.request_stop();
    gate.released.set_value();
    const bool finished = finish_cancel(*opened, result);
    REQUIRE(status == std::future_status::ready);
    REQUIRE(finished);
    REQUIRE_FALSE(result.get());
}

TEST_CASE("M1.windows.eof_and_broken_pipe", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    ::CloseHandle(std::exchange(input.writer, nullptr));
    std::array<char, 16> bytes{};
    REQUIRE(opened->read_some(bytes) == 0);
    ::CloseHandle(std::exchange(output.reader, nullptr));
    auto result = opened->write_some(std::string{"broken"});
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == synapse::sdk_errc::io_error);
}

TEST_CASE("M1.windows.token_cancel_is_direction_local", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    const std::string bytes(1024 * 1024, 'w');
    std::promise<void> entering;
    auto started = entering.get_future();
    auto writer = std::async(std::launch::async, [&] {
        entering.set_value();
        return opened->write_some(bytes);
    });
    const auto writer_started = started.wait_for(1s);
    const auto writer_blocked = writer.wait_for(20ms);
    std::stop_source stop;
    auto reader = std::async(std::launch::async, [&] {
        std::array<char, 1> buffer{};
        return opened->read_some(buffer, stop.get_token());
    });
    stop.request_stop();
    const bool reader_done = finish_cancel(*opened, reader);
    const auto writer_still_blocked = writer.wait_for(20ms);
    opened->request_stop();
    const bool writer_done = finish_cancel(*opened, writer);
    REQUIRE(writer_started == std::future_status::ready);
    REQUIRE(writer_blocked == std::future_status::timeout);
    REQUIRE(reader_done);
    REQUIRE_FALSE(reader.get());
    REQUIRE(writer_still_blocked == std::future_status::timeout);
    REQUIRE(writer_done);
    REQUIRE_FALSE(writer.get());
}

TEST_CASE("M1.windows.handle_cleanup", "[M1][windows]") {
    private_pipe input, output;
    exercise_lifecycle(input, output); // Warm up the CRT and Win32 paths first.
    exercise_cancelled_lifecycle(input, output);
    const auto baseline = handle_count();
    for (int cycle = 0; cycle < 100; ++cycle) exercise_lifecycle(input, output);
    for (int cycle = 0; cycle < 100; ++cycle) exercise_cancelled_lifecycle(input, output);
    REQUIRE(handle_count() == baseline);
    DWORD flags = 0;
    REQUIRE(::GetHandleInformation(input.reader, &flags));
    REQUIRE(::GetHandleInformation(output.writer, &flags));
    REQUIRE_FALSE(native_stdio::open(INVALID_HANDLE_VALUE, output.writer));
    REQUIRE_FALSE(native_stdio::open(input.reader, INVALID_HANDLE_VALUE));
    REQUIRE(handle_count() == baseline);
}

TEST_CASE("M1.windows.reject_non_pipe", "[M1][windows]") {
    private_pipe input, output;
    HANDLE device = ::CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    REQUIRE(device != INVALID_HANDLE_VALUE);
    auto result = native_stdio::open(device, output.writer);
    ::CloseHandle(device);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == synapse::sdk_errc::unsupported_stdio);
}

TEST_CASE("M1.windows.pre_stopped_and_moved_from", "[M1][windows]") {
    private_pipe input, output;
    auto opened = native_stdio::open(input.reader, output.writer);
    REQUIRE(opened);
    auto io = std::move(*opened);
    std::array<char, 1> bytes{};
    REQUIRE_FALSE(opened->read_some(bytes));
    std::stop_source stop;
    stop.request_stop();
    REQUIRE_FALSE(io.read_some(bytes, stop.get_token()));
    REQUIRE_FALSE(io.write_some(bytes, stop.get_token()));
    REQUIRE(io.write_some(std::string{"x"}) == 1);
    io.request_stop();
    REQUIRE_FALSE(io.read_some(bytes));
    REQUIRE_FALSE(io.write_some(bytes));
}
