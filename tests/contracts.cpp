#include <catch2/catch_test_macros.hpp>

#include <synapse/errors.hpp>
#include <synapse/handlers.hpp>
#include <synapse/server_options.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

using tool_handler_signature = std::function<std::expected<synapse::tool_result, synapse::handler_error>(
    const nlohmann::json&, const synapse::request_context&)>;
using resource_handler_signature = std::function<std::expected<synapse::resource_result, synapse::handler_error>(
    const synapse::request_context&)>;
using prompt_handler_signature = std::function<std::expected<synapse::prompt_result, synapse::handler_error>(
    const std::map<std::string, std::string>&, const synapse::request_context&)>;
using allow_tool_signature = std::function<bool(std::string_view, const synapse::request_context&)>;
using filter_output_signature = std::function<std::expected<std::string, synapse::handler_error>(
    std::string_view, std::string_view, const synapse::request_context&)>;

static_assert(std::is_same_v<decltype(synapse::request_context::id), synapse::json_rpc_id>);
static_assert(std::is_same_v<decltype(synapse::request_context::stop), std::stop_token>);
static_assert(std::is_same_v<synapse::tool_handler, tool_handler_signature>);
static_assert(std::is_same_v<synapse::resource_handler, resource_handler_signature>);
static_assert(std::is_same_v<synapse::prompt_handler, prompt_handler_signature>);
static_assert(std::is_same_v<decltype(synapse::server_options::allow_tool), allow_tool_signature>);
static_assert(std::is_same_v<decltype(synapse::server_options::filter_output), filter_output_signature>);
static_assert(std::is_same_v<decltype(synapse::server_options::now),
                             std::function<std::chrono::steady_clock::time_point()>>);
static_assert(std::is_same_v<decltype(synapse::tool_result::text), std::string>);
static_assert(std::is_same_v<decltype(synapse::resource_result::text), std::string>);
static_assert(std::is_same_v<decltype(synapse::prompt_message::role), synapse::prompt_role>);
static_assert(std::is_same_v<decltype(synapse::prompt_message::text), std::string>);
static_assert(std::is_same_v<decltype(synapse::prompt_result::messages), std::vector<synapse::prompt_message>>);
static_assert(std::is_same_v<decltype(synapse::prompt_result::description), std::optional<std::string>>);
static_assert(std::is_same_v<decltype(synapse::sdk_error::code), synapse::sdk_errc>);
static_assert(std::is_same_v<decltype(synapse::sdk_error::message), std::string>);
static_assert(std::is_same_v<decltype(synapse::handler_error::public_message), std::string>);

} // namespace

TEST_CASE("M1.contract.defaults", "[M1][contract]") {
    const synapse::stdio_options stdio;
    REQUIRE(stdio.max_frame_bytes == 1'048'576);
    REQUIRE(stdio.max_json_depth == 64);
    REQUIRE(stdio.max_outbound_bytes == 4'194'304);
    REQUIRE(stdio.flush_budget == 250ms);

    const synapse::server_options server;
    REQUIRE(server.max_pending_requests == 16);
    REQUIRE(server.tool_burst == 16);
    REQUIRE(server.tool_requests_per_second == 16);
    REQUIRE_FALSE(server.allow_tool);
    REQUIRE_FALSE(server.filter_output);
    REQUIRE(static_cast<bool>(server.now));
}

TEST_CASE("M1.contract.error_domains", "[M1][contract]") {
    const synapse::sdk_error local{synapse::sdk_errc::io_error, "local diagnostic"};
    const synapse::handler_error application{"public failure"};
    const synapse::error wire{synapse::error_code::internal_error, "wire failure"};
    REQUIRE(local.message == "local diagnostic");
    REQUIRE(application.public_message == "public failure");
    REQUIRE(wire.code == -32603);
}
