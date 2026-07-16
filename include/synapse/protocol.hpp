#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace synapse {

// JSON-RPC 2.0 Error Codes
enum class error_code : int {
    parse_error = -32700,
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_params = -32602,
    internal_error = -32603,
    // Server errors are -32000 to -32099
};

// JSON-RPC 2.0 Error Object
struct error {
    int code;
    std::string message;
    std::optional<nlohmann::json> data;

    error(int c, std::string msg, std::optional<nlohmann::json> d = std::nullopt)
        : code(c), message(std::move(msg)), data(std::move(d)) {}

    error(error_code c, std::string msg, std::optional<nlohmann::json> d = std::nullopt)
        : code(static_cast<int>(c)), message(std::move(msg)), data(std::move(d)) {}
};

inline void to_json(nlohmann::json& j, const error& e) {
    j = nlohmann::json{{"code", e.code}, {"message", e.message}};
    if (e.data.has_value()) {
        j["data"] = *e.data;
    }
}

inline void from_json(const nlohmann::json& j, error& e) {
    j.at("code").get_to(e.code);
    j.at("message").get_to(e.message);
    if (j.contains("data")) {
        e.data = j.at("data");
    }
}

// JSON-RPC Request/Response IDs can be string, integer, or null
using json_rpc_id = std::variant<std::string, int64_t, std::nullptr_t>;

// Base JSON-RPC Message
struct message {
    std::string jsonrpc = "2.0";
};

// JSON-RPC 2.0 Request Object
struct request : message {
    json_rpc_id id;
    std::string method;
    std::optional<nlohmann::json> params;
};

inline void to_json(nlohmann::json& j, const request& r) {
    j = nlohmann::json{{"jsonrpc", r.jsonrpc}, {"method", r.method}};
    std::visit([&j](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            j["id"] = nullptr;
        } else {
            j["id"] = arg;
        }
    }, r.id);
    if (r.params.has_value()) {
        j["params"] = *r.params;
    }
}

inline void from_json(const nlohmann::json& j, request& r) {
    j.at("jsonrpc").get_to(r.jsonrpc);
    j.at("method").get_to(r.method);
    if (j.at("id").is_number_integer()) {
        r.id = j.at("id").get<int64_t>();
    } else if (j.at("id").is_string()) {
        r.id = j.at("id").get<std::string>();
    } else {
        r.id = nullptr;
    }
    if (j.contains("params")) {
        r.params = j.at("params");
    }
}

// JSON-RPC 2.0 Notification Object
struct notification : message {
    std::string method;
    std::optional<nlohmann::json> params;
};

inline void to_json(nlohmann::json& j, const notification& n) {
    j = nlohmann::json{{"jsonrpc", n.jsonrpc}, {"method", n.method}};
    if (n.params.has_value()) {
        j["params"] = *n.params;
    }
}

inline void from_json(const nlohmann::json& j, notification& n) {
    j.at("jsonrpc").get_to(n.jsonrpc);
    j.at("method").get_to(n.method);
    if (j.contains("params")) {
        n.params = j.at("params");
    }
}

// JSON-RPC 2.0 Response Object
struct response : message {
    json_rpc_id id;
    std::optional<nlohmann::json> result;
    std::optional<error> err;
};

inline void to_json(nlohmann::json& j, const response& r) {
    j = nlohmann::json{{"jsonrpc", r.jsonrpc}};
    std::visit([&j](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            j["id"] = nullptr;
        } else {
            j["id"] = arg;
        }
    }, r.id);

    if (r.err.has_value()) {
        j["error"] = *r.err;
    } else if (r.result.has_value()) {
        j["result"] = *r.result;
    } else {
        // According to JSON-RPC 2.0, either result or error must be included.
        // A result of null is valid if it's the intended output.
        j["result"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, response& r) {
    j.at("jsonrpc").get_to(r.jsonrpc);
    if (j.at("id").is_number_integer()) {
        r.id = j.at("id").get<int64_t>();
    } else if (j.at("id").is_string()) {
        r.id = j.at("id").get<std::string>();
    } else {
        r.id = nullptr;
    }
    if (j.contains("error")) {
        r.err = j.at("error").get<error>();
    } else if (j.contains("result")) {
        r.result = j.at("result");
    }
}

// MCP Specific Types (Common)
using schema = nlohmann::json;

struct tool {
    std::string name;
    std::string description;
    schema inputSchema;
};

inline void to_json(nlohmann::json& j, const tool& t) {
    j = nlohmann::json{{"name", t.name}, {"description", t.description}, {"inputSchema", t.inputSchema}};
}

inline void from_json(const nlohmann::json& j, tool& t) {
    j.at("name").get_to(t.name);
    j.at("description").get_to(t.description);
    j.at("inputSchema").get_to(t.inputSchema);
}

struct resource {
    std::string uri;
    std::string name;
    std::string description;
    std::optional<std::string> mimeType;
};

inline void to_json(nlohmann::json& j, const resource& r) {
    j = nlohmann::json{{"uri", r.uri}, {"name", r.name}, {"description", r.description}};
    if (r.mimeType.has_value()) {
        j["mimeType"] = *r.mimeType;
    }
}

inline void from_json(const nlohmann::json& j, resource& r) {
    j.at("uri").get_to(r.uri);
    j.at("name").get_to(r.name);
    j.at("description").get_to(r.description);
    if (j.contains("mimeType")) {
        r.mimeType = j.at("mimeType").get<std::string>();
    }
}

} // namespace synapse
