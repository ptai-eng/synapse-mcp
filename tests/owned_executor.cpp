#include <catch2/catch_test_macros.hpp>

#include <synapse/detail/owned_executor.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace executor_allocation_fault {
// Only the enqueue caller is affected. A failure disarms itself so cleanup and
// reentry retain normal allocation behavior, including in the exception path.
thread_local int remaining = -1;

struct fail_after {
    explicit fail_after(int count) { remaining = count; }
    ~fail_after() { remaining = -1; }
};
} // namespace executor_allocation_fault

void* operator new(std::size_t size) {
    auto& remaining = executor_allocation_fault::remaining;
    if (remaining == 0) {
        remaining = -1;
        throw std::bad_alloc();
    }
    if (remaining > 0) --remaining;
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace std::chrono_literals;
using synapse::detail::owned_executor;

struct gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool open = false;

    void wait() {
        std::unique_lock lock(mutex);
        changed.wait(lock, [this] { return open; });
    }
    void release() {
        std::lock_guard lock(mutex);
        open = true;
        changed.notify_all();
    }
};

// Declared after the executor: an assertion failure releases callbacks before join.
struct release_on_exit {
    gate& value;
    ~release_on_exit() { value.release(); }
};

synapse::json_rpc_id number(std::int64_t value) { return value; }

struct counted_lifetime {
    std::atomic<int>& destroyed;
    ~counted_lifetime() { ++destroyed; }
};

// The capture destructor must observe reentry completing before it returns.
// Delegating reentry to a prestarted thread makes the broken implementation fail
// within a bounded interval: returning releases its lock, and the probe then joins.
// No detached thread or stranded executor remains on a regression/assertion.
struct destructor_reentry_probe {
    owned_executor& executor;
    std::mutex mutex;
    std::condition_variable changed;
    bool requested = false;
    bool completed = false;
    bool completed_during_destruction = false;
    std::atomic<int> destroyed{0};
    std::jthread reentry;

    explicit destructor_reentry_probe(owned_executor& owner)
        : executor(owner), reentry([this] {
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [this] { return requested; });
            }
            executor.cancel(number(999));
            executor.stop();
            {
                std::lock_guard lock(mutex);
                completed = true;
            }
            changed.notify_all();
        }) {}

    void request() {
        {
            std::lock_guard lock(mutex);
            requested = true;
        }
        changed.notify_all();
    }

    void on_destroy() {
        ++destroyed;
        request();
        std::unique_lock lock(mutex);
        completed_during_destruction = changed.wait_for(lock, 1s, [this] { return completed; });
    }

    void join() {
        if (reentry.joinable()) reentry.join();
    }

    ~destructor_reentry_probe() {
        request();
        join();
    }
};

struct reenter_on_destroy {
    destructor_reentry_probe& probe;
    ~reenter_on_destroy() { probe.on_destroy(); }
};
} // namespace

TEST_CASE("M1.executor.allocation_failure_releases_captures_outside_lock", "[M1][executor]") {
    int allocation_failures = 0;
    bool reached_success = false;
    // Enumerate allocation positions instead of depending on a library's node or
    // stop-state sizes. Exercise work ownership, stop-state and list allocations.
    for (int position = 0; position != 16 && !reached_success; ++position) {
        owned_executor executor;
        destructor_reentry_probe probe(executor);
        auto lifetime = std::make_shared<reenter_on_destroy>(probe);
        owned_executor::invocation call = [lifetime](const synapse::request_context&) {};
        lifetime.reset();
        bool admitted = false;
        bool allocation_failed = false;
        {
            executor_allocation_fault::fail_after fault(position);
            try {
                admitted = executor.enqueue(number(1), std::move(call)).has_value();
            } catch (const std::bad_alloc&) {
                allocation_failed = true;
            }
        }
        executor.stop();
        const auto joined = executor.wait();
        probe.join();
        INFO("enqueue allocation position: " << position);
        REQUIRE(joined);
        REQUIRE(probe.destroyed == 1);
        REQUIRE(probe.completed_during_destruction);
        if (allocation_failed) {
            ++allocation_failures;
        } else {
            REQUIRE(admitted);
            reached_success = true;
        }
    }
    REQUIRE(reached_success);
    REQUIRE(allocation_failures >= 3);
}

TEST_CASE("M1.executor.rejected_admission_releases_captures_outside_lock", "[M1][executor]") {
    enum class rejection { closed, active_duplicate, pending_duplicate, full };
    for (const auto reason : {rejection::closed, rejection::active_duplicate,
                              rejection::pending_duplicate, rejection::full}) {
        gate blocked;
        std::promise<void> started;
        auto future = started.get_future();
        owned_executor executor(1);
        destructor_reentry_probe probe(executor);
        release_on_exit cleanup{blocked};
        if (reason == rejection::closed) {
            executor.stop();
        } else {
            REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context&) {
                started.set_value();
                blocked.wait();
            }));
            REQUIRE(future.wait_for(2s) == std::future_status::ready);
            REQUIRE(executor.enqueue(number(2), [](const synapse::request_context&) {}));
        }
        const auto id = reason == rejection::active_duplicate ? 1 :
                        reason == rejection::pending_duplicate ? 2 : 3;
        auto lifetime = std::make_shared<reenter_on_destroy>(probe);
        owned_executor::invocation call = [lifetime](const synapse::request_context&) {};
        lifetime.reset();
        const auto result = executor.enqueue(number(id), std::move(call));
        executor.stop();
        blocked.release();
        const auto joined = executor.wait();
        probe.join();
        REQUIRE(joined);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().code == (reason == rejection::full ?
            synapse::sdk_errc::resource_exhausted : synapse::sdk_errc::invalid_state));
        REQUIRE(probe.destroyed == 1);
        REQUIRE(probe.completed_during_destruction);
    }
}

TEST_CASE("M1.executor.serial_and_bounded", "[M1][executor]") {
    gate blocked;
    std::atomic<int> active{0};
    std::atomic<int> maximum{0};
    std::vector<int> order;
    std::mutex order_mutex;
    std::promise<void> started;
    std::promise<void> drained;
    auto started_future = started.get_future();
    auto drained_future = drained.get_future();
    auto record = [&](int index) {
        const auto count = ++active;
        maximum.store((std::max)(maximum.load(), count));
        {
            std::lock_guard lock(order_mutex);
            order.push_back(index);
        }
        --active;
    };
    owned_executor executor;
    release_on_exit cleanup{blocked};
    REQUIRE(executor.enqueue(number(0), [&](const synapse::request_context&) {
        const auto count = ++active;
        maximum.store((std::max)(maximum.load(), count));
        started.set_value();
        blocked.wait();
        {
            std::lock_guard lock(order_mutex);
            order.push_back(0);
        }
        --active;
    }));
    REQUIRE(started_future.wait_for(2s) == std::future_status::ready);
    for (int index = 1; index <= 16; ++index) {
        REQUIRE(executor.enqueue(number(index), [&, index](const synapse::request_context&) {
            record(index);
            if (index == 16) drained.set_value();
        }));
    }
    const auto full = executor.enqueue(number(17), [](const synapse::request_context&) {});
    REQUIRE_FALSE(full);
    REQUIRE(full.error().code == synapse::sdk_errc::resource_exhausted);
    blocked.release();
    REQUIRE(drained_future.wait_for(2s) == std::future_status::ready);
    executor.stop();
    REQUIRE(executor.wait());
    REQUIRE(maximum == 1);
    REQUIRE(order.size() == 17);
    for (int index = 0; index <= 16; ++index) REQUIRE(order[index] == index);
}

TEST_CASE("M1.executor.stop_owns_active_work", "[M1][executor]") {
    gate blocked;
    std::atomic<int> completed{0};
    std::atomic<int> destroyed{0};
    std::promise<std::stop_token> started;
    auto started_future = started.get_future();
    auto lifetime = std::make_shared<counted_lifetime>(destroyed);
    owned_executor executor;
    release_on_exit cleanup{blocked};
    REQUIRE(executor.enqueue(number(1), [&, lifetime](const synapse::request_context& context) {
        started.set_value(context.stop);
        blocked.wait();
        ++completed;
    }));
    lifetime.reset();
    REQUIRE(started_future.wait_for(2s) == std::future_status::ready);
    const auto token = started_future.get();
    executor.stop();
    REQUIRE(token.stop_requested());
    const auto pending = executor.wait_for(50ms);
    REQUIRE(pending);
    REQUIRE_FALSE(*pending);
    REQUIRE(completed == 0);
    REQUIRE(destroyed == 0);
    blocked.release();
    REQUIRE(executor.wait());
    REQUIRE(completed == 1);
    REQUIRE(destroyed == 1);
    REQUIRE(executor.wait());
    REQUIRE(executor.wait_for(0ms).value());
    REQUIRE_FALSE(executor.enqueue(number(2), [](const synapse::request_context&) {}));
}

TEST_CASE("M1.executor.cancel_preserves_active_id", "[M1][executor]") {
    gate blocked;
    std::atomic<int> queued_calls{0};
    std::promise<std::stop_token> started;
    auto started_future = started.get_future();
    owned_executor executor(1);
    release_on_exit cleanup{blocked};
    REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context& context) {
        started.set_value(context.stop);
        blocked.wait();
    }));
    REQUIRE(started_future.wait_for(2s) == std::future_status::ready);
    const auto token = started_future.get();
    REQUIRE(executor.enqueue(number(2), [&](const synapse::request_context&) { ++queued_calls; }));
    executor.cancel(number(2));
    REQUIRE(executor.enqueue(number(3), [&](const synapse::request_context&) { ++queued_calls; }));
    executor.cancel(number(3));
    executor.cancel(number(1));
    REQUIRE(token.stop_requested());
    const auto duplicate = executor.enqueue(number(1), [](const synapse::request_context&) {});
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.error().code == synapse::sdk_errc::invalid_state);
    executor.cancel(number(999));
    blocked.release();
    executor.stop();
    REQUIRE(executor.wait());
    REQUIRE(queued_calls == 0);
}

TEST_CASE("M1.executor.typed_ids", "[M1][executor]") {
    gate blocked;
    std::promise<void> started;
    auto started_future = started.get_future();
    std::promise<void> drained;
    auto drained_future = drained.get_future();
    std::atomic<int> number_calls{0};
    std::atomic<int> string_calls{0};
    std::atomic<int> unsigned_calls{0};
    owned_executor executor;
    release_on_exit cleanup{blocked};
    REQUIRE(executor.enqueue(number(0), [&](const synapse::request_context&) {
        started.set_value();
        blocked.wait();
    }));
    REQUIRE(started_future.wait_for(2s) == std::future_status::ready);
    REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context&) { ++number_calls; }));
    const auto duplicate = executor.enqueue(std::uint64_t{1}, [](const synapse::request_context&) {});
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.error().code == synapse::sdk_errc::invalid_state);
    REQUIRE(executor.enqueue(std::string("1"), [&](const synapse::request_context& context) {
        if (std::get<std::string>(context.id) == "1") ++string_calls;
    }));
    executor.cancel(std::uint64_t{1});
    REQUIRE(executor.enqueue((std::numeric_limits<std::uint64_t>::max)(),
        [&](const synapse::request_context& context) {
            if (std::get<std::uint64_t>(context.id) == (std::numeric_limits<std::uint64_t>::max)()) {
                ++unsigned_calls;
            }
        }));
    REQUIRE(executor.enqueue(number(-1), [&](const synapse::request_context&) { drained.set_value(); }));
    blocked.release();
    REQUIRE(drained_future.wait_for(2s) == std::future_status::ready);
    executor.stop();
    REQUIRE(executor.wait());
    REQUIRE(number_calls == 0);
    REQUIRE(string_calls == 1);
    REQUIRE(unsigned_calls == 1);
    REQUIRE(synapse::detail::same_request_id(number(0), std::uint64_t{0}));
    REQUIRE_FALSE(synapse::detail::same_request_id(number(-1), (std::numeric_limits<std::uint64_t>::max)()));
}

TEST_CASE("M1.executor.worker_wait_rejected", "[M1][executor]") {
    std::promise<bool> result;
    auto future = result.get_future();
    owned_executor executor;
    REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context&) {
        const auto wait = executor.wait();
        const auto timed = executor.wait_for(1ms);
        result.set_value(!wait && wait.error().code == synapse::sdk_errc::invalid_state &&
                         !timed && timed.error().code == synapse::sdk_errc::invalid_state);
        executor.stop();
    }));
    REQUIRE(future.wait_for(2s) == std::future_status::ready);
    REQUIRE(future.get());
    REQUIRE(executor.wait());
}

TEST_CASE("M1.executor.exceptions_and_repeated_lifecycles", "[M1][executor]") {
    for (int iteration = 0; iteration < 30; ++iteration) {
        std::promise<void> survived;
        auto future = survived.get_future();
        owned_executor executor;
        REQUIRE(executor.enqueue(number(1), [](const synapse::request_context&) {
            throw std::runtime_error("private application failure");
        }));
        REQUIRE(executor.enqueue(number(2), [&](const synapse::request_context&) {
            survived.set_value();
        }));
        REQUIRE(future.wait_for(2s) == std::future_status::ready);
        executor.stop();
        REQUIRE(executor.wait());
    }
}

TEST_CASE("M1.executor.stop_discards_pending_and_rejects_bad_admission", "[M1][executor]") {
    gate blocked;
    std::promise<void> started;
    auto future = started.get_future();
    std::atomic<int> invoked{0};
    owned_executor executor;
    release_on_exit cleanup{blocked};
    REQUIRE_FALSE(executor.enqueue(nullptr, [](const synapse::request_context&) {}));
    REQUIRE_FALSE(executor.enqueue(number(0), {}));
    REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context&) {
        started.set_value();
        blocked.wait();
    }));
    REQUIRE(future.wait_for(2s) == std::future_status::ready);
    REQUIRE(executor.enqueue(number(2), [&](const synapse::request_context&) { ++invoked; }));
    executor.stop();
    executor.stop();
    blocked.release();
    REQUIRE(executor.wait());
    REQUIRE(invoked == 0);
    for (int iteration = 0; iteration < 30; ++iteration) {
        owned_executor idle;
        idle.stop();
        REQUIRE(idle.wait());
    }
    owned_executor invalid(0);
    const auto admission = invalid.enqueue(number(1), [](const synapse::request_context&) {});
    REQUIRE_FALSE(admission);
    REQUIRE(admission.error().code == synapse::sdk_errc::invalid_state);
    REQUIRE(invalid.wait());
}

TEST_CASE("M1.executor.stop_callbacks_may_reenter", "[M1][executor]") {
    gate blocked;
    std::promise<void> started;
    auto future = started.get_future();
    std::atomic<bool> reentered{false};
    owned_executor executor;
    release_on_exit cleanup{blocked};
    REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context& context) {
        std::stop_callback on_stop(context.stop, [&] {
            executor.cancel(number(999));
            executor.stop();
            const auto admission = executor.enqueue(number(3), [](const synapse::request_context&) {});
            reentered = !admission && admission.error().code == synapse::sdk_errc::invalid_state;
        });
        started.set_value();
        blocked.wait();
    }));
    REQUIRE(future.wait_for(2s) == std::future_status::ready);
    executor.cancel(std::uint64_t{1});
    REQUIRE(reentered);
    blocked.release();
    REQUIRE(executor.wait());
}

TEST_CASE("M1.executor.destructor_joins", "[M1][executor]") {
    gate blocked;
    std::promise<void> started;
    auto future = started.get_future();
    std::promise<void> stopping;
    auto stopping_future = stopping.get_future();
    std::atomic<int> completed{0};
    std::jthread release_after_stop([&] {
        // A finite failure path prevents a fixture failure from stranding a join.
        (void)stopping_future.wait_for(2s);
        blocked.release();
    });
    {
        owned_executor executor;
        REQUIRE(executor.enqueue(number(1), [&](const synapse::request_context& context) {
            std::stop_callback on_stop(context.stop, [&] { stopping.set_value(); });
            started.set_value();
            blocked.wait();
            ++completed;
        }));
        REQUIRE(future.wait_for(2s) == std::future_status::ready);
    }
    REQUIRE(completed == 1);
}
