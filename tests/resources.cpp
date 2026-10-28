#include <synapse/server.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <condition_variable>
#include <thread>
using json = nlohmann::json;
namespace {
struct transport {
 std::function<void(const json&)> message; std::function<void(synapse::close_reason)> closed;
 std::mutex mutex; std::condition_variable changed; std::vector<json> output; std::atomic<bool> stopped{false};
 std::expected<void,synapse::sdk_error> start(decltype(message) m, decltype(closed) c) { message=std::move(m); closed=std::move(c); return {}; }
 std::expected<void,synapse::sdk_error> send(const json& j) { std::lock_guard l(mutex); output.push_back(j); changed.notify_all(); return {}; }
 void stop() noexcept { if(!stopped.exchange(true) && closed) closed(synapse::close_reason::requested_stop); }
 std::expected<void,synapse::sdk_error> wait() { return {}; }
 std::expected<bool,synapse::sdk_error> wait_for(std::chrono::milliseconds) { return true; }
 json request(int id, const char* method, json params=json::object()) {
  message({{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",std::move(params)}});
  std::unique_lock l(mutex); if(!changed.wait_for(l,std::chrono::seconds(3),[&]{return !output.empty();})) return nullptr;
  auto j=output.front(); output.erase(output.begin()); return j;
 }
 void ready() { auto j=request(0,"initialize",{{"protocolVersion","2025-11-25"},{"capabilities",json::object()},{"clientInfo",{{"name","test"},{"version","1"}}}}); REQUIRE(j.contains("result")); CHECK(j["result"]["capabilities"]["resources"]==json::object()); message({{"jsonrpc","2.0"},{"method","notifications/initialized"}}); }
};
}
TEST_CASE("M1.resources.text_snapshot", "[M1][resources]") {
 transport t; std::atomic<int> calls{0}; std::atomic<bool> worker{false}, operation_ok{true}; int mode=0; auto owner=std::this_thread::get_id();
 synapse::server_options options;
 options.filter_output=[&](std::string_view op,std::string_view text,const synapse::request_context&) -> std::expected<std::string,synapse::handler_error> {
  if(op!="resources/read") operation_ok=false; worker=std::this_thread::get_id()!=owner;
  if(mode==3) return std::unexpected(synapse::handler_error{"private filter"});
  return text=="secret" ? std::string("redacted") : std::string(text);
 };
 synapse::server s(t,{},options);
 auto handler=[&](const synapse::request_context&) -> std::expected<synapse::resource_result,synapse::handler_error> {
  ++calls; if(mode==1) return std::unexpected(synapse::handler_error{"private handler"});
  if(mode==2) throw std::runtime_error("private exception");
  if(mode==4) return synapse::resource_result{std::string("bad\x1b")};
  return synapse::resource_result{"secret"};
 };
 REQUIRE(s.register_resource({"app://status","Status","A snapshot","text/plain"},handler));
 REQUIRE(s.register_resource({"app://plain","Plain","",std::nullopt},handler));
 CHECK_FALSE(s.register_resource({"app://status","Duplicate","",std::nullopt},handler));
 CHECK_FALSE(s.register_resource({"","Bad","",std::nullopt},handler));
 CHECK_FALSE(s.register_resource({"app://bad","Bad","",std::string{}},handler));
 REQUIRE(s.start()); CHECK_FALSE(s.register_resource({"app://late","Late","",std::nullopt},handler)); t.ready();
 auto list=t.request(1,"resources/list"); REQUIRE(list["result"]["resources"].size()==2);
 auto entries=list["result"]["resources"]; CHECK(entries[0]["uri"]=="app://plain"); CHECK_FALSE(entries[0].contains("mimeType"));
 CHECK(entries[1]["uri"]=="app://status"); CHECK(entries[1]["name"]=="Status"); CHECK(entries[1]["description"]=="A snapshot"); CHECK(entries[1]["mimeType"]=="text/plain");
 auto templates=t.request(2,"resources/templates/list"); REQUIRE(templates.contains("result")); CHECK(templates["result"]["resourceTemplates"]==json::array());
 auto read=t.request(3,"resources/read",{{"uri","app://status"}}); REQUIRE(read["result"]["contents"].size()==1); CHECK(read["result"]["contents"][0]==json{{"uri","app://status"},{"mimeType","text/plain"},{"text","redacted"}}); CHECK(worker); CHECK(operation_ok);
 auto plain=t.request(4,"resources/read",{{"uri","app://plain"}}); CHECK_FALSE(plain["result"]["contents"][0].contains("mimeType"));
 CHECK(t.request(5,"resources/read")["error"]["code"]==-32602); CHECK(t.request(6,"resources/read",{{"uri",42}})["error"]["code"]==-32602);
 CHECK(t.request(7,"resources/read",{{"uri","app://unknown"}})["error"]["code"]==-32002); CHECK(calls==2);
 CHECK(t.request(8,"resources/templates/list",{{"cursor",42}})["error"]["code"]==-32602);
 CHECK(t.request(9,"resources/list",{{"cursor",42}})["error"]["code"]==-32602);
 CHECK(t.request(20,"resources/subscribe",{{"uri","app://status"}})["error"]["code"]==-32601);
 for(mode=1;mode<=4;++mode) { auto err=t.request(10+mode,"resources/read",{{"uri","app://status"}}); CHECK(err["error"]["code"]==-32603); CHECK(err.dump().find("private")==std::string::npos); CHECK(err.dump().find("bad")==std::string::npos); }
 s.stop(); REQUIRE(s.wait());
}
