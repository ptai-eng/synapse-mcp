#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
struct counting_transport {
    int starts = 0;
    std::expected<void, synapse::sdk_error> start(std::function<void(const nlohmann::json&)>,
        std::function<void(synapse::close_reason)> = {}) { ++starts; return {}; }
    std::expected<void, synapse::sdk_error> send(const nlohmann::json&) { return {}; }
    void stop() noexcept {}
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
};
}
TEST_CASE("M1.ownership.stop_before_start", "[M1][ownership]") {
    counting_transport transport;
    synapse::server server(transport);
    server.stop();
    server.start();
    CHECK(transport.starts == 0);
    server.wait();
}
