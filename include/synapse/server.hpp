#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <expected>
#include <stdexcept>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>
#include "protocol.hpp"
#include "transport.hpp"

namespace synapse {

class server {
public:
    using tool_callback = std::function<std::expected<nlohmann::json, error>(const nlohmann::json&)>;
    using resource_callback = std::function<nlohmann::json()>;
    using prompt_callback = std::function<std::vector<nlohmann::json>(const std::map<std::string, std::string>&)>;

    struct server_info {
        std::string name = "synapse-mcp-server";
        std::string version = "0.1.0";
    };

    using send_callback = std::function<void(const nlohmann::json&)>;
    using start_callback = std::function<void(std::function<void(const nlohmann::json&)>)>;
    using stop_callback = std::function<void()>;
    using wait_callback = std::function<void()>;

    template <typename Transport>
    explicit server(Transport& transport, server_info info = {}) 
        : info_(std::move(info)),
          transport_start_([&transport](auto cb) { transport.start(std::move(cb)); }),
          transport_stop_([&transport]() { transport.stop(); }),
          transport_wait_([&transport]() { transport.wait(); }),
          transport_send_([&transport](const nlohmann::json& msg) { transport.send(msg); }) 
    {}

    void register_tool(std::string name, std::string description, schema inputSchema, tool_callback callback) {
        tools_[name] = {
            synapse::tool{name, description, inputSchema},
            std::move(callback)
        };
    }

    void register_resource(std::string uri, std::string name, std::string description, resource_callback callback, std::optional<std::string> mimeType = std::nullopt) {
        std::unique_lock lock(mutex_);
        resources_[uri] = {
            synapse::resource{uri, name, description, mimeType},
            std::move(callback)
        };
    }

    void register_prompt(std::string name, std::string description, std::vector<prompt_argument> args, prompt_callback callback) {
        std::unique_lock lock(mutex_);
        prompts_[name] = {
            synapse::prompt{name, description, args},
            std::move(callback)
        };
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

    void start() {
        if (transport_start_) {
            transport_start_([this](const nlohmann::json& msg) {
                handle_message(msg);
            });
        }
    }

    void wait() {
        if (transport_wait_) {
            transport_wait_();
        }
    }

    void stop() {
        if (transport_stop_) {
            transport_stop_();
        }
    }

private:
    struct registered_tool {
        synapse::tool info;
        tool_callback callback;
    };

    struct registered_resource {
        synapse::resource info;
        resource_callback callback;
    };

    struct registered_prompt {
        synapse::prompt info;
        prompt_callback callback;
    };

    server_info info_;
    start_callback transport_start_;
    stop_callback transport_stop_;
    wait_callback transport_wait_;
    send_callback transport_send_;
    
    std::shared_mutex mutex_;
    std::map<std::string, registered_tool> tools_;
    std::map<std::string, registered_resource> resources_;
    std::map<std::string, registered_prompt> prompts_;

    std::atomic<int64_t> next_id_{1};
    std::mutex promises_mutex_;
    std::map<int64_t, std::promise<std::expected<nlohmann::json, error>>> promises_;

    void handle_message(const nlohmann::json& msg) {
        if (!msg.contains("method")) {
            // It might be a response to something we sent
            if (msg.contains("id")) {
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
            return;
        }

        if (msg.contains("id")) {
            // It's a Request
            request req = msg.get<request>();
            handle_request(req);
        } else {
            // It's a Notification
            notification notif = msg.get<notification>();
            handle_notification(notif);
        }
    }

    void handle_notification(const notification& notif) {
        // We generally ignore notifications we don't care about
        if (notif.method == "initialized") {
            // Client has received initialization response
        }
    }

    void handle_request(const request& req) {
        // Run request handler in a detached thread so we don't block the transport reader
        std::thread([this, req]() {
            response res;
            res.id = req.id;

            try {
                if (req.method == "initialize") {
                    handle_initialize(req, res);
                } else if (req.method == "tools/list") {
                    handle_tools_list(req, res);
                } else if (req.method == "tools/call") {
                    handle_tools_call(req, res);
                } else if (req.method == "resources/list") {
                    handle_resources_list(req, res);
                } else if (req.method == "resources/read") {
                    handle_resources_read(req, res);
                } else if (req.method == "prompts/list") {
                    handle_prompts_list(req, res);
                } else if (req.method == "prompts/get") {
                    handle_prompts_get(req, res);
                } else {
                    res.err = error(error_code::method_not_found, "Method not found: " + req.method);
                }
            } catch (const std::exception& e) {
                res.err = error(error_code::internal_error, e.what());
            }

            nlohmann::json j_res = res;
            transport_send_(j_res);
        }).detach();
    }

    void handle_initialize(const request& /*req*/, response& res) {
        std::shared_lock lock(mutex_);
        nlohmann::json capabilities = nlohmann::json::object();
        
        if (!tools_.empty()) {
            capabilities["tools"] = nlohmann::json::object();
        }
        if (!resources_.empty()) {
            capabilities["resources"] = nlohmann::json::object();
        }
        if (!prompts_.empty()) {
            capabilities["prompts"] = nlohmann::json::object();
        }

        res.result = nlohmann::json{
            {"protocolVersion", "2024-11-05"},
            {"capabilities", capabilities},
            {"serverInfo", {
                {"name", info_.name},
                {"version", info_.version}
            }}
        };
    }

    void handle_tools_list(const request& /*req*/, response& res) {
        std::shared_lock lock(mutex_);
        nlohmann::json tools_list = nlohmann::json::array();
        for (const auto& [name, t] : tools_) {
            tools_list.push_back(t.info);
        }
        res.result = nlohmann::json{
            {"tools", tools_list}
        };
    }

    void handle_tools_call(const request& req, response& res) {
        if (!req.params.has_value() || !req.params->contains("name")) {
            res.err = error(error_code::invalid_params, "Missing 'name' in tools/call params");
            return;
        }

        std::string name = req.params->at("name").get<std::string>();
        tool_callback callback;
        {
            std::shared_lock lock(mutex_);
            auto it = tools_.find(name);
            if (it == tools_.end()) {
                res.err = error(error_code::invalid_params, "Tool not found: " + name);
                return;
            }
            callback = it->second.callback;
        }

        nlohmann::json arguments = nlohmann::json::object();
        if (req.params->contains("arguments")) {
            arguments = req.params->at("arguments");
        }

        auto result = callback(arguments);
        if (result.has_value()) {
            res.result = nlohmann::json{
                {"content", nlohmann::json::array({
                    {
                        {"type", "text"},
                        {"text", result->dump()}
                    }
                })}
            };
            // Note: If the tool returns a string that isn't valid json, 
            // the user should handle that, but for now we expect tool to return json 
            // and we wrap it in content text block for MCP compliance.
            // Wait, MCP tools usually return `content` array directly in some implementations, 
            // but let's conform to standard: result is { content: [{type:"text", text: "..."}] }
            // Let's improve this: If the user callback returns json object, we serialize it.
            // If they want to return raw text, they can return json string.
            if (result->is_string()) {
                 res.result = nlohmann::json{
                    {"content", nlohmann::json::array({
                        {
                            {"type", "text"},
                            {"text", result->get<std::string>()}
                        }
                    })}
                };
            }
        } else {
            res.result = nlohmann::json{
                {"content", nlohmann::json::array({
                    {
                        {"type", "text"},
                        {"text", result.error().message}
                    }
                })},
                {"isError", true}
            };
        }
    }

    void handle_resources_list(const request& /*req*/, response& res) {
        std::shared_lock lock(mutex_);
        nlohmann::json resources_list = nlohmann::json::array();
        for (const auto& [uri, r] : resources_) {
            resources_list.push_back(r.info);
        }
        res.result = nlohmann::json{
            {"resources", resources_list}
        };
    }

    void handle_resources_read(const request& req, response& res) {
         if (!req.params.has_value() || !req.params->contains("uri")) {
            res.err = error(error_code::invalid_params, "Missing 'uri' in resources/read params");
            return;
        }

        std::string uri = req.params->at("uri").get<std::string>();
        resource_callback callback;
        std::optional<std::string> mimeType;
        {
            std::shared_lock lock(mutex_);
            auto it = resources_.find(uri);
            if (it == resources_.end()) {
                res.err = error(error_code::invalid_params, "Resource not found: " + uri);
                return;
            }
            callback = it->second.callback;
            mimeType = it->second.info.mimeType;
        }

        auto content = callback();
        std::string text_content;
        if (content.is_string()) {
            text_content = content.get<std::string>();
        } else {
            text_content = content.dump();
        }

        nlohmann::json resource_content = {
            {"uri", uri},
            {"text", text_content}
        };
        if (mimeType.has_value()) {
            resource_content["mimeType"] = *mimeType;
        }

        res.result = nlohmann::json{
            {"contents", nlohmann::json::array({ resource_content })}
        };
    }

    void handle_prompts_list(const request& /*req*/, response& res) {
        std::shared_lock lock(mutex_);
        nlohmann::json prompts_list = nlohmann::json::array();
        for (const auto& [name, p] : prompts_) {
            prompts_list.push_back(p.info);
        }
        res.result = nlohmann::json{
            {"prompts", prompts_list}
        };
    }

    void handle_prompts_get(const request& req, response& res) {
        if (!req.params.has_value() || !req.params->contains("name")) {
            res.err = error(error_code::invalid_params, "Missing 'name' in prompts/get params");
            return;
        }

        std::string name = req.params->at("name").get<std::string>();
        prompt_callback callback;
        {
            std::shared_lock lock(mutex_);
            auto it = prompts_.find(name);
            if (it == prompts_.end()) {
                res.err = error(error_code::invalid_params, "Prompt not found: " + name);
                return;
            }
            callback = it->second.callback;
        }

        std::map<std::string, std::string> args;
        if (req.params->contains("arguments")) {
            for (const auto& [key, value] : req.params->at("arguments").items()) {
                if (value.is_string()) {
                    args[key] = value.get<std::string>();
                } else {
                    args[key] = value.dump();
                }
            }
        }

        auto result = callback(args);
        res.result = nlohmann::json{
            {"messages", result}
        };
    }
};

} // namespace synapse
