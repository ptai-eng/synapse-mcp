#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>
#include <limits>

namespace {
struct lifecycle_transport {
    std::function<void(const nlohmann::json&)> message;
    std::function<void(synapse::close_reason)> closed;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<nlohmann::json> output;
    bool stopped = false;
    std::expected<void, synapse::sdk_error> start(decltype(message) cb, decltype(closed) end = {}) {
        message = std::move(cb); closed = std::move(end); return {};
    }
    std::expected<void, synapse::sdk_error> send(const nlohmann::json& wire) {
        std::lock_guard lock(mutex); output.push_back(wire); changed.notify_all(); return {};
    }
    void stop() noexcept { if (!stopped) { stopped = true; if (closed) closed(synapse::close_reason::requested_stop); } }
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
    nlohmann::json next() {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(1), [&] { return !output.empty(); })) return nullptr;
        auto result = output.front(); output.erase(output.begin()); return result;
    }
    std::size_t count() { std::lock_guard lock(mutex); return output.size(); }
};
using namespace std::chrono_literals;
using json = nlohmann::json;
json call(json id, std::string method, json params = json::object()) {
    return {{"jsonrpc", "2.0"}, {"id", std::move(id)}, {"method", std::move(method)}, {"params", std::move(params)}};
}
json initialize_params(std::string version = "2025-11-25") {
    return {{"protocolVersion", std::move(version)}, {"capabilities", json::object()}, {"clientInfo", {{"name", "test"}, {"version", "1"}}}};
}
void ready(lifecycle_transport& t) {
    t.message(call(0, "initialize", initialize_params()));
    REQUIRE(t.next().at("result").at("protocolVersion") == "2025-11-25");
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
}
struct gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool released = false;
    void wait() { std::unique_lock lock(mutex); changed.wait(lock, [&] { return released; }); }
    void release() { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
};
struct release_gate { gate& value; ~release_gate() { value.release(); } };
synapse::resource descriptor() { return {"app://test", "Test", "Test resource", "text/plain"}; }
}

TEST_CASE("M1.lifecycle.negotiate_2025", "[M1][lifecycle]") {
    lifecycle_transport transport;
    synapse::server server(transport);
    REQUIRE(server.start());
    transport.message({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2025-11-25"}, {"capabilities", nlohmann::json::object()},
            {"clientInfo", {{"name", "test"}, {"version", "1"}}}}}});
    const auto response = transport.next();
    REQUIRE(response.at("result").at("protocolVersion") == "2025-11-25");
    server.stop(); REQUIRE(server.wait());
}

TEST_CASE("M1.lifecycle.counteroffer_and_order", "[M1][lifecycle]") {
    for (const auto* version : {"2024-11-05", "2099-01-01", "unrecognized", ""}) {
        lifecycle_transport t;
        std::atomic<int> calls{0};
        synapse::server s(t);
        REQUIRE(s.register_resource(descriptor(), [&](const auto&) -> std::expected<synapse::resource_result, synapse::handler_error> { ++calls; return synapse::resource_result{"ok"}; }));
        REQUIRE(s.start());
        t.message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        t.message(call(1, "resources/read", {{"uri", "app://test"}}));
        CHECK(t.next().at("error").at("code") == -32600);
        t.message(call(2, "initialize", initialize_params(version)));
        auto response = t.next();
        CHECK(response.at("result").at("protocolVersion") == "2025-11-25");
        CHECK_FALSE(response.at("result").at("capabilities").contains("tools"));
        t.message(call(3, "resources/read", {{"uri", "app://test"}}));
        CHECK(t.next().contains("error"));
        CHECK(calls == 0);
        t.message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        t.message(call(4, "resources/read", {{"uri", "app://test"}}));
        CHECK(t.next().at("result").at("contents")[0].at("text") == "ok");
        t.message(call(5, "initialize", initialize_params()));
        CHECK(t.next().at("error").at("code") == -32600);
        s.stop(); REQUIRE(s.wait());
    }
}

TEST_CASE("M1.lifecycle.invalid_input_and_notifications", "[M1][lifecycle]") {
    lifecycle_transport t;
    synapse::server s(t);
    REQUIRE(s.start());
    for (auto params : {json::object(), json::array(), initialize_params()}) {
        if (params.contains("clientInfo")) params["clientInfo"]["version"] = 3;
        t.message(call(1, "initialize", params));
        CHECK(t.next().at("error").at("code") == -32602);
    }
    t.message({{"method", 7}});
    t.message({{"jsonrpc", "2.0"}, {"method", "resources/read"}, {"params", nullptr}});
    t.message({{"jsonrpc", "2.0"}, {"id", 72}, {"result", "unsolicited"}});
    CHECK(t.count() == 0);
    t.message(json::array());
    CHECK(t.next().at("id").is_null());
    t.message(call(2, "ping"));
    CHECK(t.next().at("result") == json::object());
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", 0}}}});
    ready(t);
    t.message({{"jsonrpc", "2.0"}, {"method", "resources/read"}, {"params", {{"uri", "app://test"}}}});
    CHECK(t.count() == 0);
    s.stop(); REQUIRE(s.wait());
}

TEST_CASE("M1.lifecycle.cancellation_and_ping", "[M1][lifecycle]") {
    lifecycle_transport t;
    gate blocked;
    std::promise<void> entered, cancelled;
    std::atomic<int> calls{0};
    synapse::server s(t);
    release_gate release{blocked};
    REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& ctx) -> std::expected<synapse::resource_result, synapse::handler_error> {
        ++calls;
        std::stop_callback on_stop(ctx.stop, [&] { cancelled.set_value(); });
        entered.set_value(); blocked.wait(); return synapse::resource_result{"late"};
    }));
    REQUIRE(s.start()); ready(t);
    t.message(call("active", "resources/read", {{"uri", "app://test"}}));
    REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
    t.message(call("queued", "resources/read", {{"uri", "app://test"}}));
    auto cancel = [&](json id) { t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", id}}}}); };
    cancel("queued"); cancel("active");
    REQUIRE(cancelled.get_future().wait_for(1s) == std::future_status::ready);
    cancel("unknown"); cancel(0); cancel(nullptr);
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", "unknown"}, {"reason", 42}}}});
    CHECK(t.count() == 0);
    t.message(call(9, "ping"));
    CHECK(t.next().at("id") == 9);
    CHECK(calls == 1);
    s.stop(); CHECK_FALSE(s.wait_for(5ms).value());
    blocked.release(); REQUIRE(s.wait_for(1s).value());
    CHECK(t.count() == 0);
}

TEST_CASE("M1.lifecycle.duplicate_inflight_closes", "[M1][lifecycle]") {
    for (const auto* method : {"resources/read", "ping", "initialize"}) {
        lifecycle_transport t;
        gate blocked;
        std::promise<void> entered;
        std::atomic<int> calls{0};
        synapse::server s(t);
        release_gate release{blocked};
        REQUIRE(s.register_resource(descriptor(), [&](const auto&) -> std::expected<synapse::resource_result, synapse::handler_error> {
            ++calls; entered.set_value(); blocked.wait(); return synapse::resource_result{"late"};
        }));
        REQUIRE(s.start()); ready(t);
        t.message(call(1, "resources/read", {{"uri", "app://test"}}));
        REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
        t.message(call(1u, method, {{"uri", "app://test"}}));
        const auto diagnostic = t.next();
        CHECK(diagnostic.at("id").is_null());
        CHECK(diagnostic.at("error").at("code") == -32600);
        CHECK(t.stopped);
        CHECK(calls == 1);
        CHECK_FALSE(s.wait_for(1ms).value());
        blocked.release(); REQUIRE(s.wait_for(1s).value());
        CHECK(t.count() == 0);
    }
}

TEST_CASE("M1.lifecycle.typed_ids_and_sanitized_results", "[M1][lifecycle]") {
    lifecycle_transport t;
    synapse::server_options options;
    std::thread::id handler_thread, policy_thread;
    options.filter_output = [&](auto, auto text, const auto&) -> std::expected<std::string, synapse::handler_error> { policy_thread = std::this_thread::get_id(); return std::string(text); };
    synapse::server s(t, {}, options);
    REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& ctx) -> std::expected<synapse::resource_result, synapse::handler_error> {
        handler_thread = std::this_thread::get_id();
        if (std::holds_alternative<std::string>(ctx.id) && std::get<std::string>(ctx.id) == "throw") throw std::runtime_error("secret");
        return synapse::resource_result{"ok"};
    }));
    REQUIRE(s.start()); ready(t);
    for (const auto& id : {json("1"), json(1), json((std::numeric_limits<std::uint64_t>::max)())}) {
        t.message(call(id, "resources/read", {{"uri", "app://test"}}));
        CHECK(t.next().at("id") == id);
    }
    CHECK(handler_thread == policy_thread);
    CHECK(handler_thread != std::this_thread::get_id());
    t.message(call("throw", "resources/read", {{"uri", "app://test"}}));
    const auto error = t.next();
    CHECK(error.at("error").at("code") == -32603);
    CHECK(error.dump().find("secret") == std::string::npos);
    t.message(call(10, "resources/read", {{"uri", "missing"}}));
    CHECK(t.next().at("error").at("code") == -32002);
    t.message(call(11, "resources/list"));
    CHECK(t.next().at("result").at("resources").size() == 1);
    s.stop(); REQUIRE(s.wait());
}

TEST_CASE("M1.lifecycle.queue_limit_and_typed_cancel", "[M1][lifecycle]") {
    lifecycle_transport t;
    gate blocked;
    std::promise<void> entered;
    std::atomic<int> calls{0};
    std::atomic<bool> cancelled{false};
    synapse::server_options options; options.max_pending_requests = 1;
    synapse::server s(t, {}, options);
    release_gate release{blocked};
    REQUIRE(s.register_resource(descriptor(), [&](const synapse::request_context& context) -> std::expected<synapse::resource_result, synapse::handler_error> {
        ++calls;
        std::stop_callback observe(context.stop, [&] { cancelled = true; });
        entered.set_value(); blocked.wait(); return synapse::resource_result{"late"};
    }));
    REQUIRE(s.start()); ready(t);
    t.message(call(1, "resources/read", {{"uri", "app://test"}}));
    REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
    t.message(call("1", "ping"));
    CHECK(t.next().at("id") == "1");
    t.message(call((std::numeric_limits<std::uint64_t>::max)(), "ping"));
    CHECK(t.next().at("id").get<std::uint64_t>() == (std::numeric_limits<std::uint64_t>::max)());
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", "1"}}}});
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", 1}, {"reason", 42}}}});
    CHECK_FALSE(cancelled);
    t.message(call(2, "resources/read", {{"uri", "app://test"}}));
    t.message(call(3, "resources/read", {{"uri", "app://test"}}));
    auto full = t.next();
    CHECK(full.at("id") == 3);
    CHECK(full.at("error").at("code") == -32052);
    CHECK(full.at("error").at("data").at("reason") == "queue_full");
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", 2}}}});
    // Cancellation releases queued capture/ID ownership immediately.
    t.message(call(2, "ping")); CHECK(t.next().at("id") == 2);
    t.message({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", {{"requestId", 1}}}});
    CHECK(cancelled);
    // The canceled active callback still owns its ID until it returns.
    t.message(call(1, "ping")); CHECK(t.next().at("id").is_null());
    CHECK(calls == 1);
    blocked.release(); REQUIRE(s.wait_for(1s).value());
    CHECK(t.count() == 0);
}
