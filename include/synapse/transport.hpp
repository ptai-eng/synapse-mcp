#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <stop_token>
#include <nlohmann/json.hpp>
#include "protocol.hpp"

namespace synapse {

class stdio_transport {
public:
    using message_callback = std::function<void(const nlohmann::json&)>;

    stdio_transport() = default;
    ~stdio_transport() {
        stop();
    }

    void start(message_callback on_message) {
        if (running_) return;
        running_ = true;
        callback_ = std::move(on_message);

        reader_thread_ = std::jthread([this](std::stop_token stoken) {
            std::string line;
            while (!stoken.stop_requested() && std::getline(std::cin, line)) {
                if (line.empty()) continue;
                try {
                    auto j = nlohmann::json::parse(line);
                    if (callback_) {
                        callback_(j);
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    // Send parse error response
                    error err(error_code::parse_error, "Parse error");
                    response res;
                    res.err = err;
                    res.id = nullptr;
                    nlohmann::json j_res = res;
                    send(j_res);
                }
            }
            running_ = false;
        });
    }

    void stop() {
        if (running_ && reader_thread_.joinable()) {
            reader_thread_.request_stop();
            // We can't trivially interrupt std::getline on std::cin, 
            // but the jthread will stop when stdin closes.
        }
        running_ = false;
    }

    void send(const nlohmann::json& message) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        std::cout << message.dump() << "\n";
        std::cout.flush();
    }

    void wait() {
        if (reader_thread_.joinable()) {
            reader_thread_.join();
        }
    }

private:
    std::jthread reader_thread_;
    std::mutex write_mutex_;
    message_callback callback_;
    std::atomic<bool> running_{false};
};

} // namespace synapse
