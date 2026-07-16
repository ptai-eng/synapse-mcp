#pragma once

#include <string>
#include <map>
#include <functional>
#include <expected>
#include <future>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>
#include "protocol.hpp"

namespace synapse {

class client {
public:
    using message_callback = std::function<void(const nlohmann::json&)>;
    using send_callback = std::function<void(const nlohmann::json&)>;

    template <typename Transport>
    explicit client(Transport& transport) : transport_send_([&transport](const nlohmann::json& msg) { transport.send(msg); }) {
        transport.start([this](const nlohmann::json& msg) {
            handle_message(msg);
        });
    }

    explicit client(send_callback sender) : transport_send_(std::move(sender)) {}

    void on_message(const nlohmann::json& msg) {
        handle_message(msg);
    }

    std::future<std::expected<nlohmann::json, error>> send_request(const std::string& method, const std::optional<nlohmann::json>& params = std::nullopt) {
        request req;
        req.id = static_cast<int64_t>(next_id_++);
        req.method = method;
        req.params = params;

        std::promise<std::expected<nlohmann::json, error>> promise;
        auto future = promise.get_future();

        {
            std::lock_guard<std::mutex> lock(promises_mutex_);
            promises_.emplace(std::get<int64_t>(req.id), std::move(promise));
        }

        transport_send_(req);
        return future;
    }

    void send_notification(const std::string& method, const std::optional<nlohmann::json>& params = std::nullopt) {
        notification notif;
        notif.method = method;
        notif.params = params;
        transport_send_(notif);
    }

    std::future<std::expected<nlohmann::json, error>> initialize() {
        return send_request("initialize", nlohmann::json{
            {"protocolVersion", "2024-11-05"},
            {"capabilities", nlohmann::json::object()},
            {"clientInfo", {
                {"name", "synapse-mcp-client"},
                {"version", "0.1.0"}
            }}
        });
    }

    std::future<std::expected<nlohmann::json, error>> call_tool(const std::string& name, const nlohmann::json& arguments = nlohmann::json::object()) {
        return send_request("tools/call", nlohmann::json{
            {"name", name},
            {"arguments", arguments}
        });
    }

    std::future<std::expected<nlohmann::json, error>> list_tools() {
        return send_request("tools/list");
    }

    std::future<std::expected<nlohmann::json, error>> list_resources() {
        return send_request("resources/list");
    }

    std::future<std::expected<nlohmann::json, error>> read_resource(const std::string& uri) {
        return send_request("resources/read", nlohmann::json{
            {"uri", uri}
        });
    }

private:
    send_callback transport_send_;
    std::atomic<int64_t> next_id_{1};
    std::mutex promises_mutex_;
    std::map<int64_t, std::promise<std::expected<nlohmann::json, error>>> promises_;

    void handle_message(const nlohmann::json& msg) {
        if (msg.contains("id") && !msg.contains("method")) {
            // It's a response
            response res = msg.get<response>();
            if (std::holds_alternative<int64_t>(res.id)) {
                int64_t id = std::get<int64_t>(res.id);
                
                std::promise<std::expected<nlohmann::json, error>> prom;
                {
                    std::lock_guard<std::mutex> lock(promises_mutex_);
                    auto it = promises_.find(id);
                    if (it != promises_.end()) {
                        prom = std::move(it->second);
                        promises_.erase(it);
                    } else {
                        return; // Unknown request ID
                    }
                }

                if (res.err.has_value()) {
                    prom.set_value(std::unexpected(*res.err));
                } else if (res.result.has_value()) {
                    prom.set_value(*res.result);
                } else {
                    prom.set_value(nlohmann::json::object());
                }
            }
        }
    }
};

} // namespace synapse
