#pragma once

#include <synapse/errors.hpp>
#include <nlohmann/json.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#ifndef JSONCONS_HAS_STD_REGEX
#error "Synapse schema validation requires JSONCONS_HAS_STD_REGEX"
#endif

#include <cmath>
#include <expected>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace synapse::detail {
using schema_catalog = std::map<std::string,nlohmann::json>;
enum class validation_failure_kind { instance_mismatch, invalid_instance, engine_failure };
struct validation_failure {
    validation_failure_kind kind;
    std::string instance_pointer;
    std::string schema_pointer;
};

namespace schema_backend {
using json = jsoncons::json;
using evaluator = jsoncons::jsonschema::json_schema<json>;
namespace engine = jsoncons::jsonschema;

// A query (including URN r/q components) belongs to resource identity.
// jsoncons::uri::base() removes both query and fragment.
inline jsoncons::uri resource_uri(const jsoncons::uri& uri) {
    return jsoncons::uri(jsoncons::uri_encoded_part,uri.scheme(),uri.encoded_userinfo(),
        uri.host(),uri.port(),uri.encoded_path(),uri.encoded_query(),"");
}
inline engine::evaluation_options options() {
    engine::evaluation_options result;
    result.default_version(engine::schema_version::draft202012())
          .require_format_validation(false).compatibility_mode(false);
    return result;
}
inline bool representable(const nlohmann::json& value) {
    if (value.is_binary() || value.is_discarded()) return false;
    if (value.is_number_float() && !std::isfinite(value.get<double>())) return false;
    if (value.is_structured())
        for (const auto& child : value) if (!representable(child)) return false;
    return true;
}
inline json convert(const nlohmann::json& value) {
    // dump validates UTF-8 and preserves string/key lengths. Reject values that
    // serialization could silently change before calling this function.
    return json::parse(value.dump());
}
inline const json& bundled_metaschema() {
    static const json value = engine::meta_resolver<json>(
        jsoncons::uri(engine::schema_version::draft202012()));
    return value;
}
inline const evaluator& metaschema_validator() {
    static const evaluator value = engine::make_json_schema(bundled_metaschema(),options());
    return value;
}
inline const std::map<std::string,json>& builtin_resources() {
    static const auto resources = [] {
        std::map<std::string,json> result;
        const auto& root = bundled_metaschema();
        result.emplace(root.at("$id").as<std::string>(),root);
        for (const auto& component : root.at("allOf").array_range())
            result.emplace(component.at("$id").as<std::string>(),component);
        return result;
    }();
    return resources;
}
class policy_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Registration-only state: compiled evaluators own their resources, and never
// retain a reference to the application's catalog or to this resolver.
class registration {
    struct resource { const nlohmann::json* value; std::string base; };
    const schema_catalog& catalog_;
    std::map<jsoncons::uri,resource> resources_;
    std::map<jsoncons::uri,const nlohmann::json*> anchors_;
    std::map<std::string,json> converted_;
    std::set<const nlohmann::json*> inspected_;
    std::set<std::string> checked_dialects_,active_dialects_;
    std::vector<jsoncons::uri> references_;

    static bool supported_vocabulary(const std::string& uri) {
        constexpr std::string_view prefix = "https://json-schema.org/draft/2020-12/vocab/";
        for (const auto name : {"core","applicator","unevaluated","validation",
                               "meta-data","format-annotation","content"})
            if (uri == std::string(prefix)+name) return true;
        return false;
    }
    void check_dialect(const std::string& identifier) {
        const jsoncons::uri uri(identifier);
        if (!uri.is_absolute() || !uri.fragment().empty())
            throw policy_error("Unsupported schema dialect");
        const auto key = resource_uri(uri).string();
        if (key == engine::schema_version::draft202012() || checked_dialects_.contains(key)) return;
        if (!active_dialects_.insert(key).second)
            throw policy_error("Cyclic schema dialect declaration");
        const auto& meta = load_resource(key);
        if (!meta.is_object() || !meta.contains("$schema"))
            throw policy_error("Unsupported schema dialect");
        check_dialect(meta.at("$schema").get<std::string>());
        active_dialects_.erase(key);
        checked_dialects_.insert(key);
    }
    void inspect(const nlohmann::json& schema,std::string base) {
        if (!schema.is_object() || !inspected_.insert(&schema).second) return;
        if (schema.contains("$id")) {
            base = resource_uri(jsoncons::uri(base).resolve(
                jsoncons::uri(schema.at("$id").get<std::string>()))).string();
            // Retrieval aliases share the canonical resource's anchor namespace.
            for (auto& [alias,entry] : resources_)
                if (entry.value == &schema) entry.base = base;
            const auto [position,inserted] = resources_.emplace(jsoncons::uri(base),resource{&schema,base});
            if (!inserted && position->second.value != &schema)
                throw policy_error("Duplicate schema resource identifier");
        }
        if (schema.contains("$schema")) check_dialect(schema.at("$schema").get<std::string>());
        if (schema.contains("$vocabulary")) {
            for (const auto& [uri,required] : schema.at("$vocabulary").items())
                if (required.get<bool>() && !supported_vocabulary(uri))
                    throw policy_error("Unsupported required schema vocabulary");
        }
        for (const auto name : {"$anchor","$dynamicAnchor"})
            if (schema.contains(name))
                anchors_.emplace(jsoncons::uri(base+"#"+schema.at(name).get<std::string>()),&schema);
        for (const auto name : {"$ref","$dynamicRef"})
            if (schema.contains(name))
                references_.push_back(jsoncons::uri(base).resolve(jsoncons::uri(schema.at(name).get<std::string>())));
        // Literal const/enum/default/annotation objects are instance data.
        for (const auto name : {"$defs","definitions","properties","patternProperties","dependentSchemas"})
            if (schema.contains(name))
                for (const auto& child : schema.at(name)) inspect(child,base);
        for (const auto name : {"additionalProperties","unevaluatedProperties","propertyNames",
                "items","contains","unevaluatedItems","not","if","then","else","contentSchema"})
            if (schema.contains(name)) inspect(schema.at(name),base);
        for (const auto name : {"prefixItems","allOf","anyOf","oneOf"})
            if (schema.contains(name))
                for (const auto& child : schema.at(name)) inspect(child,base);
        if (schema.contains("dependencies"))
            for (const auto& child : schema.at("dependencies"))
                if (!child.is_array()) inspect(child,base);
    }
    void validate_schema(const nlohmann::json& schema) {
        if (!representable(schema) || !metaschema_validator().is_valid(convert(schema)))
            throw policy_error("Invalid schema document");
    }
    const nlohmann::json& load_resource(const std::string& key) {
        if (auto found = resources_.find(jsoncons::uri(key)); found != resources_.end()) return *found->second.value;
        const auto found = catalog_.find(key);
        if (found == catalog_.end()) throw policy_error("Unregistered offline schema resource");
        validate_schema(found->second);
        resources_.emplace(jsoncons::uri(key),resource{&found->second,key});
        converted_.emplace(key,convert(found->second));
        inspect(found->second,key);
        return found->second;
    }
    void inspect_reference(const jsoncons::uri& uri) {
        const auto key = resource_uri(uri).string();
        if (builtin_resources().contains(key)) return;
        const auto& document = load_resource(key);
        const auto fragment = uri.fragment();
        if (fragment.empty()) return;
        if (fragment.front() != '/') {
            if (!anchors_.contains(jsoncons::uri(resources_.at(jsoncons::uri(key)).base+"#"+fragment)))
                throw policy_error("Unregistered schema anchor");
            return;
        }
        // JSON Pointer references can turn unknown keyword values into schemas.
        const auto& target = document.at(nlohmann::json::json_pointer(fragment));
        if (!inspected_.contains(&target)) {
            validate_schema(target);
            inspect(target,resources_.at(jsoncons::uri(key)).base);
        }
    }
public:
    registration(const nlohmann::json& root,const schema_catalog& catalog) : catalog_(catalog) {
        validate_schema(root);
        const auto base = options().default_base_uri();
        resources_.emplace(jsoncons::uri(base),resource{&root,base});
        inspect(root,base);
        for (std::size_t index=0; index<references_.size(); ++index) {
            // Inspection may append references and reallocate the vector.
            const auto uri = references_[index];
            inspect_reference(uri);
        }
    }
    json resolve(const jsoncons::uri& uri) const {
        const auto key = resource_uri(uri).string();
        if (const auto found = builtin_resources().find(key); found != builtin_resources().end()) return found->second;
        if (const auto found = converted_.find(key); found != converted_.end()) return found->second;
        if (const auto found = resources_.find(jsoncons::uri(key)); found != resources_.end()) return convert(*found->second.value);
        return json::null();
    }
};
inline std::string diagnostic_pointer(std::string pointer) {
    if (!pointer.empty() && pointer.front() != '/') return {};
    for (const unsigned char ch : pointer) if (ch < 0x20 || ch == 0x7f) return {};
    return pointer;
}
} // namespace schema_backend

class compiled_schema {
    std::unique_ptr<const schema_backend::evaluator> evaluator_;
    explicit compiled_schema(schema_backend::evaluator&& evaluator)
        : evaluator_(std::make_unique<const schema_backend::evaluator>(std::move(evaluator))) {}
    friend std::expected<compiled_schema,sdk_error>
    compile_schema(const nlohmann::json&,const schema_catalog&);
public:
    compiled_schema(compiled_schema&&) noexcept = default;
    compiled_schema& operator=(compiled_schema&&) noexcept = default;
    compiled_schema(const compiled_schema&) = delete;
    compiled_schema& operator=(const compiled_schema&) = delete;

    std::expected<void,validation_failure> validate(const nlohmann::json& instance) const {
        using kind = validation_failure_kind;
        if (!evaluator_) return std::unexpected(validation_failure{kind::engine_failure,{},{}});
        try {
            if (!schema_backend::representable(instance))
                return std::unexpected(validation_failure{kind::invalid_instance,{},{}});
            std::string encoded;
            try { encoded = instance.dump(); }
            catch (const nlohmann::json::exception&) {
                return std::unexpected(validation_failure{kind::invalid_instance,{},{}});
            }
            const auto value = schema_backend::json::parse(encoded);
            bool mismatch = false;
            validation_failure failure{kind::instance_mismatch,{},{}};
            evaluator_->validate(value,[&](const jsoncons::jsonschema::validation_message& message) {
                mismatch = true;
                failure.instance_pointer = schema_backend::diagnostic_pointer(message.instance_location().string());
                auto schema_pointer = message.schema_location().fragment();
                // The pinned type validator reports the parent schema location.
                if (message.keyword() == "type") schema_pointer += "/type";
                failure.schema_pointer = schema_backend::diagnostic_pointer(std::move(schema_pointer));
                return jsoncons::jsonschema::walk_state::abort;
            });
            if (mismatch) return std::unexpected(std::move(failure));
            return {};
        } catch (const std::exception&) {
            return std::unexpected(validation_failure{kind::engine_failure,{},{}});
        }
    }
};

inline std::expected<compiled_schema,sdk_error>
compile_schema(const nlohmann::json& schema,const schema_catalog& catalog) {
    try {
        schema_backend::registration registration(schema,catalog);
        auto resolver = [&](const jsoncons::uri& uri) -> schema_backend::json { return registration.resolve(uri); };
        return compiled_schema(jsoncons::jsonschema::make_json_schema(
            schema_backend::convert(schema),resolver,schema_backend::options()));
    } catch (const schema_backend::policy_error& error) {
        return std::unexpected(sdk_error{sdk_errc::invalid_schema,error.what()});
    } catch (const std::exception&) {
        return std::unexpected(sdk_error{sdk_errc::invalid_schema,"Schema registration failed"});
    }
}
} // namespace synapse::detail
