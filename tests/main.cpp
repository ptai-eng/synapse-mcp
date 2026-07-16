#include <catch2/catch_test_macros.hpp>
#include <synapse/protocol.hpp>
#include <nlohmann/json.hpp>

TEST_CASE("Request Serialization", "[protocol]") {
    synapse::request req;
    req.id = 1;
    req.method = "initialize";
    req.params = nlohmann::json{{"protocolVersion", "2024-11-05"}};

    nlohmann::json j = req;
    REQUIRE(j["jsonrpc"] == "2.0");
    REQUIRE(j["method"] == "initialize");
    REQUIRE(j["id"] == 1);
    REQUIRE(j["params"]["protocolVersion"] == "2024-11-05");

    auto req2 = j.get<synapse::request>();
    REQUIRE(req2.jsonrpc == "2.0");
    REQUIRE(req2.method == "initialize");
    REQUIRE(std::get<int64_t>(req2.id) == 1);
}

TEST_CASE("Response Serialization", "[protocol]") {
    synapse::response res;
    res.id = "abc";
    res.result = nlohmann::json{{"status", "ok"}};

    nlohmann::json j = res;
    REQUIRE(j["jsonrpc"] == "2.0");
    REQUIRE(j["id"] == "abc");
    REQUIRE(j["result"]["status"] == "ok");
    REQUIRE(!j.contains("error"));

    auto res2 = j.get<synapse::response>();
    REQUIRE(std::get<std::string>(res2.id) == "abc");
    REQUIRE(res2.result.has_value());
    REQUIRE(!res2.err.has_value());
}

TEST_CASE("Error Serialization", "[protocol]") {
    synapse::error err(synapse::error_code::method_not_found, "Method not found");
    synapse::response res;
    res.id = nullptr;
    res.err = err;

    nlohmann::json j = res;
    REQUIRE(j["jsonrpc"] == "2.0");
    REQUIRE(j["id"] == nullptr);
    REQUIRE(j["error"]["code"] == -32601);
    REQUIRE(j["error"]["message"] == "Method not found");
}
