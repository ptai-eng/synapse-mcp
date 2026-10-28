#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;
using json = nlohmann::json;

namespace {
struct policy_transport {
    std::function<void(const json&)> message;
    std::function<void(synapse::close_reason)> closed;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<json> output;
    std::atomic<bool> stopped{false};
    std::size_t frame_capacity = 1'048'576;
    std::size_t max_encoded_frame_bytes() const noexcept { return frame_capacity; }
    std::expected<void, synapse::sdk_error> start(decltype(message) callback, decltype(closed) close = {}) {
        message = std::move(callback); closed = std::move(close); return {};
    }
    std::expected<void, synapse::sdk_error> send(const json& wire) {
        std::lock_guard lock(mutex); output.push_back(wire); changed.notify_all(); return {};
    }
    void stop() noexcept {
        if (!stopped.exchange(true) && closed) closed(synapse::close_reason::requested_stop);
        changed.notify_all();
    }
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
    json next() {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, 2s, [&] { return !output.empty(); })) return nullptr;
        auto result = output.front(); output.erase(output.begin()); return result;
    }
    json request(int id, const char* method, json params = json::object()) {
        message({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}});
        return next();
    }
    void ready() {
        auto result = request(0, "initialize", {{"protocolVersion", "2025-11-25"},
            {"capabilities", json::object()}, {"clientInfo", {{"name", "test"}, {"version", "1"}}}});
        REQUIRE(result.contains("result"));
        message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    }
};
}

TEST_CASE("M1.policy.access_rate_output", "[M1][policy]") {
    policy_transport transport;
    std::atomic<int> calls{0}, hooks{0};
    std::atomic<bool> allowed{false}, wrong_thread{false};
    std::atomic<long long> clock_ms{0};
    const auto owner = std::this_thread::get_id();
    synapse::server_options options;
    options.now = [&] { return std::chrono::steady_clock::time_point{} + std::chrono::milliseconds(clock_ms.load()); };
    options.allow_tool = [&](std::string_view, const synapse::request_context&) {
        ++hooks; if (std::this_thread::get_id() == owner) wrong_thread = true;
        return allowed.load();
    };
    options.filter_output = [&](std::string_view, std::string_view text, const synapse::request_context&)
        -> std::expected<std::string, synapse::handler_error> {
        ++hooks; if (std::this_thread::get_id() == owner) wrong_thread = true;
        return text == "fixture-secret" ? std::string("[redacted]") : std::string(text);
    };
    synapse::server server(transport, {}, options);
    REQUIRE(server.register_tool({"echo", "", {{"type", "object"}}},
        [&](const json&, const synapse::request_context&) -> std::expected<synapse::tool_result, synapse::handler_error> {
            ++calls; if (std::this_thread::get_id() == owner) wrong_thread = true;
            return synapse::tool_result{"fixture-secret"};
        }));
    REQUIRE(server.start()); transport.ready();
    const json params = {{"name", "echo"}, {"arguments", json::object()}};
    auto denied = transport.request(1, "tools/call", params);
    REQUIRE(denied.at("error").at("code") == -32050);
    CHECK(denied.at("error").at("data").at("reason") == "access_denied");
    CHECK(calls == 0);
    allowed = true;
    for (int i = 0; i < 16; ++i) {
        auto result = transport.request(i + 2, "tools/call", params);
        REQUIRE(result.at("result").at("content")[0].at("text") == "[redacted]");
    }
    auto limited = transport.request(18, "tools/call", params);
    REQUIRE(limited.at("error").at("code") == -32051);
    CHECK(limited.at("error").at("data").at("reason") == "rate_limited");
    CHECK(calls == 16);
    CHECK(transport.request(19, "ping").contains("result"));
    clock_ms = 63;
    CHECK(transport.request(20, "tools/call", params).contains("result"));
    CHECK(calls == 17);
    CHECK(hooks > 0);
    CHECK_FALSE(wrong_thread);
    server.stop(); REQUIRE(server.wait());
}

TEST_CASE("M1.policy.schema_quota_and_unsafe_output", "[M1][policy]") {
    policy_transport transport;
    std::atomic<int> calls{0}, mode{0};
    synapse::server_options options;
    options.tool_burst = 4;
    options.tool_requests_per_second = 1;
    options.now = [] { return std::chrono::steady_clock::time_point{}; };
    synapse::server server(transport, {}, options);
    REQUIRE(server.register_tool({"checked", "", {{"type", "object"},
            {"properties", {{"n", {{"type", "integer"}}}}}, {"required", {"n"}}}},
        [&](const json&, const synapse::request_context&) -> std::expected<synapse::tool_result, synapse::handler_error> {
            ++calls;
            switch (mode.load()) {
                case 1: return synapse::tool_result{std::string("private\x1b[0m")};
                case 2: return synapse::tool_result{std::string("private\xff", 8)};
                case 3: return synapse::tool_result{std::string(600'000, '\t')};
                default: return synapse::tool_result{"ok"};
            }
        }));
    REQUIRE(server.start()); transport.ready();
    const auto call = [&](int id, json arguments) {
        return transport.request(id, "tools/call", {{"name", "checked"}, {"arguments", std::move(arguments)}});
    };
    auto invalid = call(1, {{"n", "private-value"}});
    REQUIRE(invalid.at("result").at("isError") == true);
    CHECK(invalid.dump().find("private-value") == std::string::npos);
    CHECK(calls == 0);
    mode = 1;
    auto unsafe = call(2, {{"n", 1}});
    REQUIRE(unsafe.at("result").at("isError") == true);
    CHECK(unsafe.dump().find("private") == std::string::npos);
    CHECK(calls == 1);
    mode = 2;
    auto invalid_utf8 = call(3, {{"n", 1}});
    REQUIRE(invalid_utf8.at("result").at("isError") == true);
    CHECK(invalid_utf8.dump().find("private") == std::string::npos);
    mode = 3;
    auto too_large = call(4, {{"n", 1}});
    REQUIRE(too_large.at("result").at("isError") == true);
    auto limited = call(5, {{"n", 1}});
    REQUIRE(limited.at("error").at("code") == -32051);
    CHECK(calls == 3);
    server.stop(); REQUIRE(server.wait());
}

TEST_CASE("M1.policy.control_while_policy_blocked", "[M1][policy]") {
    policy_transport transport;
    std::promise<void> entered;
    std::mutex mutex;
    std::condition_variable changed;
    bool released = false;
    synapse::server_options options;
    options.allow_tool = [&](std::string_view, const synapse::request_context&) {
        entered.set_value();
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return released; });
        return true;
    };
    synapse::server server(transport, {}, options);
    struct release_on_exit {
        std::mutex& mutex; std::condition_variable& changed; bool& released;
        ~release_on_exit() { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
    } cleanup{mutex, changed, released};
    REQUIRE(server.register_tool({"echo", "", {{"type", "object"}}},
        [](const json&, const synapse::request_context&) -> std::expected<synapse::tool_result, synapse::handler_error> {
            return synapse::tool_result{"ok"};
        }));
    REQUIRE(server.start()); transport.ready();
    transport.message({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"},
        {"params", {{"name", "echo"}, {"arguments", json::object()}}}});
    REQUIRE(entered.get_future().wait_for(1s) == std::future_status::ready);
    auto ping = transport.request(2, "ping");
    REQUIRE(ping.at("id") == 2);
    REQUIRE(ping.contains("result"));
    { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
    REQUIRE(transport.next().at("id") == 1);
    server.stop(); REQUIRE(server.wait());
}

TEST_CASE("M1.policy.respects_transport_frame_capacity", "[M1][policy]") {
    policy_transport transport;
    transport.frame_capacity = 256;
    synapse::server server(transport);
    REQUIRE(server.register_tool({"echo", "", {{"type", "object"}}},
        [](const json&, const synapse::request_context&) -> std::expected<synapse::tool_result, synapse::handler_error> {
            return synapse::tool_result{std::string(160, '\t')};
        }));
    REQUIRE(server.start()); transport.ready();
    const auto rejected = transport.request(1, "tools/call", {{"name", "echo"}});
    REQUIRE(rejected.at("result").at("isError") == true);
    CHECK(rejected.dump().size() <= transport.frame_capacity);
    CHECK(transport.request(2, "ping").contains("result"));
    server.stop(); REQUIRE(server.wait());
}

TEST_CASE("M1.policy.oversized_fallback_closes", "[M1][policy]") {
    policy_transport transport;
    transport.frame_capacity = 256;
    synapse::server server(transport);
    REQUIRE(server.register_tool({"echo", "", {{"type", "object"}}},
        [](const json&, const synapse::request_context&) -> std::expected<synapse::tool_result, synapse::handler_error> {
            return synapse::tool_result{std::string(600, 'x')};
        }));
    REQUIRE(server.start()); transport.ready();
    const json request = {{"jsonrpc", "2.0"}, {"id", std::string(180, 'i')},
        {"method", "tools/call"}, {"params", {{"name", "echo"}}}};
    REQUIRE(request.dump().size() <= transport.frame_capacity);
    transport.message(request);
    {
        std::unique_lock lock(transport.mutex);
        REQUIRE(transport.changed.wait_for(lock, 2s, [&] { return transport.stopped.load(); }));
        CHECK(transport.output.empty());
    }
    REQUIRE(server.wait());
}
