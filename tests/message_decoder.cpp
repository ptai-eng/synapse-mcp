#include <catch2/catch_test_macros.hpp>
#include <synapse/protocol.hpp>
#include <synapse/detail/message_decoder.hpp>

#include <cstdint>
#include <limits>
#include <string>

namespace {
using nlohmann::json;
}

TEST_CASE("M1.protocol.roundtrip_ids", "[M1][protocol]") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const auto& id : {json("request"), json(std::int64_t{-17}),
                           json(std::int64_t{23}), json(maximum)}) {
        CAPTURE(id);
        const json request_wire{{"jsonrpc", "2.0"}, {"id", id}, {"method", "ping"}};
        const auto request = request_wire.get<synapse::request>();
        REQUIRE(json(request).at("id") == id);
        REQUIRE(json(request).at("id").dump() == id.dump());
        const json response_wire{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
        const auto response = response_wire.get<synapse::response>();
        REQUIRE(json(response).at("id") == id);
        REQUIRE(json(response).at("id").dump() == id.dump());
    }
}

TEST_CASE("M1.protocol.reject_invalid_envelopes", "[M1][protocol]") {
    const json valid{{"jsonrpc", "2.0"}, {"id", "safe"}, {"method", "ping"}};
    const json good_error{{"code", -32603}, {"message", "Internal error"}};
    const std::vector<json> invalid{
        nullptr, false, 42, "secret text", json::array(), json::array({valid}),
        json::object(), json{{"id", 1}, {"method", "ping"}},
        json{{"jsonrpc", "1.0"}, {"id", 1}, {"method", "ping"}},
        json{{"jsonrpc", 2}, {"id", 1}, {"method", "ping"}},
        json{{"jsonrpc", "2.0"}, {"id", 1}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", nullptr}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", 42}},
        json{{"jsonrpc", "2.0"}, {"id", nullptr}, {"method", "ping"}},
        json{{"jsonrpc", "2.0"}, {"id", true}, {"method", "ping"}},
        json{{"jsonrpc", "2.0"}, {"id", 1.0}, {"method", "ping"}},
        json{{"jsonrpc", "2.0"}, {"id", json::array()}, {"method", "ping"}},
        json{{"jsonrpc", "2.0"}, {"id", json::object()}, {"method", "ping"}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"params", nullptr}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"params", "secret params"}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"params", true}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"params", 2}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"result", 42}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"error", good_error}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"result", 42}, {"error", good_error}},
        json{{"jsonrpc", "2.0"}, {"result", 42}},
        json{{"jsonrpc", "2.0"}, {"id", false}, {"result", 42}},
        json{{"jsonrpc", "2.0"}, {"id", 1.0}, {"result", 42}},
        json{{"jsonrpc", "2.0"}, {"id", nullptr}, {"result", 42}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", "secret error"}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"message", "secret"}}}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32603}}}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", 1.0}, {"message", "secret"}}}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", true}, {"message", "secret"}}}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32603}, {"message", 42}}}},
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", std::numeric_limits<std::uint64_t>::max()}, {"message", "secret"}}}}
    };
    for (const auto& wire : invalid) {
        CAPTURE(wire.dump());
        const auto decoded = synapse::detail::decode_message(wire);
        REQUIRE_FALSE(decoded);
        const auto& failure = decoded.error();
        REQUIRE(failure.jsonrpc == "2.0");
        REQUIRE_FALSE(failure.result);
        REQUIRE(failure.err);
        REQUIRE(failure.err->code == -32600);
        REQUIRE(failure.err->message == "Invalid Request");
        REQUIRE_FALSE(failure.err->data);
        const auto failure_id = json(failure).at("id");
        if (wire.is_object() && wire.contains("id") &&
            (wire["id"].is_string() || wire["id"].is_number_integer())) {
            REQUIRE(failure_id.dump() == wire["id"].dump());
        } else {
            REQUIRE(failure_id.is_null());
        }
    }
}

TEST_CASE("M1.protocol.decode_valid_messages", "[M1][protocol]") {
    for (const auto& id : {json("abc"), json(std::int64_t{-17}),
                           json(std::int64_t{23}), json(std::numeric_limits<std::uint64_t>::max())}) {
        CAPTURE(id.dump());
        const json request_wire{{"jsonrpc", "2.0"}, {"id", id}, {"method", "ping"}, {"params", json::object()}};
        const auto request = synapse::detail::decode_message(request_wire);
        REQUIRE(request);
        REQUIRE(std::holds_alternative<synapse::request>(*request));
        REQUIRE(json(std::get<synapse::request>(*request)).dump() == request_wire.dump());
        const json response_wire{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
        const auto response = synapse::detail::decode_message(response_wire);
        REQUIRE(response);
        REQUIRE(std::holds_alternative<synapse::response>(*response));
        REQUIRE(json(std::get<synapse::response>(*response)).dump() == response_wire.dump());
    }
    const auto parsed = synapse::detail::decode_message(json::parse(
        R"({"jsonrpc":"2.0","id":18446744073709551615,"method":"ping"})"));
    REQUIRE(parsed);
    REQUIRE(std::get<std::uint64_t>(std::get<synapse::request>(*parsed).id) ==
            std::numeric_limits<std::uint64_t>::max());
    for (const auto& params : {json::object(), json::array({1, "value"})}) {
        const auto notification = synapse::detail::decode_message(
            json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}, {"params", params}});
        REQUIRE(notification);
        REQUIRE(std::holds_alternative<synapse::notification>(*notification));
    }
    const auto notification = synapse::detail::decode_message(
        json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    REQUIRE(notification);
    REQUIRE_FALSE(std::get<synapse::notification>(*notification).params);
    const auto error_response = synapse::detail::decode_message(json{{"jsonrpc", "2.0"}, {"id", nullptr},
        {"error", {{"code", -32700}, {"message", "Parse error"}, {"data", json::object()}}}});
    REQUIRE(error_response);
    REQUIRE(std::get<synapse::response>(*error_response).err->data);
}

TEST_CASE("M1.protocol.malformed_notifications_have_no_id", "[M1][protocol]") {
    for (const auto& wire : {
        json{{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", 42}},
        json{{"jsonrpc", "1.0"}, {"method", "notifications/initialized"}},
        json{{"jsonrpc", "2.0"}, {"method", false}},
        json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}, {"result", 42}}}) {
        const auto decoded = synapse::detail::decode_message(wire);
        REQUIRE_FALSE(decoded);
        REQUIRE(std::holds_alternative<std::nullptr_t>(decoded.error().id));
    }
    const auto large_id_error = synapse::detail::decode_message(json{{"jsonrpc", "wrong"},
        {"id", std::numeric_limits<std::uint64_t>::max()}, {"method", "ping"}});
    REQUIRE_FALSE(large_id_error);
    REQUIRE(json(large_id_error.error()).at("id").dump() == "18446744073709551615");
}

TEST_CASE("M1.protocol.optional_fields_reset", "[M1][protocol]") {
    SECTION("request and notification params") {
        synapse::request request;
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}, {"params", json::object()}}.get_to(request);
        json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "ping"}}.get_to(request);
        REQUIRE_FALSE(request.params);
        synapse::notification notification;
        json{{"jsonrpc", "2.0"}, {"method", "ready"}, {"params", json::object()}}.get_to(notification);
        json{{"jsonrpc", "2.0"}, {"method", "ready"}}.get_to(notification);
        REQUIRE_FALSE(notification.params);
    }
    SECTION("response result and error") {
        synapse::response response;
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"result", 42}}.get_to(response);
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32603}, {"message", "Internal error"}}}}.get_to(response);
        REQUIRE_FALSE(response.result);
        REQUIRE(response.err);
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"result", nullptr}}.get_to(response);
        REQUIRE_FALSE(response.err);
        REQUIRE(response.result);
    }
    SECTION("error data") {
        synapse::error error;
        json{{"code", -32603}, {"message", "error"}, {"data", "old"}}.get_to(error);
        json{{"code", -32603}, {"message", "error"}}.get_to(error);
        REQUIRE_FALSE(error.data);
    }
    SECTION("MCP descriptions and metadata") {
        synapse::tool tool{"old", "old description", json::object()};
        json{{"name", "new"}, {"inputSchema", json::object()}}.get_to(tool);
        REQUIRE(tool.description.empty());
        synapse::resource resource{"old", "old", "old description", "text/plain"};
        json{{"uri", "app://new"}, {"name", "new"}}.get_to(resource);
        REQUIRE(resource.description.empty());
        REQUIRE_FALSE(resource.mimeType);
        synapse::prompt_argument argument{"old", "old description", true};
        json{{"name", "new"}}.get_to(argument);
        REQUIRE(argument.description.empty());
        REQUIRE_FALSE(argument.required);
        synapse::prompt prompt{"old", "old description", {argument}};
        json{{"name", "new"}}.get_to(prompt);
        REQUIRE(prompt.description.empty());
        REQUIRE(prompt.arguments.empty());
    }
}

TEST_CASE("M1.protocol.raw_codec_compatibility", "[M1][protocol]") {
    const auto request = json{{"jsonrpc", "2.0"}, {"id", nullptr}, {"method", "legacy"}}
                             .get<synapse::request>();
    REQUIRE(std::holds_alternative<std::nullptr_t>(request.id));
    REQUIRE(json(request).at("id").is_null());
}
