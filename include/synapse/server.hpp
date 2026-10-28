#pragma once

#include <synapse/detail/message_decoder.hpp>
#include <synapse/detail/owned_executor.hpp>
#include <synapse/detail/server_policy.hpp>
#include <synapse/detail/schema_validator.hpp>
#include <synapse/server_options.hpp>
#include <synapse/transport.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>

namespace synapse {

// Borrowed transport and captured state outlive this owner and its joined work.
// One owner waits at a time. Destruction can wait for noncooperative callbacks.
class server {
public:
    struct server_info { std::string name = "synapse-mcp-server"; std::string version = "0.2.0"; };
    template<class Transport>
    explicit server(Transport& transport, server_info info = {}, server_options options = {})
        : info_(std::move(info)), options_(std::move(options)),
          transport_start_([&transport](auto cb, auto close) { return transport.start(std::move(cb), std::move(close)); }),
          transport_send_([&transport](const auto& wire) { return transport.send(wire); }),
          transport_stop_([&transport] { transport.stop(); }),
          transport_wait_([&transport] { return transport.wait(); }),
          transport_wait_for_([&transport](auto timeout) { return transport.wait_for(timeout); }),
          transport_frame_limit_([&transport] {
              if constexpr (requires { transport.max_encoded_frame_bytes(); })
                  return static_cast<std::size_t>(transport.max_encoded_frame_bytes());
              else return stdio_options{}.max_frame_bytes;
          }) {}
    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;
    ~server() { stop(); (void)wait(); }

    std::expected<void, sdk_error> register_tool(tool descriptor, tool_handler handler) {
        std::lock_guard lock(mutex_);
        if (state_ != lifecycle::configuring) return local_error("Registration is frozen");
        if (!handler || descriptor.name.empty() || !safe_text(descriptor.name) ||
            !safe_text(descriptor.description) || tools_.contains(descriptor.name) ||
            !descriptor.inputSchema.is_object() || !descriptor.inputSchema.contains("type") ||
            descriptor.inputSchema["type"] != "object" ||
            (descriptor.inputSchema.contains("properties") && !descriptor.inputSchema["properties"].is_object()))
            return std::unexpected(sdk_error{sdk_errc::invalid_schema, "Invalid tool descriptor"});
        auto validator = detail::compile_schema(descriptor.inputSchema, {});
        if (!validator) return std::unexpected(validator.error());
        auto key = descriptor.name;
        tools_.emplace(std::move(key), registered_tool{std::move(descriptor), std::move(handler), std::move(*validator)});
        return {};
    }
    std::expected<void, sdk_error> register_resource(resource descriptor, resource_handler handler) {
        std::lock_guard lock(mutex_);
        if (state_ != lifecycle::configuring) return local_error("Registration is frozen");
        if (!handler || descriptor.uri.empty() || descriptor.name.empty() ||
            !safe_text(descriptor.uri) || !safe_text(descriptor.name) || !safe_text(descriptor.description) ||
            (descriptor.mimeType && (descriptor.mimeType->empty() || !safe_text(*descriptor.mimeType))) ||
            resources_.contains(descriptor.uri)) return local_error("Invalid resource descriptor");
        auto key = descriptor.uri;
        resources_.emplace(std::move(key), registered_resource{std::move(descriptor), std::move(handler)});
        return {};
    }
    std::expected<void, sdk_error> start() {
        {
            std::lock_guard lock(mutex_);
            if (state_ != lifecycle::configuring) return local_error("Server already started or stopped");
            state_ = lifecycle::starting;
            if (info_.name.empty() || info_.version.empty() || !safe_text(info_.name) || !safe_text(info_.version) ||
                !options_.max_pending_requests || !options_.tool_burst ||
                !std::isfinite(options_.tool_requests_per_second) || options_.tool_requests_per_second <= 0 || !options_.now) {
                state_ = lifecycle::stopped;
                return local_error("Invalid server limits");
            }
        }
        try {
            auto executor = std::make_unique<detail::owned_executor>(options_.max_pending_requests);
            bool interrupted;
            {
                std::lock_guard lock(mutex_);
                executor_ = std::move(executor);
                interrupted = state_ != lifecycle::starting;
            }
            if (interrupted) { stop(); (void)wait(); return local_error("Server stopped during startup"); }
            auto result = transport_start_([this](const auto& wire) {
                callback_scope scope(this);
                try { receive(wire); } catch (...) { stop(); }
            }, [this](close_reason) { callback_scope scope(this); stop(); });
            if (!result) { stop(); (void)wait(); return result; }
            {
                std::lock_guard lock(mutex_);
                interrupted = state_ != lifecycle::starting;
                if (!interrupted) state_ = lifecycle::running;
            }
            if (interrupted) { stop(); (void)wait(); return local_error("Server stopped during startup"); }
            return {};
        } catch (...) {
            stop(); (void)wait();
            return std::unexpected(sdk_error{sdk_errc::resource_exhausted, "Cannot start server work"});
        }
    }
    void stop() noexcept {
        detail::owned_executor* executor;
        {
            std::lock_guard lock(mutex_);
            if (state_ == lifecycle::stopped) return;
            state_ = lifecycle::stopping;
            protocol_ = connection::closing;
            for (auto& op : inflight_) op->cancelled = true;
            executor = executor_.get();
        }
        if (executor) { callback_scope scope(this); executor->stop(); }
        transport_stop_(); // A close callback may synchronously reenter stop.
    }
    std::expected<void, sdk_error> wait() {
        if (callback_scope::contains(this)) return local_error("Cannot wait inside SDK callback");
        if (auto result = transport_wait_(); !result) return result;
        detail::owned_executor* executor;
        { std::lock_guard lock(mutex_); executor = executor_.get(); }
        if (executor) if (auto result = executor->wait(); !result) return result;
        { std::lock_guard lock(mutex_); state_ = lifecycle::stopped; }
        return {};
    }
    std::expected<bool, sdk_error> wait_for(std::chrono::milliseconds timeout) {
        if (callback_scope::contains(this)) return std::unexpected(local_error("Cannot wait inside SDK callback").error());
        const auto deadline = std::chrono::steady_clock::now() + (std::max)(timeout, std::chrono::milliseconds::zero());
        auto transport = transport_wait_for_(timeout);
        if (!transport || !*transport) return transport;
        detail::owned_executor* executor;
        { std::lock_guard lock(mutex_); executor = executor_.get(); }
        if (executor) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            auto result = executor->wait_for((std::max)(remaining, std::chrono::milliseconds::zero()));
            if (!result || !*result) return result;
        }
        { std::lock_guard lock(mutex_); state_ = lifecycle::stopped; }
        return true;
    }
private:
    enum class lifecycle { configuring, starting, running, stopping, stopped };
    enum class connection { await_initialize, await_initialized, ready, closing };
    struct registered_tool { tool descriptor; tool_handler handler; detail::compiled_schema validator; };
    struct registered_resource { resource descriptor; resource_handler handler; };
    struct operation { json_rpc_id id; bool cancelled = false; };
    struct completion {
        server* owner; std::shared_ptr<operation> op;
        ~completion() { owner->complete(op); }
    };
    struct callback_scope {
        server* owner;
        callback_scope* previous;
        explicit callback_scope(server* value) noexcept : owner(value), previous(active_) { active_ = this; }
        ~callback_scope() { active_ = previous; }
        callback_scope(const callback_scope&) = delete;
        callback_scope& operator=(const callback_scope&) = delete;
        static bool contains(const server* value) noexcept {
            for (auto* scope = active_; scope; scope = scope->previous) {
                if (scope->owner == value) return true;
            }
            return false;
        }
    };
    inline static thread_local callback_scope* active_ = nullptr;
    server_info info_;
    server_options options_;
    detail::server_policy policy_{options_};
    std::function<std::expected<void, sdk_error>(stdio_transport::message_callback, stdio_transport::close_callback)> transport_start_;
    std::function<std::expected<void, sdk_error>(const nlohmann::json&)> transport_send_;
    std::function<void()> transport_stop_;
    std::function<std::expected<void, sdk_error>()> transport_wait_;
    std::function<std::expected<bool, sdk_error>(std::chrono::milliseconds)> transport_wait_for_;
    std::function<std::size_t()> transport_frame_limit_;
    std::mutex mutex_;
    lifecycle state_ = lifecycle::configuring;
    connection protocol_ = connection::await_initialize;
    std::map<std::string, registered_resource> resources_;
    std::map<std::string, registered_tool> tools_;
    std::vector<std::shared_ptr<operation>> inflight_;
    std::unique_ptr<detail::owned_executor> executor_;

    static std::expected<void, sdk_error> local_error(const char* message) {
        return std::unexpected(sdk_error{sdk_errc::invalid_state, message});
    }
    static bool safe_text(const std::string& text) {
        for (unsigned char ch : text) if ((ch < 32 && ch != '\t' && ch != '\n' && ch != '\r') || ch == 127) return false;
        try { (void)nlohmann::json(text).dump(); return true; } catch (...) { return false; }
    }
    static response failure(json_rpc_id id, int code, const char* message) {
        response reply; reply.id = std::move(id); reply.err = error{code, message}; return reply;
    }
    static response failure(json_rpc_id id, error_code code, const char* message) {
        return failure(std::move(id), static_cast<int>(code), message);
    }
    void complete(const std::shared_ptr<operation>& op) noexcept {
        std::lock_guard lock(mutex_); std::erase(inflight_, op);
    }
    bool emit(const response& reply) { return transport_send_(nlohmann::json(reply)).has_value(); }
    bool serving() const noexcept { return state_ == lifecycle::starting || state_ == lifecycle::running; }

    void receive(const nlohmann::json& wire) {
        auto decoded = detail::decode_message(wire);
        if (!decoded) {
            if (wire.is_object() && wire.contains("method") && !wire.contains("id")) return;
            bool ok = true;
            { std::lock_guard lock(mutex_); if (serving()) ok = emit(decoded.error()); }
            if (!ok) stop();
            return;
        }
        if (auto* note = std::get_if<notification>(&*decoded)) { notify(*note); return; }
        auto* req = std::get_if<request>(&*decoded);
        if (!req) return;
        std::shared_ptr<operation> op;
        bool close = false;
        {
            std::lock_guard lock(mutex_);
            if (!serving() || protocol_ == connection::closing) return;
            if (std::any_of(inflight_.begin(), inflight_.end(), [&](const auto& item) { return detail::same_request_id(item->id, req->id); })) {
                (void)emit(failure(nullptr, error_code::invalid_request, "Duplicate in-flight request ID"));
                protocol_ = connection::closing; close = true;
            } else if (req->method == "initialize" || req->method == "ping") {
                response reply; reply.id = req->id;
                if (req->method == "ping") reply.result = nlohmann::json::object();
                else reply = initialize(*req);
                close = !emit(reply);
            } else if (protocol_ != connection::ready) {
                close = !emit(failure(req->id, error_code::invalid_request, "Server is not initialized"));
            } else {
                op = std::make_shared<operation>(req->id); inflight_.push_back(op);
            }
        }
        if (close) { stop(); return; }
        if (!op) return;
        // Capture destruction retires IDs even for discarded queued work.
        // Executor capture destructors never run under the server mutex.
        auto done = std::make_shared<completion>(this, op);
        auto queued = executor_->enqueue(req->id, [this, req = *req, op, done](const request_context& context) {
            callback_scope scope(this);
            { std::lock_guard lock(mutex_); if (op->cancelled || !serving()) return; }
            response reply;
            try { reply = application(req, context); }
            catch (...) { reply = failure(req.id, error_code::internal_error, "Internal error"); }
            const auto frame_limit = transport_frame_limit_();
            if (!detail::server_policy::frame_fits(nlohmann::json(reply), frame_limit)) {
                if (req.method == "tools/call" && reply.result) reply = tool_failure(req.id);
                else reply = failure(req.id, error_code::internal_error, "Internal error");
                // An ID may make even a sanitized fallback exceed the limit.
                if (!detail::server_policy::frame_fits(nlohmann::json(reply), frame_limit)) {
                    stop(); return;
                }
            }
            bool ok = true;
            {
                // Cancellation and writer admission serialize on this lock.
                std::lock_guard lock(mutex_);
                if (!op->cancelled && serving()) ok = emit(reply);
            }
            if (!ok) stop();
        });
        if (!queued) {
            bool ok = true;
            {
                std::lock_guard lock(mutex_);
                if (!op->cancelled && serving()) {
                    auto reply = failure(req->id, -32052, "Request queue is full");
                    reply.err->data = nlohmann::json{{"reason", "queue_full"}}; ok = emit(reply);
                }
            }
            if (!ok) stop();
        }
    }
    response initialize(const request& req) {
        if (protocol_ != connection::await_initialize) return failure(req.id, error_code::invalid_request, "Initialization already attempted");
        if (!req.params || !req.params->is_object()) return failure(req.id, error_code::invalid_params, "Invalid initialize parameters");
        const auto& p = *req.params;
        if (!p.contains("protocolVersion") || !p["protocolVersion"].is_string() ||
            !p.contains("capabilities") || !p["capabilities"].is_object() || !p.contains("clientInfo") || !p["clientInfo"].is_object() ||
            !p["clientInfo"].contains("name") || !p["clientInfo"]["name"].is_string() ||
            !p["clientInfo"].contains("version") || !p["clientInfo"]["version"].is_string())
            return failure(req.id, error_code::invalid_params, "Invalid initialize parameters");
        nlohmann::json capabilities = nlohmann::json::object();
        if (!resources_.empty()) capabilities["resources"] = nlohmann::json::object();
        if (!tools_.empty()) capabilities["tools"] = nlohmann::json::object();
        response reply; reply.id = req.id;
        reply.result = nlohmann::json{{"protocolVersion", "2025-11-25"}, {"capabilities", capabilities},
            {"serverInfo", {{"name", info_.name}, {"version", info_.version}}}};
        protocol_ = connection::await_initialized;
        return reply;
    }
    void notify(const notification& note) {
        std::shared_ptr<operation> cancelled;
        {
            std::lock_guard lock(mutex_);
            if (!serving()) return;
            if (note.method == "notifications/initialized") {
                if (protocol_ == connection::await_initialized && (!note.params || note.params->is_object())) protocol_ = connection::ready;
            } else if (note.method == "notifications/cancelled" && note.params && note.params->is_object() &&
                note.params->contains("requestId") && detail::is_request_id((*note.params)["requestId"]) &&
                (!note.params->contains("reason") || (*note.params)["reason"].is_string())) {
                auto id = detail::read_json_rpc_id((*note.params)["requestId"]);
                for (const auto& op : inflight_) if (detail::same_request_id(op->id, id)) { op->cancelled = true; cancelled = op; break; }
            }
        }
        if (cancelled) { callback_scope scope(this); executor_->cancel(cancelled->id); }
    }
    response application(const request& req, const request_context& context) {
        response reply; reply.id = req.id;
        if (req.method == "tools/list") {
            if (req.params && (!req.params->is_object() || (req.params->contains("cursor") && !(*req.params)["cursor"].is_string())))
                return failure(req.id, error_code::invalid_params, "Invalid tool parameters");
            auto list = nlohmann::json::array();
            for (const auto& [name, item] : tools_) list.push_back(item.descriptor);
            reply.result = nlohmann::json{{"tools", std::move(list)}};
        } else if (req.method == "tools/call") {
            if (!req.params || !req.params->is_object() || !req.params->contains("name") || !(*req.params)["name"].is_string() ||
                (req.params->contains("arguments") && !(*req.params)["arguments"].is_object()))
                return failure(req.id, error_code::invalid_params, "Invalid tool parameters");
            const auto found = tools_.find((*req.params)["name"].get_ref<const std::string&>());
            if (found == tools_.end()) return failure(req.id, error_code::invalid_params, "Unknown tool");
            if (!policy_.allow(found->first, context)) {
                auto denied = failure(req.id, -32050, "Access denied");
                denied.err->data = nlohmann::json{{"reason", "access_denied"}};
                return denied;
            }
            if (!policy_.consume()) {
                auto limited = failure(req.id, -32051, "Rate limited");
                limited.err->data = nlohmann::json{{"reason", "rate_limited"}};
                return limited;
            }
            auto arguments = req.params->value("arguments", nlohmann::json::object());
            auto valid = found->second.validator.validate(arguments);
            if (!valid) {
                if (valid.error().kind != detail::validation_failure_kind::instance_mismatch)
                    return failure(req.id, error_code::internal_error, "Internal error");
                auto text = policy_.filter(req.method, "Tool arguments do not match schema", context);
                if (!text) return tool_failure(req.id);
                reply.result = nlohmann::json{{"content", nlohmann::json::array({{{"type", "text"}, {"text", std::move(*text)}}})}, {"isError", true}};
                return reply;
            }
            auto result = found->second.handler(arguments, context);
            std::string text = result ? std::move(result->text) : result.error().public_message;
            auto filtered = policy_.filter(req.method, text, context);
            if (!filtered) return tool_failure(req.id);
            reply.result = nlohmann::json{{"content", nlohmann::json::array({{{"type", "text"}, {"text", std::move(*filtered)}}})}};
            if (!result) (*reply.result)["isError"] = true;
        } else if (req.method == "resources/list") {
            if (req.params && (!req.params->is_object() || (req.params->contains("cursor") && !(*req.params)["cursor"].is_string())))
                return failure(req.id, error_code::invalid_params, "Invalid resource parameters");
            auto list = nlohmann::json::array();
            for (const auto& [uri, item] : resources_) list.push_back(item.descriptor);
            reply.result = nlohmann::json{{"resources", std::move(list)}};
        } else if (req.method == "resources/read") {
            if (!req.params || !req.params->is_object() || !req.params->contains("uri") || !(*req.params)["uri"].is_string())
                return failure(req.id, error_code::invalid_params, "Invalid resource parameters");
            const auto found = resources_.find((*req.params)["uri"].get_ref<const std::string&>());
            if (found == resources_.end()) return failure(req.id, -32002, "Resource not found");
            auto result = found->second.handler(context);
            if (!result) return failure(req.id, error_code::internal_error, "Internal error");
            auto text = std::move(result->text);
            auto filtered = policy_.filter(req.method, text, context);
            if (!filtered) return failure(req.id, error_code::internal_error, "Internal error");
            text = std::move(*filtered);
            nlohmann::json content = {{"uri", found->second.descriptor.uri}, {"text", std::move(text)}};
            if (found->second.descriptor.mimeType) content["mimeType"] = *found->second.descriptor.mimeType;
            reply.result = nlohmann::json{{"contents", nlohmann::json::array({std::move(content)})}};
        } else return failure(req.id, error_code::method_not_found, "Method not found");
        return reply;
    }
    static response tool_failure(json_rpc_id id) {
        response reply; reply.id = std::move(id);
        reply.result = nlohmann::json{{"content", nlohmann::json::array({{{"type", "text"}, {"text", "Tool output rejected"}}})}, {"isError", true}};
        return reply;
    }
};
} // namespace synapse
