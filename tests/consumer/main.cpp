#include <expected>
#include <string>

#include <jsoncons/json.hpp>
#include <synapse/protocol.hpp>
#include <synapse/detail/schema_validator.hpp>

static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR == 11
              && NLOHMANN_JSON_VERSION_PATCH == 3);

int main() {
    std::expected<synapse::request, std::string> request{synapse::request{}};
    request->id = std::string{"consumer"};
    request->method = "ping";
    const nlohmann::json message = *request;
    const jsoncons::json dependency = jsoncons::json::object();
    const auto compiled = synapse::detail::compile_schema(
        nlohmann::json{{"pattern", R"(^\p{Letter}+$)"}}, {});
    if (!compiled || !compiled->validate("\xcf\x80") || compiled->validate("42")) return 2;
    return dependency.is_object() && message.at("jsonrpc") == "2.0"
                   && message.at("method") == "ping" ? 0 : 1;
}
