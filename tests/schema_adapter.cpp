#include <catch2/catch_test_macros.hpp>
#include <synapse/detail/schema_validator.hpp>

#include <algorithm>
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
