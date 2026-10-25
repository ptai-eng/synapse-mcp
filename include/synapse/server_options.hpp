#pragma once

#include <synapse/handlers.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <string>
#include <string_view>

namespace synapse {

struct stdio_options {
    std::size_t max_frame_bytes = 1'048'576;
    std::size_t max_json_depth = 64;
    std::size_t max_outbound_bytes = 4'194'304;
    std::chrono::milliseconds flush_budget{250};
};

struct server_options {
    std::size_t max_pending_requests = 16;
    std::size_t tool_burst = 16;
    double tool_requests_per_second = 16.0;
    std::function<bool(std::string_view, const request_context&)> allow_tool;
    std::function<std::expected<std::string, handler_error>(
        std::string_view, std::string_view, const request_context&)> filter_output;
    std::function<std::chrono::steady_clock::time_point()> now = [] {
        return std::chrono::steady_clock::now();
    };
};

} // namespace synapse
