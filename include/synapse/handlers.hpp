#pragma once

#include <synapse/errors.hpp>
#include <synapse/protocol.hpp>

#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace synapse {

struct request_context {
    json_rpc_id id;
    std::stop_token stop;
};

struct tool_result {
    std::string text;
};

struct resource_result {
    std::string text;
};

enum class prompt_role {
    user,
    assistant
};

struct prompt_message {
    prompt_role role;
    std::string text;
};

struct prompt_result {
    std::vector<prompt_message> messages;
    std::optional<std::string> description;
};

using tool_handler = std::function<std::expected<tool_result, handler_error>(
    const nlohmann::json&, const request_context&)>;
using resource_handler = std::function<std::expected<resource_result, handler_error>(
    const request_context&)>;
using prompt_handler = std::function<std::expected<prompt_result, handler_error>(
    const std::map<std::string, std::string>&, const request_context&)>;

} // namespace synapse
