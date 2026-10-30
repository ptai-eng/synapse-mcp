#pragma once

#include <synapse/server_options.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

namespace synapse::detail {

// All admission and filter calls are serialized by the server's application worker.
class server_policy {
    const server_options& options_;
    double tokens_;
    std::chrono::steady_clock::time_point last_{};
    bool clock_started_ = false;

    class counting_buffer : public std::streambuf {
        std::size_t size_ = 0;
        const std::size_t limit_;
    protected:
        std::streamsize xsputn(const char*, std::streamsize count) override {
            const auto available = limit_ - size_;
            const auto requested = static_cast<std::size_t>(count);
            const auto accepted = (std::min)(available, requested);
            size_ += accepted;
            return static_cast<std::streamsize>(accepted);
        }
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
            if (size_ == limit_) return traits_type::eof();
            ++size_; return ch;
        }
    public:
        explicit counting_buffer(std::size_t limit) : limit_(limit) {}
    };

public:
    explicit server_policy(const server_options& options)
        : options_(options), tokens_(static_cast<double>(options.tool_burst)) {}

    bool allow(std::string_view name, const request_context& context) const {
        return !options_.allow_tool || options_.allow_tool(name, context);
    }

    bool consume() {
        const auto now = options_.now();
        if (!clock_started_) { last_ = now; clock_started_ = true; }
        if (now > last_) {
            const auto elapsed = std::chrono::duration<double>(now - last_).count();
            tokens_ = (std::min)(static_cast<double>(options_.tool_burst),
                                 tokens_ + elapsed * options_.tool_requests_per_second);
            last_ = now;
        }
        if (tokens_ < 1.0) return false;
        tokens_ -= 1.0;
        return true;
    }

    static bool valid_text(std::string_view text) {
        for (const unsigned char ch : text)
            if ((ch < 32 && ch != '\t' && ch != '\n' && ch != '\r') || ch == 127) return false;
        try { (void)nlohmann::json(text).dump(); return true; }
        catch (const nlohmann::json::exception&) { return false; }
    }

    std::expected<std::string, handler_error> filter(std::string_view operation,
        std::string_view text, const request_context& context) const {
        if (!valid_text(text)) return std::unexpected(handler_error{"Invalid output"});
        std::string result(text);
        if (options_.filter_output) {
            auto filtered = options_.filter_output(operation, text, context);
            if (!filtered || !valid_text(*filtered)) return std::unexpected(handler_error{"Invalid output"});
            result = std::move(*filtered);
        }
        return result;
    }

    static bool frame_fits(const nlohmann::json& value, std::size_t limit) {
        try {
            counting_buffer buffer(limit);
            std::ostream output(&buffer);
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output << value;
            return true;
        } catch (const std::exception&) { return false; }
    }
};
}
