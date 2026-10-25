#include <jsoncons/nonstd/compiler_support.hpp>

// Exercise the same fail-closed guard required by the SDK's schema adapter.
#if defined(SYNAPSE_QUALIFICATION_DISABLE_REGEX)
#undef JSONCONS_HAS_STD_REGEX
#endif
#if !defined(JSONCONS_HAS_STD_REGEX) || !JSONCONS_HAS_STD_REGEX
#error "Synapse schema validation requires JSONCONS_HAS_STD_REGEX"
#endif

#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

static_assert(std::is_same_v<jsoncons::json::key_type, std::string>,
              "The qualified property lookup correction requires std::string keys");

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

auto compile(const jsoncons::json& schema) {
    const auto options = jsoncons::jsonschema::evaluation_options{}
        .default_version(jsoncons::jsonschema::schema_version::draft202012());
    return jsoncons::jsonschema::make_json_schema(schema, options);
}

void pattern_nul() {
    const auto schema = compile(jsoncons::json::parse(R"({"type":"string","pattern":"z"})"));
    require(schema.is_valid(jsoncons::json(std::string("a\0z", 3))),
            "pattern z must accept the full string a\\u0000z");
    require(!schema.is_valid(jsoncons::json(std::string("a\0x", 3))),
            "pattern z must still reject a string with no z");
    require(!schema.is_valid(jsoncons::json("a")),
            "pattern z must still reject an ordinary nonmatching string");
}

void pattern_properties_nul() {
    const auto schema = compile(jsoncons::json::parse(R"({
        "type":"object", "patternProperties":{"z":{"type":"integer"}},
        "additionalProperties":false
    })"));
    const std::string key("a\0z", 3);
    jsoncons::json instance = jsoncons::json::object();
    instance[key] = 7;
    require(schema.is_valid(instance), "patternProperties must match beyond a NUL in a key");
    instance[key] = "wrong";
    require(!schema.is_valid(instance), "patternProperties must validate the matched value");

    jsoncons::json exact_schema = jsoncons::json::object();
    exact_schema["type"] = "object";
    exact_schema["patternProperties"] = jsoncons::json::object();
    exact_schema["patternProperties"][key] = jsoncons::json::parse(R"({"type":"integer"})");
    exact_schema["additionalProperties"] = false;
    const auto exact = compile(exact_schema);
    instance[key] = 7;
    require(exact.is_valid(instance), "NUL-containing patternProperties patterns must retain their range");
    instance["a"] = 7;
    require(!exact.is_valid(instance), "a NUL-containing property pattern must not truncate to a");
}

jsoncons::json property_schema() {
    return jsoncons::json::parse(R"({
        "type":"object", "patternProperties":{"^a$|z":{"type":"integer"}}
    })");
}

void additional_properties_false_nul(const char* policy = "additionalProperties") {
    auto schema = property_schema();
    schema[policy] = false;
    const auto compiled = compile(schema);
    jsoncons::json instance = jsoncons::json::object();
    instance[std::string("a\0z", 3)] = 7;
    require(compiled.is_valid(instance), "false additionalProperties must retain a matched NUL key");
    instance[std::string("a\0q", 3)] = 7;
    require(!compiled.is_valid(instance), "false additionalProperties must reject an unmatched NUL key");
    instance = jsoncons::json::object();
    instance["a"] = 7;
    instance[std::string("a\0q", 3)] = 7;
    require(!compiled.is_valid(instance), "an allowed prefix must not hide an unmatched NUL key");

    // Absent and true additionalProperties keep extras allowed, while the
    // pattern's own value constraint still applies to matched NUL keys.
    for (bool explicit_true : {false, true}) {
        auto open_schema = property_schema();
        if (explicit_true) {
            open_schema[policy] = true;
        }
        const auto open = compile(open_schema);
        instance = jsoncons::json::object();
        instance[std::string("a\0z", 3)] = 7;
        instance[std::string("a\0q", 3)] = "allowed";
        require(open.is_valid(instance), "open properties must allow unmatched NUL keys");
        instance[std::string("a\0z", 3)] = "wrong";
        require(!open.is_valid(instance), "open properties must still validate matched NUL keys");
    }
}

void additional_properties_schema_nul(const char* policy = "additionalProperties") {
    auto schema = property_schema();
    schema[policy] = jsoncons::json::parse(R"({"type":"string"})");
    const auto compiled = compile(schema);
    jsoncons::json instance = jsoncons::json::object();
    instance[std::string("a\0z", 3)] = 7;
    require(compiled.is_valid(instance), "additional schema must not revalidate a matched NUL key");
    instance[std::string("a\0q", 3)] = "allowed";
    require(compiled.is_valid(instance), "additional schema must accept a conforming unmatched NUL key");
    instance[std::string("a\0q", 3)] = 7;
    require(!compiled.is_valid(instance), "additional schema must reject a wrong unmatched NUL value");
    instance = jsoncons::json::object();
    instance["a"] = 7;
    instance[std::string("a\0q", 3)] = 7;
    require(!compiled.is_valid(instance), "an allowed prefix must not bypass the additional schema");
}

void additional_properties_walk_nul() {
    auto schema = property_schema();
    schema["additionalProperties"] = jsoncons::json::parse(R"({"type":"string"})");
    const auto compiled = compile(schema);
    jsoncons::json instance = jsoncons::json::object();
    instance[std::string("a\0z", 3)] = 7;
    instance[std::string("a\0q", 3)] = "allowed";
    int matched_visits = 0;
    int additional_visits = 0;
    const auto reporter = [&](const std::string& keyword, const jsoncons::json& subschema,
                              const jsoncons::uri&, const jsoncons::json& value,
                              const jsoncons::jsonpointer::json_pointer&) {
        if (keyword == "type" && subschema.is_object()) {
            if (subschema.at("type") == "integer") {
                require(value.is_int64(), "walk must dispatch a matched NUL key to its integer schema");
                ++matched_visits;
            } else if (subschema.at("type") == "string") {
                require(value.is_string(), "walk must not dispatch a matched NUL key to additionalProperties");
                ++additional_visits;
            }
        }
        return jsoncons::jsonschema::walk_state::advance;
    };
    compiled.walk(instance, reporter);
    require(matched_visits == 1 && additional_visits == 1,
            "walk must visit each matched/unmatched NUL key's schema exactly once");

    instance = jsoncons::json::object();
    instance["a"] = 7;
    instance[std::string("a\0q", 3)] = "allowed";
    matched_visits = additional_visits = 0;
    compiled.walk(instance, reporter);
    require(matched_visits == 1 && additional_visits == 1,
            "walk must not confuse an unmatched NUL key with its allowed prefix");
}

void properties_validate_nul() {
    jsoncons::json schema = jsoncons::json::object();
    schema["type"] = "object";
    schema["properties"] = jsoncons::json::object();
    schema["properties"][std::string("a\0z", 3)] = jsoncons::json::parse(R"({"type":"integer"})");
    const auto compiled = compile(schema);
    jsoncons::json instance = jsoncons::json::object();
    instance[std::string("a\0z", 3)] = "wrong";
    require(!compiled.is_valid(instance), "properties must validate its full embedded-NUL name");
    instance[std::string("a\0z", 3)] = 7;
    require(compiled.is_valid(instance), "properties must accept a matching NUL key's valid value");
    instance[std::string("a\0q", 3)] = "allowed";
    require(compiled.is_valid(instance), "properties must leave an unmatched NUL key unconstrained");

    const auto prefix = compile(jsoncons::json::parse(R"({"properties":{"a":{"type":"integer"}}})"));
    instance = jsoncons::json::object();
    instance[std::string("a\0z", 3)] = "allowed";
    require(prefix.is_valid(instance), "properties must not apply a plain-prefix schema to a NUL key");
}

void properties_walk_nul() {
    jsoncons::json schema = jsoncons::json::object();
    schema["properties"] = jsoncons::json::object();
    schema["properties"][std::string("a\0z", 3)] = jsoncons::json::parse(R"({"type":"integer"})");
    schema["properties"]["a"] = jsoncons::json::parse(R"({"type":"string"})");
    const auto compiled = compile(schema);
    jsoncons::json instance = jsoncons::json::object();
    instance[std::string("a\0z", 3)] = 7;
    instance[std::string("a\0q", 3)] = true;
    int integer_visits = 0;
    int string_visits = 0;
    const auto reporter = [&](const std::string& keyword, const jsoncons::json& subschema,
                              const jsoncons::uri&, const jsoncons::json&,
                              const jsoncons::jsonpointer::json_pointer&) {
        if (keyword == "type" && subschema.is_object()) {
            integer_visits += subschema.at("type") == "integer";
            string_visits += subschema.at("type") == "string";
        }
        return jsoncons::jsonschema::walk_state::advance;
    };
    compiled.walk(instance, reporter);
    require(integer_visits == 1 && string_visits == 0,
            "properties walk must use full keys and leave unmatched NUL keys alone");
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "one qualification case name is required");
        const std::string name(argv[1]);
        if (name == "pattern_nul") {
            pattern_nul();
        } else if (name == "pattern_properties_nul") {
            pattern_properties_nul();
        } else if (name == "additional_properties_false_nul") {
            additional_properties_false_nul();
        } else if (name == "additional_properties_schema_nul") {
            additional_properties_schema_nul();
        } else if (name == "additional_properties_walk_nul") {
            additional_properties_walk_nul();
        } else if (name == "properties_validate_nul") {
            properties_validate_nul();
        } else if (name == "properties_walk_nul") {
            properties_walk_nul();
        } else if (name == "unevaluated_properties_false_nul") {
            additional_properties_false_nul("unevaluatedProperties");
        } else if (name == "unevaluated_properties_schema_nul") {
            additional_properties_schema_nul("unevaluatedProperties");
        } else {
            throw std::runtime_error("unknown qualification case");
        }
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
