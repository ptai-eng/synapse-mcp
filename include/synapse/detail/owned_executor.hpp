#pragma once

#include <synapse/handlers.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

namespace synapse::detail {

// Wire identity: strings differ from numbers; signed and unsigned representations
// of the same integer compare equal, without narrowing uint64 values. Null is not
// a valid request ID and never compares equal. Shared with server admission.
inline bool same_request_id(const json_rpc_id& left, const json_rpc_id& right) noexcept {
    return std::visit([](const auto& a, const auto& b) noexcept {
        using A = std::decay_t<decltype(a)>;
        using B = std::decay_t<decltype(b)>;
        if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
            return std::cmp_equal(a, b);
        } else if constexpr (std::is_same_v<A, std::string> && std::is_same_v<B, std::string>) {
            return a == b;
        } else {
            return false;
        }
    }, left, right);
}

// Construction starts the sole worker; there is no separate start operation.
// stop() closes admission permanently, discards queued invocations and requests
// active cancellation. wait() joins after stop; wait_for() never stops work.
// A zero pending limit creates an inert executor and enqueue reports invalid_state.
// The owner must outlive callbacks, and must not destroy this object on its worker.
// Only one application owner should join at a time. Arbitrary callback exceptions
// are contained here; invocation adapters own sanitized protocol error reporting.
class owned_executor {
public:
    using invocation = std::function<void(const request_context&)>;

    explicit owned_executor(std::size_t max_pending = 16)
        : max_pending_(max_pending), stopping_(max_pending == 0), finished_(max_pending == 0) {
        if (max_pending_ != 0) worker_ = std::thread([this] { run(); });
    }

    owned_executor(const owned_executor&) = delete;
    owned_executor& operator=(const owned_executor&) = delete;
    owned_executor(owned_executor&&) = delete;
    owned_executor& operator=(owned_executor&&) = delete;

    ~owned_executor() {
        stop();
        // Destruction from the worker violates the owner-lifetime contract; the
        // joinable std::thread enforces it rather than detaching live state.
        (void)wait();
    }

    std::expected<void, sdk_error> enqueue(json_rpc_id id, invocation call) {
        if (std::holds_alternative<std::nullptr_t>(id) || !call) {
            return std::unexpected(sdk_error{sdk_errc::invalid_state, "Invalid executor invocation"});
        }
        // Both work construction (including its stop state) and list-node
        // allocation can throw. Their invocation owners must outlive the lock
        // scope so capture destructors can re-enter even on failure/rejection.
        std::list<std::shared_ptr<work>> prepared;
        prepared.push_back(std::make_shared<work>(std::move(id), std::move(call)));
        const auto& prepared_id = prepared.front()->context.id;
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return std::unexpected(sdk_error{sdk_errc::invalid_state, "Executor admission is closed"});
            }
            if (active_ && same_request_id(active_->context.id, prepared_id)) {
                return std::unexpected(sdk_error{sdk_errc::invalid_state, "Duplicate in-flight request ID"});
            }
            for (const auto& item : pending_) {
                if (same_request_id(item->context.id, prepared_id)) {
                    return std::unexpected(sdk_error{sdk_errc::invalid_state, "Duplicate in-flight request ID"});
                }
            }
            if (pending_.size() >= max_pending_) {
                return std::unexpected(sdk_error{sdk_errc::resource_exhausted, "Executor queue is full"});
            }
            // Equal default allocators make this transfer allocation-free.
            pending_.splice(pending_.end(), prepared);
        }
        changed_.notify_one();
        return {};
    }

    void cancel(const json_rpc_id& id) noexcept {
        std::optional<std::stop_source> active_stop;
        std::list<std::shared_ptr<work>> discarded;
        {
            std::lock_guard lock(mutex_);
            if (active_ && same_request_id(active_->context.id, id)) {
                active_stop = active_->stop;
            } else {
                for (auto entry = pending_.begin(); entry != pending_.end(); ++entry) {
                    if (same_request_id((*entry)->context.id, id)) {
                        discarded.splice(discarded.end(), pending_, entry);
                        break;
                    }
                }
            }
        }
        // Stop callbacks and captured-state destructors may re-enter the executor.
        // Never invoke either while holding its state mutex.
        if (active_stop) active_stop->request_stop();
    }

    void stop() noexcept {
        std::optional<std::stop_source> active_stop;
        std::list<std::shared_ptr<work>> discarded;
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
            discarded.swap(pending_);
            if (active_) active_stop = active_->stop;
        }
        changed_.notify_all();
        if (active_stop) active_stop->request_stop();
    }

    std::expected<void, sdk_error> wait() {
        {
            std::unique_lock lock(mutex_);
            if (on_worker()) return self_wait_error();
            changed_.wait(lock, [this] { return finished_; });
        }
        return join();
    }

    std::expected<bool, sdk_error> wait_for(std::chrono::milliseconds duration) {
        {
            std::unique_lock lock(mutex_);
            if (on_worker()) return std::unexpected(self_wait_error().error());
            if (!changed_.wait_for(lock, duration, [this] { return finished_; })) return false;
        }
        if (auto result = join(); !result) return std::unexpected(result.error());
        return true;
    }

private:
    struct work {
        std::stop_source stop;
        request_context context;
        invocation call;

        work(json_rpc_id id, invocation value)
            : context{std::move(id), stop.get_token()}, call(std::move(value)) {}
    };

    bool on_worker() const noexcept {
        return !finished_ && worker_id_ == std::this_thread::get_id();
    }

    static std::expected<void, sdk_error> self_wait_error() {
        return std::unexpected(sdk_error{sdk_errc::invalid_state, "Worker cannot wait for itself"});
    }

    std::expected<void, sdk_error> join() {
        std::lock_guard lock(join_mutex_);
        try {
            if (worker_.joinable()) worker_.join();
        } catch (const std::system_error&) {
            return std::unexpected(sdk_error{sdk_errc::invalid_state, "Executor join failed"});
        }
        return {};
    }

    void run() noexcept {
        {
            std::lock_guard lock(mutex_);
            worker_id_ = std::this_thread::get_id();
        }
        for (;;) {
            std::shared_ptr<work> current;
            {
                std::unique_lock lock(mutex_);
                changed_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
                if (stopping_) break;
                current = std::move(pending_.front());
                pending_.pop_front();
                active_ = current;
            }
            try {
                current->call(current->context);
            } catch (...) {
                // User exceptions cannot unwind out of SDK-owned work.
            }
            {
                std::lock_guard lock(mutex_);
                active_.reset();
            }
            current.reset();
        }
        {
            std::lock_guard lock(mutex_);
            finished_ = true;
        }
        changed_.notify_all();
    }

    const std::size_t max_pending_;
    std::mutex mutex_;
    std::mutex join_mutex_;
    std::condition_variable changed_;
    std::list<std::shared_ptr<work>> pending_;
    std::shared_ptr<work> active_;
    bool stopping_;
    bool finished_;
    std::thread::id worker_id_;
    std::thread worker_;
};

} // namespace synapse::detail
