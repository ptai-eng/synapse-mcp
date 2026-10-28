#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace {
using json = nlohmann::json;
struct transport {
 std::function<void(const json&)> message; std::function<void(synapse::close_reason)> closed;
 std::mutex mutex; std::condition_variable changed; std::vector<json> output; std::atomic<bool> stopped=false;
 std::expected<void,synapse::sdk_error> start(decltype(message) m, decltype(closed) c) { message=std::move(m); closed=std::move(c); return {}; }
 std::expected<void,synapse::sdk_error> send(const json& j) { std::lock_guard l(mutex); output.push_back(j); changed.notify_all(); return {}; }
 void stop() noexcept { if (!stopped.exchange(true)) { if(closed) closed(synapse::close_reason::requested_stop); } }
 std::expected<void,synapse::sdk_error> wait() { return {}; }
 std::expected<bool,synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
 json next() { std::unique_lock l(mutex); if(!changed.wait_for(l,std::chrono::seconds(3),[&]{return !output.empty();})) return nullptr; auto j=output.front(); output.erase(output.begin()); return j; }
};
json call(int id, const char* method, json params=json::object()) { return {{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",std::move(params)}}; }
void ready(transport& t) { t.message(call(0,"initialize",{{"protocolVersion","2025-11-25"},{"capabilities",json::object()},{"clientInfo",{{"name","test"},{"version","1"}}}})); REQUIRE(t.next()["result"]["capabilities"].contains("tools")); t.message({{"jsonrpc","2.0"},{"method","notifications/initialized"}}); }
}
TEST_CASE("M1.tools.validation_and_results", "[M1][tools]") {
 transport t; std::atomic<int> count=0; std::thread::id worker; synapse::server s(t);
 synapse::tool desc{"echo","Echo",{{"type","object"},{"properties",{{"value",{{"type","string"}}}}},{"additionalProperties",false}}};
 auto handler=[&](const json& args,const synapse::request_context&) -> std::expected<synapse::tool_result,synapse::handler_error> { ++count; worker=std::this_thread::get_id(); if(args.value("value","")=="domain") return std::unexpected(synapse::handler_error{"public error"}); if(args.value("value","")=="throw") throw std::runtime_error("secret exception"); return synapse::tool_result{args.value("value","empty")}; };
 REQUIRE(s.register_tool(desc,handler)); CHECK_FALSE(s.register_tool(desc,handler));
 auto bad=desc; bad.name="bad"; bad.inputSchema={{"type","array"}}; CHECK_FALSE(s.register_tool(bad,handler));
 bad.inputSchema={{"type","object"},{"required",7}}; CHECK_FALSE(s.register_tool(bad,handler));
 REQUIRE(s.start()); CHECK_FALSE(s.register_tool({"late","",{{"type","object"}}},handler)); ready(t);
 t.message(call(1,"tools/list")); auto list=t.next(); REQUIRE(list["result"]["tools"].size()==1); CHECK(list["result"]["tools"][0]["name"]=="echo");
 t.message(call(2,"tools/call",{{"name","echo"}})); CHECK(t.next()["result"]["content"][0]["text"]=="empty"); CHECK(worker!=std::this_thread::get_id());
 t.message(call(3,"tools/call",{{"name","echo"},{"arguments",{{"value","hello"}}}})); CHECK(t.next()["result"]["content"][0]["text"]=="hello");
 for (auto p : {json{{"name","missing"}}, json{{"name","echo"},{"arguments",42}}, json{{"name",42}}}) { t.message(call(4,"tools/call",p)); CHECK(t.next()["error"]["code"]==-32602); }
 CHECK(count==2);
 t.message(call(5,"tools/call",{{"name","echo"},{"arguments",{{"value",123},{"secret","SECRET_INSTANCE"}}}})); auto mismatch=t.next(); CHECK(mismatch["result"]["isError"]==true); CHECK(mismatch.dump().find("SECRET_INSTANCE")==std::string::npos); CHECK(count==2);
 t.message(call(6,"tools/call",{{"name","echo"},{"arguments",{{"value","domain"}}}})); auto domain=t.next(); CHECK(domain["result"]["isError"]==true); CHECK(domain["result"]["content"][0]["text"]=="public error");
 t.message(call(7,"tools/call",{{"name","echo"},{"arguments",{{"value","throw"}}}})); auto err=t.next(); CHECK(err["error"]["code"]==-32603); CHECK(err.dump().find("secret exception")==std::string::npos);
 s.stop(); REQUIRE(s.wait());
}
