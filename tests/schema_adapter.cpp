#include <catch2/catch_test_macros.hpp>
#include <synapse/detail/schema_validator.hpp>

#include <algorithm>
#include <future>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using nlohmann::json;
using namespace synapse::detail;
const std::string dialect = "https://json-schema.org/draft/2020-12/schema";

void rejects_schema(const json& schema, const schema_catalog& catalog = {}) {
    auto result = compile_schema(schema, catalog);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == synapse::sdk_errc::invalid_schema);
    CHECK_FALSE(result.error().message.empty());
    CHECK(result.error().message.find("PRIVATE") == std::string::npos);
}
void check_instance(const json& schema, const json& instance, bool valid,
                    const schema_catalog& catalog = {}) {
    auto compiled = compile_schema(schema, catalog);
    REQUIRE(compiled);
    const auto before = instance.dump();
    auto result = compiled->validate(instance);
    CHECK(static_cast<bool>(result) == valid);
    if (!result) CHECK(result.error().kind == validation_failure_kind::instance_mismatch);
    CHECK(instance.dump() == before);
}
json read_json(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot read schema suite data");
    return json::parse(input);
}
}

TEST_CASE("M1.schema.registration_and_validation", "[M1][schema][adapter]") {
    SECTION("registration uses the bundled metaschema, not compile success") {
        for (const auto& schema : std::vector<json>{42, nullptr, "PRIVATE",
                 json{{"type",42}}, json{{"minimum","PRIVATE"}},
                 json{{"required",json::array({"a","a"})}},
                 json{{"properties",{{"x",{{"type",42}}}}}}}) rejects_schema(schema);
        rejects_schema(json{{"pattern","[PRIVATE"}});
        check_instance(true, json{{"anything",1}}, true);
        check_instance(false, 1, false);
    }
    SECTION("absent and explicit dialects preserve NUL string matching") {
        const std::string nul("a\0z",3);
        for (bool explicit_dialect : {false,true}) {
            json schema{{"type","string"},{"pattern","z"}};
            if (explicit_dialect) schema["$schema"] = dialect;
            check_instance(schema,nul,true);
            check_instance(schema,std::string("a\0x",3),false);
        }
    }
    SECTION("unsupported dialects are rejected at every schema resource") {
        for (const auto& uri : {std::string("http://json-schema.org/draft-07/schema#"),
                std::string("https://json-schema.org/draft/2019-09/schema"),
                std::string("https://PRIVATE.invalid/dialect"),dialect+std::string("\0PRIVATE",8)})
            rejects_schema(json{{"$schema",uri},{"pattern","z"}});
        rejects_schema(json{{"$defs",{{"x",{{"$schema","https://PRIVATE.invalid/dialect"}}}}}});
        rejects_schema(json{{"$ref","https://test.invalid/a"}},
            {{"https://test.invalid/a",json{{"$schema","http://json-schema.org/draft-07/schema#"}}}});
        rejects_schema(json{{"$ref","#/unknown"},{"unknown",{{"$schema","https://PRIVATE.invalid/dialect"}}}});
        check_instance(json{{"const",{{"$schema","https://PRIVATE.invalid/data"}}}},
            json{{"$schema","https://PRIVATE.invalid/data"}},true);
    }
    SECTION("offline references own catalog data after registration") {
        schema_catalog catalog{{"https://test.invalid/value",json{{"type","integer"}}}};
        auto compiled = compile_schema(json{{"$ref","https://test.invalid/value"}},catalog);
        REQUIRE(compiled);
        catalog.clear();
        CHECK(compiled->validate(1));
        CHECK_FALSE(compiled->validate("PRIVATE"));
        check_instance(json{{"$defs",{{"n",{{"type","integer"}}}}},{"$ref","#/$defs/n"}},2,true);
        rejects_schema(json{{"$ref","https://PRIVATE.invalid/missing"}});
        rejects_schema(json{{"$ref","file:///PRIVATE/missing.json"}});
        rejects_schema(json{{"$ref","https://test.invalid/bad"}},
            {{"https://test.invalid/bad",json{{"type",42}}}});
    }
    SECTION("dynamic references recurse through registered resources") {
        const json tree{{"$id","https://test.invalid/tree"},{"$dynamicAnchor","node"},
            {"type","object"},{"properties",{{"children",{{"type","array"},
                {"items",{{"$dynamicRef","#node"}}}}}}}};
        check_instance(json{{"$ref","https://test.invalid/tree"}},
            json{{"children",json::array({json::object()})}},true,
            {{"https://test.invalid/tree",tree}});
        check_instance(json{{"$ref","https://test.invalid/tree"}},
            json{{"children",json::array({42})}},false,{{"https://test.invalid/tree",tree}});
        rejects_schema(json{{"$dynamicRef","https://PRIVATE.invalid/missing#node"}});
    }
    SECTION("required vocabularies and derived dialects are checked offline") {
        const std::string unknown = "https://PRIVATE.invalid/vocabulary";
        rejects_schema(json{{"$vocabulary",{{unknown,true}}}});
        rejects_schema(json{{"$vocabulary",{{"https://json-schema.org/draft/2020-12/vocab/format-assertion",true}}}});
        check_instance(json{{"$vocabulary",{{unknown,false}}},{"type","integer"}},1,true);
        const std::string custom = "https://test.invalid/meta";
        json meta{{"$schema",dialect},{"$id",custom},{"$vocabulary",{
            {"https://json-schema.org/draft/2020-12/vocab/core",true},
            {"https://json-schema.org/draft/2020-12/vocab/validation",true},{unknown,false}}}};
        check_instance(json{{"$schema",custom},{"type","integer"}},1,true,{{custom,meta}});
        meta["$vocabulary"][unknown] = true;
        rejects_schema(json{{"$schema",custom}},{{custom,meta}});
        rejects_schema(json{{"$ref",custom}},{{custom,meta}});
        rejects_schema(json{{"$schema",custom}},{{custom,json{{"$schema",custom}}}});
    }
    SECTION("prefix items, unevaluated data and branches keep engine semantics") {
        const json items{{"prefixItems",json::array({json{{"type","integer"}}})},
                         {"unevaluatedItems",false}};
        check_instance(items,json::array({1}),true);
        check_instance(items,json::array({1,2}),false);
        const json properties{{"allOf",json::array({json{{"properties",{{"a",true}}}}})},
                              {"unevaluatedProperties",false}};
        check_instance(properties,json{{"a",1}},true);
        check_instance(properties,json{{"a",1},{"b",2}},false);
        check_instance(json{{"oneOf",json::array({json{{"type","integer"}},json{{"minimum",0}}})}},1,false);
        check_instance(json{{"anyOf",json::array({json{{"type","integer"}},json{{"type","string"}}})}},"a",true);
        check_instance(json{{"if",{{"type","integer"}}},{"then",{{"minimum",2}}},{"else",{{"type","string"}}}},1,false);
    }
    SECTION("NUL property keys never alias their prefixes") {
        const std::string key("a\0z",3);
        check_instance(json{{"patternProperties",{{"z",{{"type","integer"}}}}},
                            {"additionalProperties",false}},json{{key,1}},true);
        check_instance(json{{"properties",{{"a",{{"type","integer"}}}}},
                            {"additionalProperties",false}},json{{key,1}},false);
        check_instance(json{{"properties",{{key,{{"type","integer"}}}}},
                            {"additionalProperties",false}},json{{key,1}},true);
        check_instance(json{{"properties",{{"a",true}}},{"unevaluatedProperties",false}},
                            json{{"a",1},{key,1}},false);
        check_instance(json{{"patternProperties",{{"z",true}}},{"unevaluatedProperties",false}},
                            json{{key,1}},true);
    }
    SECTION("format is annotation and defaults never mutate input") {
        check_instance(json{{"format","email"}},"not an email",true);
        check_instance(json{{"properties",{{"x",{{"default",42}}}}}},json::object(),true);
        const auto large = std::numeric_limits<std::uint64_t>::max();
        check_instance(json{{"const",large}},json(large),true);
    }
    SECTION("unrepresentable SDK values cannot silently become wire JSON") {
        auto compiled = compile_schema(true,{});
        REQUIRE(compiled);
        std::vector<json> invalid{std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),
            json::binary({0,1,255}),json::parse("[",nullptr,false),std::string(1,char(0xff))};
        invalid.push_back(json{{"nested",json::array({json::binary({1})})}});
        invalid.push_back(json{{std::string(1,char(0xff)),1}});
        for (const auto& value : invalid) {
            auto result = compiled->validate(value);
            REQUIRE_FALSE(result);
            CHECK(result.error().kind == validation_failure_kind::invalid_instance);
            CHECK(result.error().instance_pointer.empty());
            CHECK(result.error().schema_pointer.empty());
            rejects_schema(json{{"const",value}});
            rejects_schema(json{{"$ref","https://test.invalid/invalid"}},
                {{"https://test.invalid/invalid",json{{"const",value}}}});
        }
    }
    SECTION("diagnostics are typed and contain no backend instance messages") {
        auto compiled = compile_schema(json{{"type","integer"}},{});
        REQUIRE(compiled);
        auto result = compiled->validate("PRIVATE instance");
        REQUIRE_FALSE(result);
        CHECK(result.error().kind == validation_failure_kind::instance_mismatch);
        CHECK(result.error().instance_pointer.empty());
        CHECK(result.error().schema_pointer == "/type");
        auto owned = std::move(*compiled);
        result = compiled->validate(1);
        REQUIRE_FALSE(result);
        CHECK(result.error().kind == validation_failure_kind::engine_failure);
        CHECK(owned.validate(1));
    }
}


TEST_CASE("M1.schema.unicode_evaluation", "[M1][schema][adapter]") {
    const std::string greek = "\xcf\x80", cjk = "\xe4\xb8\xad", supplementary = "\xf0\x90\x90\x80";
    const json letters{{"pattern",R"(^\p{Letter}+$)"}};
    for (const auto& value : {std::string("Hello"),greek,cjk,supplementary}) check_instance(letters,value,true);
    check_instance(letters,"42",false);
    check_instance(json{{"pattern",R"(^\p{Script=Greek}+$)"}},greek,true);
    check_instance(json{{"pattern",R"(^\p{General_Category=Lu}+$)"}},"ABC",true);
    for (const auto pattern : {R"(^\P{Letter}+$)",R"(^[^\p{Letter}]+$)"}) {
        check_instance(json{{"pattern",pattern}},"123",true);
        check_instance(json{{"pattern",pattern}},greek,false);
    }
    check_instance(json{{"pattern",R"(^[\p{Letter}0-9]{2}$)"}},greek+"2",true);
    check_instance(json{{"pattern","^.$"}},supplementary,true);
    check_instance(json{{"pattern","^.$"}},std::string("e\xcc\x81"),false);
    check_instance(json{{"pattern","^.{2}$"}},greek+supplementary,true);
    check_instance(json{{"pattern",R"(^\u{10400}$)"}},supplementary,true);
    check_instance(json{{"pattern",R"(^\uD801\uDC00$)"}},supplementary,true);
    check_instance(json{{"pattern",R"(^[\u03B1-\u03C9]+$)"}},greek,true);
    check_instance(json{{"pattern",R"(^(\p{Letter})\1$)"}},greek+greek,true);
    check_instance(json{{"pattern",R"(^(?=\p{Letter})(?!\d).$)"}},greek,true);
    check_instance(json{{"pattern",R"(\p{Letter})"}},"12"+greek+"34",true);
    check_instance(json{{"pattern",R"(^\\p\{Letter\}$)"}},R"(\p{Letter})",true);
    const std::string nul("a\0",2);
    check_instance(json{{"pattern",R"(\p{Letter}$)"}},nul+greek,true);
    check_instance(json{{"pattern",nul+greek}},nul+greek,true);
    check_instance(json{{"pattern",nul+greek}},"a",false);
    const auto key = nul+greek;
    const json object_schema{{"patternProperties",{{R"(\p{Letter}$)",{{"type","integer"}}},
        {"^a",{{"minimum",2}}}}},{"unevaluatedProperties",false}};
    check_instance(object_schema,json{{key,2}},true);
    check_instance(object_schema,json{{key,1}},false);
    check_instance(object_schema,json{{key,"PRIVATE"}},false);
    check_instance(object_schema,json{{key,2},{"42",2}},false);
    for (const auto pattern : {"[PRIVATE","(","\\","[z-a]","a{3,2}",R"(\p{PRIVATE})",
             R"(\p{Script=PRIVATE})",R"(\p{Letter)",R"(\u{XYZ})"})
        rejects_schema(json{{"pattern",pattern}});
    check_instance(json{{"anyOf",json::array({letters,json{{"const",42}}})}},greek,true);
    check_instance(json{{"oneOf",json::array({letters,json{{"type","string"}}})}},greek,false);
    check_instance(json{{"not",letters}},greek,false);
    check_instance(json{{"allOf",json::array({letters,json{{"maxLength",1}}})}},greek,true);
    check_instance(json{{"if",letters},{"then",{{"maxLength",1}}},{"else",{{"const","12"}}}},greek+greek,false);
    check_instance(json{{"if",letters},{"then",{{"maxLength",1}}},{"else",{{"const","12"}}}},"12",true);
    check_instance(json{{"contains",letters}},json::array({"42",greek}),true);
    check_instance(json{{"propertyNames",letters}},json{{greek,1}},true);
    check_instance(json{{"properties",{{"x",letters}}}},json{{"x","42"}},false);
    check_instance(json{{"$defs",{{"letters",letters}}},{"$ref","#/$defs/letters"}},greek,true);
    check_instance(json{{"$ref","https://test.invalid/letters"}},greek,true,
        {{"https://test.invalid/letters",letters}});
}

TEST_CASE("M1.schema.fragment_resource_lifetime", "[M1][schema][adapter]") {
    for (int entry_kind : {0,1,2}) {
        const std::string entry = entry_kind ? "#entry" : "#/$defs/entry";
        json resource = json::parse(R"({
          "$id":"https://test.invalid/canonical", "const":false,
          "$defs":{
            "entry":{"$anchor":"entry", "$ref":"nested#/$defs/entry"},
            "length":{"$dynamicAnchor":"length","maxLength":2},
            "nested":{"$id":"nested", "const":false,"$defs":{
              "entry":{"$dynamicRef":"#length"},
              "length":{"$dynamicAnchor":"length","maxLength":3}}}
          }})");
        if (entry_kind == 2) {
            resource["$defs"]["entry"].erase("$anchor");
            resource["$defs"]["entry"]["$dynamicAnchor"] = "entry";
        }
        // Retrieval alias differs from canonical identity; the root itself has no $id.
        schema_catalog catalog{{"https://test.invalid/alias",resource}};
        json root{{"$ref","https://test.invalid/alias"+entry}};
        auto compiled = compile_schema(root,catalog);
        REQUIRE(compiled);
        catalog.clear(); resource = nullptr; root = nullptr;
        CHECK(compiled->validate("hi"));
        CHECK_FALSE(compiled->validate("hey"));
        auto owned = std::move(*compiled);
        CHECK(owned.validate("hi"));
        CHECK_FALSE(owned.validate("hey"));
    }
    // A nested resource's anchors never enter an enclosing resource's dictionary.
    auto schema = json::parse(R"({"$id":"https://test.invalid/base","$ref":"#/$defs/entry",
      "$defs":{"entry":{"$dynamicRef":"target#length"},
        "unvisited":{"$id":"other","$dynamicAnchor":"length","maxLength":1},
        "target":{"$id":"target","$dynamicAnchor":"length","maxLength":3}}})");
    check_instance(schema,"hey",true);
    check_instance(schema,"four",false);
    // A dynamicRef initially resolving an ordinary anchor stays static.
    schema = json::parse(R"({"$id":"https://test.invalid/base","$dynamicAnchor":"length",
      "$ref":"#/$defs/entry","$defs":{"entry":{"$dynamicRef":"target#length"},
        "target":{"$id":"target","$anchor":"length","maxLength":3}}})");
    check_instance(schema,"hey",true);
    check_instance(schema,"four",false);
}



TEST_CASE("M1.schema.dynamic_branch_isolation", "[M1][schema][adapter]") {
    auto schema = json::parse(R"({"$id":"https://test.invalid/base",
      "anyOf":[{"$ref":"strict#/$defs/entry"},{"$ref":"loose#/$defs/entry"}],
      "$defs":{
        "strict":{"$id":"strict","$defs":{"entry":{"$ref":"common#entry"},
          "length":{"$dynamicAnchor":"length","maxLength":1}}},
        "loose":{"$id":"loose","$defs":{"entry":{"$ref":"common#entry"},
          "length":{"$dynamicAnchor":"length","maxLength":3}}},
        "common":{"$id":"common","$defs":{"entry":{"$anchor":"entry","$dynamicRef":"#length"},
          "length":{"$dynamicAnchor":"length","maxLength":2}}}}})");
    check_instance(schema,"hey",true);
    check_instance(schema,"four",false);
    schema["oneOf"] = schema["anyOf"];
    schema.erase("anyOf");
    check_instance(schema,"hey",true);
    check_instance(schema,"a",false);
}

TEST_CASE("M1.schema.resource_query_identity", "[M1][schema][adapter]") {
    const std::string first = "https://test.invalid/resource?kind=letter";
    const std::string second = "https://test.invalid/resource?kind=integer";
    schema_catalog catalog{
        {first,json{{"$id",first},{"$defs",{{"entry",{{"$anchor","entry"},{"pattern",R"(^\p{Letter}+$)"}}}}}}},
        {second,json{{"$id",second},{"$defs",{{"entry",{{"$anchor","entry"},{"type","integer"}}}}}}}
    };
    const json root{{"type","object"},{"properties",{
        {"letters",{{"$ref",first+"#entry"}}},{"number",{{"$ref",second+"#entry"}}}}}};
    check_instance(root,json{{"letters","abc"},{"number",42}},true,catalog);
    check_instance(root,json{{"letters","42"},{"number",42}},false,catalog);
    check_instance(root,json{{"letters","abc"},{"number","abc"}},false,catalog);
    for (const auto& uri : {std::string("urn:example:letters?+CCResolve:cc=uk"),
                           std::string("urn:example:letters?=kind=letter")}) {
        check_instance(json{{"$ref",uri+"#/$defs/entry"}},"abc",true,
            {{uri,json{{"$id",uri},{"$defs",{{"entry",{{"pattern",R"(^\p{Letter}+$)"}}}}}}}});
    }
    check_instance(json{{"$ref","https://test.invalid/no-id?kind=integer#/$defs/entry"}},42,true,
        {{"https://test.invalid/no-id?kind=integer",json{{"$defs",{{"entry",{{"type","integer"}}}}}}}});
    auto compiled = compile_schema(root,catalog);
    REQUIRE(compiled);
    catalog.clear();
    std::vector<std::future<bool>> validations;
    for (int worker=0; worker<4; ++worker)
        validations.push_back(std::async(std::launch::async,[&] {
            for (int repeat=0; repeat<30; ++repeat)
                if (!compiled->validate(json{{"letters","abc"},{"number",42}}) ||
                    compiled->validate(json{{"letters","42"},{"number",42}})) return false;
            return true;
        }));
    for (auto& validation : validations) CHECK(validation.get());
}


TEST_CASE("M1.schema.query_dialect_identity", "[M1][schema][adapter]") {
    const auto supported_meta = [](const std::string& identifier) {
        return json{{"$schema",dialect},{"$id",identifier},{"$vocabulary",{
            {"https://json-schema.org/draft/2020-12/vocab/core",true},
            {"https://json-schema.org/draft/2020-12/vocab/validation",true}}}};
    };
    const std::string first="https://test.invalid/meta?v=1", second="https://test.invalid/meta?v=2";
    schema_catalog catalog{{first,supported_meta(first)},{second,supported_meta(second)}};
    for (const auto& identifier : {first,second}) {
        check_instance(json{{"$schema",identifier},{"type","integer"}},42,true,catalog);
        check_instance(json{{"$schema",identifier},{"type","integer"}},"PRIVATE",false,catalog);
        const json nested{{"$id","https://test.invalid/root"},{"$defs",{
            {"a",{{"type","number"}}},
            {"b",{{"$id","https://test.invalid/child"},{"$schema",identifier},{"type","integer"}}}}},
            {"$ref","https://test.invalid/child"}};
        check_instance(nested,42,true,catalog);
        check_instance(nested,"PRIVATE",false,catalog);
    }
    // Distinct queries must not resolve to a queryless or sibling dialect.
    catalog[second]["$vocabulary"]["https://PRIVATE.invalid/vocabulary"] = true;
    check_instance(json{{"$schema",first},{"type","integer"}},42,true,catalog);
    rejects_schema(json{{"$schema",second},{"type","integer"}},catalog);
    rejects_schema(json{{"$schema","https://test.invalid/meta?v=missing"}},catalog);
    rejects_schema(json{{"$schema","https://test.invalid/meta"}},catalog);
}

TEST_CASE("M1.schema.official_draft2020_12", "[M1][schema][schema-suite]") {
    namespace fs = std::filesystem;
    const fs::path suite(SYNAPSE_SCHEMA_TEST_SUITE_DIR);
    schema_catalog catalog;
    for (const auto& entry : fs::recursive_directory_iterator(suite/"remotes")) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            catalog.emplace("http://localhost:1234/"+
                fs::relative(entry.path(),suite/"remotes").generic_string(),read_json(entry.path()));
    }
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(suite/"tests"/"draft2020-12"))
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    std::sort(files.begin(),files.end());
    std::size_t groups=0,cases=0,failures=0;
    for (const auto& file : files) {
        for (const auto& group : read_json(file)) {
            ++groups;
            auto compiled = compile_schema(group.at("schema"),catalog);
            for (const auto& test : group.at("tests")) {
                ++cases;
                INFO(file.filename().string() << " :: " << group.at("description").get<std::string>()
                    << " :: " << test.at("description").get<std::string>());
                if (!compiled) {
                    ++failures;
                    INFO("registration failed: " << compiled.error().message);
                    CHECK(static_cast<bool>(compiled));
                    continue;
                }
                auto result = compiled->validate(test.at("data"));
                const bool valid = test.at("valid").get<bool>();
                if (static_cast<bool>(result) != valid) ++failures;
                CHECK(static_cast<bool>(result) == valid);
                if (!result) CHECK(result.error().kind == validation_failure_kind::instance_mismatch);
            }
        }
    }
    std::cout << "Official draft2020-12: files=" << files.size() << " groups=" << groups
              << " cases=" << cases << " failures=" << failures << " skipped=0\n";
    CHECK(files.size() == 46);
    CHECK(groups == 384);
    CHECK(cases == 1302);
}
