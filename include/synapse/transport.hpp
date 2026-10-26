#pragma once

#include <synapse/detail/native_stdio.hpp>
#include <synapse/server_options.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <expected>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <stop_token>
#include <streambuf>
#include <string>
#include <thread>
#include <utility>
#include <nlohmann/json.hpp>

namespace synapse {

enum class close_reason { eof, requested_stop, io_error, protocol_limit };

// Owns exactly one reader and writer. The application owns this object until
// owner-thread wait completes; destroying it from a callback is invalid.
class stdio_transport {
public:
    using message_callback = std::function<void(const nlohmann::json&)>;
    using close_callback = std::function<void(close_reason)>;

    explicit stdio_transport(stdio_options options = {})
        : stdio_transport(detail::standard_input(), detail::standard_output(), options) {}
    stdio_transport(detail::native_stdio_endpoint input,
                    detail::native_stdio_endpoint output, stdio_options options = {})
        : input_(input), output_(output), options_(options) {}
    stdio_transport(const stdio_transport&) = delete;
    stdio_transport& operator=(const stdio_transport&) = delete;
    ~stdio_transport() { stop(); (void)wait(); }

    std::expected<void, sdk_error> start(message_callback on_message,
                                          close_callback on_close = {}) {
        std::unique_lock lock(mutex_);
        if (consumed_) return failure(sdk_errc::invalid_state, "Transport already started or stopped");
        consumed_ = true;
        if (!options_.max_frame_bytes || !options_.max_json_depth ||
            !options_.max_outbound_bytes || options_.flush_budget.count() <= 0 ||
            options_.flush_budget > std::chrono::milliseconds(250))
            return failure(sdk_errc::invalid_state, "Invalid stdio limits");
        try {
            auto opened = detail::native_stdio::open(input_, output_);
            if (!opened) return std::unexpected(opened.error());
            io_.emplace(std::move(*opened));
            message_ = std::move(on_message);
            close_ = std::move(on_close);
            writer_done_ = false;
            try { writer_ = std::thread([this] { worker(false); }); }
            catch (...) { writer_done_ = true; throw; }
            reader_done_ = false;
            try { reader_ = std::thread([this] { worker(true); }); }
            catch (...) { reader_done_ = true; throw; }
            started_ = true;
            released_ = true;
            changed_.notify_all();
            return {};
        } catch (...) {
            // Publish no close event for a failed start, and join partial work
            // before releasing its native endpoints.
            closing_ = true;
            released_ = true;
            if (io_) io_->request_stop();
            changed_.notify_all();
            lock.unlock();
            if (reader_.joinable()) reader_.join();
            if (writer_.joinable()) writer_.join();
            lock.lock();
            io_.reset();
            return failure(sdk_errc::resource_exhausted, "Cannot start stdio work");
        }
    }

    std::expected<void, sdk_error> send(const nlohmann::json& message) {
        std::lock_guard lock(mutex_);
        if (!started_ || closing_) return failure(sdk_errc::invalid_state, "Transport is not running");
        try {
            // The stream adapter bounds bytes during escaping/encoding, before
            // any complete JSON dump can allocate an oversized string.
            bounded_buffer buffer((std::min)(options_.max_frame_bytes,
                options_.max_outbound_bytes - 1));
            std::ostream stream(&buffer);
            stream.exceptions(std::ios::badbit | std::ios::failbit);
            try { stream << message; }
            catch (...) {
                if (buffer.full()) return failure(sdk_errc::resource_exhausted, "Encoded frame limit exceeded");
                throw;
            }
            auto bytes = buffer.take();
            bytes.push_back('\n');
            if (bytes.size() > options_.max_outbound_bytes - queued_bytes_)
                return failure(sdk_errc::resource_exhausted, "Writer queue is full");
            const auto size = bytes.size();
            queue_.push_back(std::move(bytes));
            queued_bytes_ += size;
            changed_.notify_all();
            return {};
        } catch (const nlohmann::json::exception&) {
            return failure(sdk_errc::invalid_state, "Cannot encode JSON frame");
        } catch (...) {
            return failure(sdk_errc::resource_exhausted, "Cannot queue JSON frame");
        }
    }

    void stop() noexcept { begin_close(close_reason::requested_stop); }

    std::expected<void, sdk_error> wait() {
        auto result = join_until(std::nullopt);
        if (!result) return std::unexpected(result.error());
        return {};
    }
    std::expected<bool, sdk_error> wait_for(std::chrono::milliseconds timeout) {
        return join_until(std::chrono::steady_clock::now() +
            (std::max)(timeout, std::chrono::milliseconds::zero()));
    }

private:
    class bounded_buffer : public std::streambuf {
        std::string bytes_;
        std::size_t limit_;
        bool full_ = false;
    protected:
        std::streamsize xsputn(const char* data, std::streamsize size) override {
            if (size < 0 || static_cast<std::size_t>(size) > limit_ - bytes_.size()) {
                full_ = true;
                return 0;
            }
            bytes_.append(data, static_cast<std::size_t>(size));
            return size;
        }
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
            const char value = traits_type::to_char_type(ch);
            return xsputn(&value, 1) == 1 ? ch : traits_type::eof();
        }
    public:
        explicit bounded_buffer(std::size_t limit) : limit_(limit) {}
        bool full() const noexcept { return full_; }
        std::string take() { return std::move(bytes_); }
    };
    struct callback_scope {
        stdio_transport* previous = active_;
        explicit callback_scope(stdio_transport* transport) { active_ = transport; }
        ~callback_scope() { active_ = previous; }
    };
    inline static thread_local stdio_transport* active_ = nullptr;
    detail::native_stdio_endpoint input_, output_;
    stdio_options options_;
    // Native endpoints remain alive until after both thread members are joined.
    std::optional<detail::native_stdio> io_;
    std::thread reader_, writer_;
    std::stop_source reader_stop_;
    std::mutex mutex_;
    std::condition_variable changed_;
    message_callback message_;
    close_callback close_;
    std::deque<std::string> queue_;
    std::size_t queued_bytes_ = 0;
    bool consumed_ = false, started_ = false, released_ = false;
    bool closing_ = false, reader_done_ = true, writer_done_ = true;
    bool close_done_ = true, close_claimed_ = false;
    std::optional<close_reason> reason_;
    std::chrono::steady_clock::time_point flush_deadline_{};

    static std::unexpected<sdk_error> failure(sdk_errc code, const char* text) {
        return std::unexpected(sdk_error{code, text});
    }
    void begin_close(close_reason reason) noexcept {
        {
            std::lock_guard lock(mutex_);
            consumed_ = true;
            if (closing_) return;
            closing_ = true;
            reason_ = reason;
            flush_deadline_ = std::chrono::steady_clock::now() + options_.flush_budget;
            close_done_ = !started_;
            changed_.notify_all();
        }
        reader_stop_.request_stop();
    }
    void publish_close() noexcept {
        close_reason reason;
        {
            std::lock_guard lock(mutex_);
            if (!started_ || !reason_ || close_claimed_) return;
            close_claimed_ = true;
            reason = *reason_;
        }
        // Always invoke user closure on owned work, so stop() never waits for
        // arbitrary user code. The claiming worker cannot be joined until it
        // finishes this callback.
        try { if (close_) close_(reason); } catch (...) {}
        std::lock_guard lock(mutex_);
        close_done_ = true;
        changed_.notify_all();
    }
    std::expected<bool, sdk_error> join_until(
        std::optional<std::chrono::steady_clock::time_point> deadline) {
        if (active_ == this) return failure(sdk_errc::invalid_state, "Cannot wait inside transport callback");
        std::unique_lock lock(mutex_);
        for (;;) {
            const auto now = std::chrono::steady_clock::now();
            if (io_) {
                if (closing_ && now >= flush_deadline_) io_->request_stop();
                // Retry at a bounded cadence, including the Win32 interval
                // between cancellation's last check and entering native I/O.
                if (closing_) io_->cancel_pending();
            }
            if (reader_done_ && writer_done_ && close_done_) break;
            if (deadline && now >= *deadline) return false;
            auto next = now + std::chrono::milliseconds(1);
            if (deadline) next = (std::min)(next, *deadline);
            changed_.wait_until(lock, next);
        }
        lock.unlock();
        if (reader_.joinable()) reader_.join();
        if (writer_.joinable()) writer_.join();
        return true;
    }
    bool excessive_depth(std::string_view line) const noexcept {
        std::size_t depth = 0;
        bool quoted = false, escaped = false;
        for (const auto ch : line) {
            if (quoted) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') quoted = false;
            } else if (ch == '"') quoted = true;
            else if (ch == '{' || ch == '[') {
                if (depth == options_.max_json_depth) return true;
                ++depth;
            } else if ((ch == '}' || ch == ']') && depth) --depth;
        }
        return false;
    }
    bool frame(std::string_view line) {
        if (line.empty()) return true;
        if (excessive_depth(line)) { begin_close(close_reason::protocol_limit); return false; }
        nlohmann::json value;
        try { value = nlohmann::json::parse(line); }
        catch (const nlohmann::json::parse_error&) {
            const auto diagnostic = nlohmann::json{{"jsonrpc", "2.0"}, {"id", nullptr},
                {"error", {{"code", -32700}, {"message", "Parse error"}}}};
            if (!send(diagnostic)) { begin_close(close_reason::protocol_limit); return false; }
            return true;
        }
        // Callback exceptions are not JSON parse failures and never produce a
        // misleading parse-error response containing application details.
        try { if (message_) message_(value); }
        catch (...) { begin_close(close_reason::io_error); return false; }
        return true;
    }
    void read_loop() {
        std::array<char, 4096> bytes{};
        std::string line;
        while (!reader_stop_.stop_requested()) {
            auto count = io_->read_some(bytes, reader_stop_.get_token());
            if (!count) {
                if (!reader_stop_.stop_requested()) begin_close(close_reason::io_error);
                return;
            }
            if (*count == 0) {
                if (!line.empty() && !frame(line)) return;
                begin_close(close_reason::eof);
                return;
            }
            for (std::size_t i = 0; i < *count; ++i) {
                if (reader_stop_.stop_requested()) return;
                if (bytes[i] == '\n') {
                    if (!frame(line)) return;
                    line.clear();
                } else {
                    if (line.size() == options_.max_frame_bytes) {
                        begin_close(close_reason::protocol_limit); return;
                    }
                    line.push_back(bytes[i]);
                }
            }
        }
    }
    void write_loop() {
        for (;;) {
            std::string bytes;
            {
                std::unique_lock lock(mutex_);
                changed_.wait(lock, [this] { return closing_ || !queue_.empty(); });
                if (queue_.empty()) return;
                if (closing_ && std::chrono::steady_clock::now() >= flush_deadline_) return;
                bytes = std::move(queue_.front());
                queue_.pop_front();
                // queued_bytes_ deliberately retains this active frame.
            }
            std::size_t offset = 0;
            while (offset < bytes.size()) {
                auto count = io_->write_some(std::span<const char>(bytes).subspan(offset));
                if (!count || !*count) { begin_close(close_reason::io_error); return; }
                offset += *count;
            }
            std::lock_guard lock(mutex_);
            queued_bytes_ -= bytes.size();
        }
    }
    void worker(bool reading) noexcept {
        callback_scope scope(this);
        {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, [this] { return released_; });
            if (!started_) {
                (reading ? reader_done_ : writer_done_) = true;
                changed_.notify_all();
                return;
            }
        }
        try { if (reading) read_loop(); else write_loop(); }
        catch (...) { begin_close(close_reason::io_error); }
        publish_close();
        std::lock_guard lock(mutex_);
        (reading ? reader_done_ : writer_done_) = true;
        changed_.notify_all();
    }
};
} // namespace synapse
