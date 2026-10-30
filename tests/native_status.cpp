#include "../examples/native_status_state.hpp"
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace {
using json = nlohmann::json;
using namespace std::chrono_literals;
struct reference_transport {
    std::function<void(const json&)> message;
    std::function<void(synapse::close_reason)> closed;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<json> output;
    std::atomic<bool> stopped{false};
    std::expected<void, synapse::sdk_error> start(decltype(message) callback, decltype(closed) close = {}) {
        message = std::move(callback); closed = std::move(close); return {};
    }
    std::expected<void, synapse::sdk_error> send(const json& wire) {
        std::lock_guard lock(mutex); output.push_back(wire); changed.notify_all(); return {};
    }
    void stop() noexcept { if (!stopped.exchange(true) && closed) closed(synapse::close_reason::requested_stop); }
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
    json request(int id, std::string method, json params = json::object()) {
        message({{"jsonrpc", "2.0"}, {"id", id}, {"method", std::move(method)}, {"params", std::move(params)}});
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, 2s, [&] { return !output.empty(); })) return nullptr;
        auto value = output.front(); output.erase(output.begin()); return value;
    }
};
}

TEST_CASE("M1.reference.real_state", "[M1][reference]") {
    std::atomic<long long> clock_ms{0};
    native_status_state state([&] {
        return std::chrono::steady_clock::time_point{} + std::chrono::milliseconds(clock_ms.load());
    });
    reference_transport transport;
    synapse::server server(transport, {"native-status", "0.3.0"});
    REQUIRE(register_native_status(server, state));
    REQUIRE(server.start());
    auto initialized = transport.request(0, "initialize", {{"protocolVersion", "2025-11-25"},
        {"capabilities", json::object()}, {"clientInfo", {{"name", "test"}, {"version", "1"}}}});
    REQUIRE(initialized.at("result").at("serverInfo").at("name") == "native-status");
    transport.message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    clock_ms = 1234;
    auto tool = transport.request(1, "tools/call", {{"name", "app.status"}, {"arguments", json::object()}});
    auto first = json::parse(tool.at("result").at("content")[0].at("text").get<std::string>());
    CHECK(first.at("uptime_ms") == 1234);
    CHECK(first.at("handled_operations") == 1);
    CHECK(first.at("counter_scope") == "app.status tool invocations");
    auto resource = transport.request(2, "resources/read", {{"uri", "app://status"}});
    auto snapshot = json::parse(resource.at("result").at("contents")[0].at("text").get<std::string>());
    CHECK(snapshot == first);
    auto prompt = transport.request(3, "prompts/get", {{"name", "explain_status"}});
    const auto& message = prompt.at("result").at("messages")[0];
    CHECK(message.at("role") == "user");
    auto text = message.at("content").at("text").get<std::string>();
    const std::string prefix = "Explain this application status: ";
    REQUIRE(text.starts_with(prefix));
    CHECK(json::parse(text.substr(prefix.size())) == first);
    clock_ms = 2234;
    tool = transport.request(4, "tools/call", {{"name", "app.status"}, {"arguments", json::object()}});
    auto second = json::parse(tool.at("result").at("content")[0].at("text").get<std::string>());
    CHECK(second.at("uptime_ms") == 2234);
    CHECK(second.at("handled_operations") == 2);
    resource = transport.request(5, "resources/read", {{"uri", "app://status"}});
    CHECK(json::parse(resource.at("result").at("contents")[0].at("text").get<std::string>()) == second);
    server.stop(); REQUIRE(server.wait());
}
