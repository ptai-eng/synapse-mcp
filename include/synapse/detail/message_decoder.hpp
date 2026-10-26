#pragma once

#include <synapse/protocol.hpp>

#include <expected>
#include <limits>
#include <utility>

namespace synapse::detail {

inline bool is_request_id(const nlohmann::json& value) noexcept {
    return value.is_string() || value.is_number_integer();
}

inline bool is_wire_error(const nlohmann::json& value) {
    if (!value.is_object() || !value.contains("code") ||
        !value.contains("message") || !value.at("message").is_string()) {
        return false;
    }
    const auto& code = value.at("code");
    if (code.is_number_unsigned()) {
        return code.get<uint64_t>() <= static_cast<uint64_t>((std::numeric_limits<int>::max)());
    }
    if (code.is_number_integer()) {
        const auto number = code.get<int64_t>();
        return number >= (std::numeric_limits<int>::min)() && number <= (std::numeric_limits<int>::max)();
    }
    return false;
}

// A decoding failure is a diagnostic, not permission to send a response.
// The server must suppress it for an object with a method and no ID, including
// malformed notifications. Never manufacture an ID to make it dispatchable.
inline std::expected<incoming_message, response> decode_message(const nlohmann::json& wire) noexcept {
    response failure;
    failure.id = nullptr;
    failure.err = error{error_code::invalid_request, "Invalid Request"};
    try {
        if (!wire.is_object()) {
            return std::unexpected(std::move(failure));
        }
        if (wire.contains("id") && is_request_id(wire.at("id"))) {
            failure.id = read_json_rpc_id(wire.at("id"));
        }
        if (!wire.contains("jsonrpc") || !wire.at("jsonrpc").is_string() ||
            wire.at("jsonrpc") != "2.0") {
            return std::unexpected(std::move(failure));
        }
        if (wire.contains("method")) {
            if (!wire.at("method").is_string() || wire.contains("result") || wire.contains("error") ||
                (wire.contains("params") && !wire.at("params").is_object() && !wire.at("params").is_array())) {
                return std::unexpected(std::move(failure));
            }
            if (wire.contains("id")) {
                if (!is_request_id(wire.at("id"))) {
                    return std::unexpected(std::move(failure));
                }
                return incoming_message{wire.get<request>()};
            }
            return incoming_message{wire.get<notification>()};
        }
        const bool has_result = wire.contains("result");
        const bool has_error = wire.contains("error");
        if (!wire.contains("id") || has_result == has_error ||
            (!is_request_id(wire.at("id")) && !(has_error && wire.at("id").is_null())) ||
            (has_error && !is_wire_error(wire.at("error")))) {
            return std::unexpected(std::move(failure));
        }
        return incoming_message{wire.get<response>()};
    } catch (...) {
        // JSON conversion details and arbitrary input never enter wire errors.
        return std::unexpected(std::move(failure));
    }
}

} // namespace synapse::detail
