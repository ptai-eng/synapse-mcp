#pragma once

#include <synapse/server.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <utility>

// The counter records status-tool invocations; resource and prompt
// reads observe the same application-owned state without changing it.
class native_status_state {
public:
    using clock = std::chrono::steady_clock;
    using now_function = std::function<clock::time_point()>;

    explicit native_status_state(now_function now = [] { return clock::now(); })
        : now_(std::move(now)), started_(now_()) {}

    void record_operation() noexcept { operations_.fetch_add(1, std::memory_order_relaxed); }

    nlohmann::json snapshot() const {
        const auto current = now_();
        const auto elapsed = current > started_ ? current - started_ : clock::duration::zero();
        return {{"uptime_ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()},
                {"handled_operations", operations_.load(std::memory_order_relaxed)},
                {"counter_scope", "app.status tool invocations"}};
    }

private:
    now_function now_;
    clock::time_point started_;
    std::atomic<std::uint64_t> operations_{0};
};

inline std::expected<void, synapse::sdk_error>
register_native_status(synapse::server& server, native_status_state& state) {
    auto tool = server.register_tool({"app.status", "Read the current application status",
        {{"type", "object"}, {"properties", nlohmann::json::object()}, {"additionalProperties", false}}},
        [&state](const nlohmann::json&, const synapse::request_context&)
            -> std::expected<synapse::tool_result, synapse::handler_error> {
            state.record_operation();
            return synapse::tool_result{state.snapshot().dump()};
        });
    if (!tool) return std::unexpected(tool.error());
    auto resource = server.register_resource({"app://status", "Application status",
        "A snapshot of the application state", "application/json"},
        [&state](const synapse::request_context&)
            -> std::expected<synapse::resource_result, synapse::handler_error> {
            return synapse::resource_result{state.snapshot().dump()};
        });
    if (!resource) return std::unexpected(resource.error());
    return server.register_prompt({"explain_status", "Explain the current application status", {}},
        [&state](const std::map<std::string, std::string>&, const synapse::request_context&)
            -> std::expected<synapse::prompt_result, synapse::handler_error> {
            return synapse::prompt_result{{{synapse::prompt_role::user,
                "Explain this application status: " + state.snapshot().dump()}}, std::nullopt};
        });
}
