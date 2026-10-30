#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
using json = nlohmann::json;
using namespace std::chrono_literals;
struct prompt_transport {
    std::function<void(const json&)> message;
    std::function<void(synapse::close_reason)> closed;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<json> output;
    std::atomic<bool> stopped{false};
    std::expected<void, synapse::sdk_error> start(decltype(message) callback, decltype(closed) close = {}) {
        message = std::move(callback); closed = std::move(close); return {};
    }
    std::expected<void, synapse::sdk_error> send(const json& wire) {
        std::lock_guard lock(mutex); output.push_back(wire); changed.notify_all(); return {};
    }
    void stop() noexcept { if (!stopped.exchange(true) && closed) closed(synapse::close_reason::requested_stop); }
    std::expected<void, synapse::sdk_error> wait() { return {}; }
    std::expected<bool, synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
    json next() {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, 2s, [&] { return !output.empty(); })) return nullptr;
        auto value = output.front(); output.erase(output.begin()); return value;
    }
    json request(int id, std::string method, json params = json::object()) {
        message({{"jsonrpc", "2.0"}, {"id", id}, {"method", std::move(method)}, {"params", std::move(params)}});
        return next();
    }
    void ready() {
        auto result = request(0, "initialize", {{"protocolVersion", "2025-11-25"},
            {"capabilities", json::object()}, {"clientInfo", {{"name", "test"}, {"version", "1"}}}});
        REQUIRE(result.at("result").at("capabilities").contains("prompts"));
        CHECK_FALSE(result.at("result").at("capabilities").at("prompts").contains("listChanged"));
        message({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    }
};
}

TEST_CASE("M1.prompts.text_messages", "[M1][prompts]") {
    prompt_transport transport;
    std::atomic<int> called{0}, filtered{0};
    std::atomic<bool> wrong_thread{false};
    const auto owner = std::this_thread::get_id();
    synapse::server_options options;
    options.filter_output = [&](std::string_view, std::string_view text, const synapse::request_context&)
        -> std::expected<std::string, synapse::handler_error> {
        ++filtered; if (std::this_thread::get_id() == owner) wrong_thread = true;
        return text == "fixture-secret" ? std::string("[redacted]") : std::string(text);
    };
    synapse::server server(transport, {}, options);
    synapse::prompt descriptor{"review", "Review code", {{"code", "Code", true}, {"style", "Style", false}}};
    auto handler = [&](const std::map<std::string, std::string>& arguments, const synapse::request_context&)
        -> std::expected<synapse::prompt_result, synapse::handler_error> {
        ++called; if (std::this_thread::get_id() == owner) wrong_thread = true;
        if (arguments.at("code") == "throw") throw std::runtime_error("private exception");
        if (arguments.at("code") == "bad") return std::unexpected(synapse::handler_error{"private failure"});
        if (arguments.at("code") == "role") return synapse::prompt_result{{{static_cast<synapse::prompt_role>(42), "x"}}, {}};
        if (arguments.at("code") == "unsafe") return synapse::prompt_result{{{synapse::prompt_role::user, "\x1b"}}, {}};
        if (arguments.at("code") == "unsafe-description") return synapse::prompt_result{{}, "\x1b"};
        return synapse::prompt_result{{{synapse::prompt_role::user, arguments.at("code")},
            {synapse::prompt_role::assistant, "fixture-secret"}}, "fixture-secret"};
    };
    CHECK_FALSE(server.register_prompt({"", "", {}}, handler));
    CHECK_FALSE(server.register_prompt({"invalid", "", {{"", "", false}}}, handler));
    CHECK_FALSE(server.register_prompt({"invalid", "", {{"a", "", false}, {"a", "", true}}}, handler));
    CHECK_FALSE(server.register_prompt({"invalid", "\x1b", {}}, handler));
    CHECK_FALSE(server.register_prompt({"invalid", "", {}}, {}));
    REQUIRE(server.register_prompt(descriptor, handler));
    CHECK_FALSE(server.register_prompt(descriptor, handler));
    REQUIRE(server.start());
    CHECK_FALSE(server.register_prompt({"late", "", {}}, handler));
    transport.ready();
    const auto list = transport.request(1, "prompts/list");
    REQUIRE(list.at("result").at("prompts").size() == 1);
    CHECK(list.at("result").at("prompts")[0].at("name") == "review");
    const auto valid = transport.request(2, "prompts/get", {{"name", "review"}, {"arguments", {{"code", "hello"}}}});
    const auto& result = valid.at("result");
    CHECK(result.at("description") == "[redacted]");
    REQUIRE(result.at("messages").size() == 2);
    CHECK(result.at("messages")[0].at("role") == "user");
    CHECK(result.at("messages")[0].at("content").at("text") == "hello");
    CHECK(result.at("messages")[1].at("role") == "assistant");
    CHECK(result.at("messages")[1].at("content").at("text") == "[redacted]");
    CHECK(transport.request(5, "prompts/get", {{"name", "review"}, {"arguments", {{"code", "hello"}, {"style", "brief"}}}}).contains("result"));
    int id = 10;
    for (const auto& bad : {json{{"name", "review"}}, json{{"name", "missing"}},
            json{{"name", "review"}, {"arguments", {{"code", 4}}}},
            json{{"name", "review"}, {"arguments", {{"code", "ok"}, {"unexpected", "x"}}}}}) {
        const auto reply = transport.request(id++, "prompts/get", bad);
        CHECK(reply.at("error").at("code") == -32602);
    }
    CHECK(called == 2);
    for (const auto* input : {"throw", "bad", "role", "unsafe", "unsafe-description"}) {
        const auto reply = transport.request(id++, "prompts/get", {{"name", "review"}, {"arguments", {{"code", input}}}});
        CHECK(reply.at("error").at("code") == -32603);
        CHECK(reply.dump().find("private") == std::string::npos);
    }
    CHECK(filtered >= 3);
    CHECK_FALSE(wrong_thread);
    server.stop(); REQUIRE(server.wait());
}
