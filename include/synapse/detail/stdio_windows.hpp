#pragma once

#include <synapse/errors.hpp>

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <utility>

namespace synapse::detail {

#ifdef SYNAPSE_WINDOWS_STDIO_TEST_HOOKS
struct windows_stdio_test_hook {
    void (*invoke)(void*, bool);
    void* context;
};
inline std::atomic<windows_stdio_test_hook*> windows_stdio_before_io{nullptr};
#endif

// Pipe-backed synchronous I/O only; one reader and one writer may run together.
// The owner joins both operations before moving/destroying this object. Stop
// requests do not wait. While joining, repeatedly call cancel_pending(): a
// one-shot CancelSynchronousIo can precede entry into ReadFile/WriteFile.
class windows_stdio {
    struct operation {
        HANDLE thread = nullptr;
        bool cancelled = false;
    };

    struct state {
        HANDLE input = nullptr;
        HANDLE output = nullptr;
        std::mutex mutex;
        std::condition_variable completed;
        operation reader;
        operation writer;
        bool stopped = false;

        void cancel_locked() noexcept {
            for (auto* pending : {&reader, &writer})
                if (pending->thread && (stopped || pending->cancelled))
                    ::CancelSynchronousIo(pending->thread);
        }

        ~state() {
            std::unique_lock lock(mutex);
            stopped = true;
            while (reader.thread || writer.thread) {
                cancel_locked();
                completed.wait_for(lock, std::chrono::milliseconds(1));
            }
            lock.unlock();
            if (input) ::CloseHandle(input);
            if (output) ::CloseHandle(output);
        }
    };

    // Completion and cancellation serialize handle access: a cancel never sees
    // a closed/reused thread handle, even as read and write finish concurrently.
    struct complete_operation {
        state& owner;
        operation& pending;
        ~complete_operation() {
            std::lock_guard lock(owner.mutex);
            ::CloseHandle(pending.thread);
            pending.thread = nullptr;
            pending.cancelled = false;
            owner.completed.notify_all();
        }
    };

    std::unique_ptr<state> state_;
    explicit windows_stdio(std::unique_ptr<state> value) noexcept : state_(std::move(value)) {}

    static auto failure(const char* message) {
        return std::unexpected(sdk_error{sdk_errc::io_error, message});
    }
    static auto cancelled() {
        return std::unexpected(sdk_error{sdk_errc::invalid_state, "Native I/O stopped"});
    }

    template<bool Writing, class Buffer>
    std::expected<std::size_t, sdk_error> transfer(Buffer buffer, std::stop_token token) {
        auto* owner = state_.get();
        if (!owner) return cancelled();
        if (buffer.empty()) return 0;
        auto& pending = Writing ? owner->writer : owner->reader;
        {
            std::lock_guard lock(owner->mutex);
            if (owner->stopped || token.stop_requested()) return cancelled();
            if (pending.thread)
                return std::unexpected(sdk_error{sdk_errc::invalid_state,
                    "Native I/O direction already active"});
            // GetCurrentThread is a pseudo-handle. Duplicate it into an owned
            // Win32 handle; std::thread::native_handle is deliberately unused.
            if (!::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                    ::GetCurrentProcess(), &pending.thread, 0, FALSE, DUPLICATE_SAME_ACCESS))
                return failure("Cannot duplicate native I/O thread");
        }
        complete_operation complete{*owner, pending};
        std::stop_callback notify(token, [owner, &pending] {
            std::lock_guard lock(owner->mutex);
            pending.cancelled = true;
            if (pending.thread) ::CancelSynchronousIo(pending.thread);
        });
        {
            std::lock_guard lock(owner->mutex);
            if (owner->stopped || pending.cancelled || token.stop_requested()) return cancelled();
        }
#ifdef SYNAPSE_WINDOWS_STDIO_TEST_HOOKS
        if (auto* hook = windows_stdio_before_io.load()) hook->invoke(hook->context, Writing);
#endif
        const auto size = static_cast<DWORD>((std::min)(buffer.size(),
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD count = 0;
        BOOL success;
        if constexpr (Writing) {
            success = ::WriteFile(owner->output, buffer.data(), size, &count, nullptr);
        } else {
            success = ::ReadFile(owner->input, buffer.data(),
                size, &count, nullptr);
        }
        const auto error = success ? ERROR_SUCCESS : ::GetLastError();
        if (success) return static_cast<std::size_t>(count);
        if (error == ERROR_OPERATION_ABORTED) return cancelled();
        if (!Writing && (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED)) return 0;
        if (!Writing && error == ERROR_MORE_DATA && count) return static_cast<std::size_t>(count);
        return failure(Writing ? "Native output write failed" : "Native input read failed");
    }

public:
    windows_stdio(const windows_stdio&) = delete;
    windows_stdio& operator=(const windows_stdio&) = delete;
    windows_stdio(windows_stdio&&) noexcept = default;
    windows_stdio& operator=(windows_stdio&&) noexcept = default;
    ~windows_stdio() = default;

    static std::expected<windows_stdio, sdk_error> open(HANDLE input, HANDLE output) {
        if (::GetFileType(input) != FILE_TYPE_PIPE || ::GetFileType(output) != FILE_TYPE_PIPE)
            return std::unexpected(sdk_error{sdk_errc::unsupported_stdio,
                "Native standard streams must be pipes"});
        auto value = std::make_unique<state>();
        if (!::DuplicateHandle(::GetCurrentProcess(), input, ::GetCurrentProcess(),
                &value->input, 0, FALSE, DUPLICATE_SAME_ACCESS))
            return failure("Cannot duplicate native input");
        if (!::DuplicateHandle(::GetCurrentProcess(), output, ::GetCurrentProcess(),
                &value->output, 0, FALSE, DUPLICATE_SAME_ACCESS))
            return failure("Cannot duplicate native output");
        return windows_stdio(std::move(value));
    }

    std::expected<std::size_t, sdk_error> read_some(std::span<char> buffer,
                                                  std::stop_token token = {}) {
        return transfer<false>(buffer, token);
    }
    std::expected<std::size_t, sdk_error> write_some(std::span<const char> buffer,
                                                   std::stop_token token = {}) {
        return transfer<true>(buffer, token);
    }

    void request_stop() noexcept {
        if (!state_) return;
        std::lock_guard lock(state_->mutex);
        state_->stopped = true;
        state_->cancel_locked();
    }
    // Retry only stopped operations. A canceled reader token must leave the
    // independent writer running so the transport can honor its flush budget.
    void cancel_pending() noexcept {
        if (!state_) return;
        std::lock_guard lock(state_->mutex);
        state_->cancel_locked();
    }
};
} // namespace synapse::detail
