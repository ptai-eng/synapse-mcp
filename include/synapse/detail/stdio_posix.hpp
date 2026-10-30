#pragma once

#include <synapse/errors.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <csignal>
#include <expected>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <poll.h>
#include <pthread.h>
#include <span>
#include <stop_token>
#include <unistd.h>
#include <utility>

namespace synapse::detail {

// One reader and one writer may run concurrently. Join them before moving or
// destroying this object. Duplicates share O_NONBLOCK with the caller's original
// endpoints: exclusive stdio use is required until destruction restores flags.
class posix_stdio {
    struct state {
        int input = -1;
        int output = -1;
        int input_flags = -1;
        int output_flags = -1;
        bool input_changed = false;
        bool output_changed = false;
        std::array<int, 2> read_wake{-1, -1};
        std::array<int, 2> write_wake{-1, -1};
        std::atomic<bool> stopped{false};

        ~state() {
            if (input_changed) set_flags(input, input_flags);
            if (output_changed) set_flags(output, output_flags);
            for (int fd : {input, output, read_wake[0], read_wake[1],
                           write_wake[0], write_wake[1]})
                if (fd >= 0) ::close(fd);
        }

        static int set_flags(int fd, int flags) noexcept {
            int result;
            do { result = ::fcntl(fd, F_SETFL, flags); } while (result < 0 && errno == EINTR);
            return result;
        }
    };

    std::unique_ptr<state> state_;
    explicit posix_stdio(std::unique_ptr<state> value) noexcept : state_(std::move(value)) {}

    static auto failure(const char* message) {
        return std::unexpected(sdk_error{sdk_errc::io_error, message});
    }
    static auto cancelled() {
        return std::unexpected(sdk_error{sdk_errc::invalid_state, "Native I/O stopped"});
    }
    static int duplicate(int fd) noexcept {
        int result;
        do { result = ::fcntl(fd, F_DUPFD_CLOEXEC, 0); } while (result < 0 && errno == EINTR);
        return result;
    }
    static int get_flags(int fd) noexcept {
        int result;
        do { result = ::fcntl(fd, F_GETFL); } while (result < 0 && errno == EINTR);
        return result;
    }
    static bool make_wake(std::array<int, 2>& wake) noexcept {
        int result;
        do { result = ::pipe(wake.data()); } while (result < 0 && errno == EINTR);
        if (result < 0) return false;
        for (int fd : wake) {
            do { result = ::fcntl(fd, F_SETFD, FD_CLOEXEC); }
            while (result < 0 && errno == EINTR);
            if (result < 0 || state::set_flags(fd, O_NONBLOCK) < 0) return false;
        }
        return true;
    }
    static void wake(int fd) noexcept {
        const char byte = 0;
        ssize_t result;
        do { result = ::write(fd, &byte, 1); } while (result < 0 && errno == EINTR);
        // EAGAIN means an earlier wake already makes the pipe readable.
    }
    static void drain(int fd) noexcept {
        std::array<char, 128> bytes{};
        for (;;) {
            auto count = ::read(fd, bytes.data(), bytes.size());
            if (count > 0 || (count < 0 && errno == EINTR)) continue;
            break;
        }
    }
    std::expected<void, sdk_error> wait_ready(int fd, short events,
                                             int wake_fd, std::stop_token token) {
        for (;;) {
            if (state_->stopped.load(std::memory_order_acquire) || token.stop_requested())
                return cancelled();
            pollfd descriptors[2]{{fd, events, 0}, {wake_fd, POLLIN, 0}};
            auto count = ::poll(descriptors, 2, -1);
            if (count < 0) {
                if (errno == EINTR) continue;
                return failure("Native I/O poll failed");
            }
            if (descriptors[1].revents) drain(wake_fd);
            if (state_->stopped.load(std::memory_order_acquire) || token.stop_requested())
                return cancelled();
            if (descriptors[0].revents & POLLNVAL)
                return failure("Native I/O endpoint invalid");
            // HUP/ERR must reach read/write: read drains final bytes before EOF,
            // and write reports EPIPE through the thread-local SIGPIPE guard.
            if (descriptors[0].revents & (events | POLLHUP | POLLERR)) return {};
        }
    }

    class sigpipe_guard {
        sigset_t previous_{};
        sigset_t blocked_{};
        bool active_ = false;
        bool pending_before_ = false;
    public:
        sigpipe_guard() noexcept {
            ::sigemptyset(&blocked_);
            ::sigaddset(&blocked_, SIGPIPE);
            if (::pthread_sigmask(SIG_BLOCK, &blocked_, &previous_) != 0) return;
            active_ = true;
            sigset_t pending{};
            if (::sigpending(&pending) != 0) {
                ::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
                active_ = false;
                return;
            }
            pending_before_ = ::sigismember(&pending, SIGPIPE) == 1;
        }
        bool valid() const noexcept { return active_; }
        void consume_new_signal() noexcept {
            if (pending_before_) return;
            const timespec timeout{0, 0};
            while (::sigtimedwait(&blocked_, nullptr, &timeout) < 0 && errno == EINTR) {}
        }
        ~sigpipe_guard() {
            if (active_) ::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
        }
    };

public:
    posix_stdio(const posix_stdio&) = delete;
    posix_stdio& operator=(const posix_stdio&) = delete;
    posix_stdio(posix_stdio&&) noexcept = default;
    posix_stdio& operator=(posix_stdio&&) noexcept = default;
    ~posix_stdio() = default;

    static std::expected<posix_stdio, sdk_error> open(int input, int output) {
        auto value = std::make_unique<state>();
        value->input = duplicate(input);
        if (value->input < 0) return failure("Cannot duplicate native input");
        value->output = duplicate(output);
        if (value->output < 0) return failure("Cannot duplicate native output");
        value->input_flags = get_flags(value->input);
        value->output_flags = get_flags(value->output);
        if (value->input_flags < 0 || value->output_flags < 0 ||
            !make_wake(value->read_wake) || !make_wake(value->write_wake))
            return failure("Cannot configure native I/O");
        if ((value->input_flags & O_NONBLOCK) == 0) {
            if (state::set_flags(value->input, value->input_flags | O_NONBLOCK) < 0)
                return failure("Cannot configure native input");
            value->input_changed = true;
        }
        if ((value->output_flags & O_NONBLOCK) == 0) {
            if (state::set_flags(value->output, value->output_flags | O_NONBLOCK) < 0)
                return failure("Cannot configure native output");
            value->output_changed = true;
        }
        return posix_stdio(std::move(value));
    }

    std::expected<std::size_t, sdk_error> read_some(std::span<char> buffer,
                                                  std::stop_token token = {}) {
        if (!state_) return cancelled();
        if (buffer.empty()) return 0;
        std::stop_callback notify(token, [this] { wake(state_->read_wake[1]); });
        for (;;) {
            auto ready = wait_ready(state_->input, POLLIN, state_->read_wake[0], token);
            if (!ready) return std::unexpected(ready.error());
            const auto size = (std::min)(buffer.size(),
                static_cast<std::size_t>((std::numeric_limits<ssize_t>::max)()));
            auto count = ::read(state_->input, buffer.data(), size);
            if (count >= 0) return static_cast<std::size_t>(count);
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
                return failure("Native input read failed");
        }
    }

    std::expected<std::size_t, sdk_error> write_some(std::span<const char> buffer,
                                                   std::stop_token token = {}) {
        if (!state_) return cancelled();
        if (buffer.empty()) return 0;
        std::stop_callback notify(token, [this] { wake(state_->write_wake[1]); });
        sigpipe_guard signals;
        if (!signals.valid()) return failure("Cannot protect native output signal");
        for (;;) {
            auto ready = wait_ready(state_->output, POLLOUT, state_->write_wake[0], token);
            if (!ready) return std::unexpected(ready.error());
            const auto size = (std::min)(buffer.size(),
                static_cast<std::size_t>((std::numeric_limits<ssize_t>::max)()));
            auto count = ::write(state_->output, buffer.data(), size);
            if (count >= 0) return static_cast<std::size_t>(count);
            const int error = errno;
            if (error == EPIPE) signals.consume_new_signal();
            if (error != EINTR && error != EAGAIN && error != EWOULDBLOCK)
                return failure("Native output write failed");
        }
    }

    void request_stop() noexcept {
        if (!state_) return;
        state_->stopped.store(true, std::memory_order_release);
        cancel_pending();
    }
    void cancel_pending() noexcept {
        if (!state_) return;
        wake(state_->read_wake[1]);
        wake(state_->write_wake[1]);
    }
};
} // namespace synapse::detail
