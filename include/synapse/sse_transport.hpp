#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include "protocol.hpp"

namespace synapse {

class sse_transport {
public:
    using write_callback = std::function<void(const std::string&)>;
    using message_callback = std::function<void(const nlohmann::json&)>;

    explicit sse_transport(write_callback writer) : writer_(std::move(writer)) {}
    
    // Process incoming client HTTP POST requests
    void handle_client_message(const std::string& payload) {
        try {
            auto j = nlohmann::json::parse(payload);
            if (message_callback_) {
                message_callback_(j);
            }
        } catch (const nlohmann::json::parse_error& e) {
            // Send standard JSON-RPC 2.0 parse error
            error err(error_code::parse_error, "Parse error");
            response res;
            res.err = err;
            res.id = nullptr;
            nlohmann::json j_res = res;
            send(j_res);
        }
    }
    
    void send(const nlohmann::json& message) {
        std::string sse_payload = "data: " + message.dump() + "\n\n";
        if (writer_) {
            writer_(sse_payload);
        }
    }

    void start(message_callback on_message) {
        message_callback_ = std::move(on_message);
    }

    // sse_transport doesn't have background threads, so stop and wait are no-ops
    void stop() {}
    void wait() {}

private:
    write_callback writer_;
    message_callback message_callback_;
};

} // namespace synapse
