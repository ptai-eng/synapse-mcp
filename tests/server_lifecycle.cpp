#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <mutex>

namespace {
struct lifecycle_transport {
    std::function<void(const nlohmann::json&)> message;
    std::function<void(synapse::close_reason)> closed;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<nlohmann::json> output;
    std::expected<void, synapse::sdk_error> start(decltype(message) cb, decltype(closed) end = {}) {
        message = std::move(cb); closed = std::move(end); return {};
    }
    std::expected<void, synapse::sdk_error> send(const nlohmann::json& wire) {
        std::lock_guard lock(mutex); output.push_back(wire); changed.notify_all(); return {};
    }
    void stop() noexcept { if (closed) { auto cb = std::move(closed); cb(synapse::close_reason::requested_stop); } }
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
    nlohmann::json next() {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(1), [&] { return !output.empty(); })) return nullptr;
        auto result = output.front(); output.erase(output.begin()); return result;
    }
};
}

TEST_CASE("M1.lifecycle.negotiate_2025", "[M1][lifecycle]") {
    lifecycle_transport transport;
    synapse::server server(transport);
    server.start();
    transport.message({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2025-11-25"}, {"capabilities", nlohmann::json::object()},
            {"clientInfo", {{"name", "test"}, {"version", "1"}}}}}});
    const auto response = transport.next();
    REQUIRE(response.at("result").at("protocolVersion") == "2025-11-25");
    server.stop(); server.wait();
}
