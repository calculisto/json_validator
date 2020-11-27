#pragma once
#include "detail/draft-07-schema.hpp"

// XXX fix for https://github.com/taocpp/json/issues/83
#include <tao/json.hpp>
#define TAO_PEGTL_DEMANGLE_HPP
#include <isto/uri/uri.hpp>

#include <fmt/ranges.h>
    using fmt::format;

#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <set>

// Make tao::json::value printable.
    template <>
    struct 
fmt::formatter <tao::json::value>
    : formatter <std::string> 
{
        template <typename FormatContext>
        auto 
    format (const tao::json::value& v, FormatContext &ctx) 
    {
        return formatter <std::string>::format (tao::json::to_string (v), ctx);
    }
};
    template <>
    struct 
fmt::formatter <tao::json::type>
    : formatter <std::string> 
{
        template <typename FormatContext>
        auto 
    format (const tao::json::type& t, FormatContext &ctx) 
    {
        return formatter <std::string>::format (tao::json::to_string (t), ctx);
    }
};

    namespace 
isto::json_validator
{
    using
json_t = tao::json::value;
    using 
schema_t = json_t;
    using
instance_t = json_t;
    using
uri_t = isto::uri::uri_t;

    namespace
detail // {{{
{
        bool
    type_match (const json_t& schema, const json_t& instance)
    {
            auto
        string = schema.get_string ();
        if (string == "null")    { return instance.is_null        (); }
        if (string == "boolean") { return instance.is_boolean     (); }
        if (string == "object")  { return instance.is_object      (); }
        if (string == "array")   { return instance.is_array       (); }
        if (string == "number")  { return instance.is_number      (); }
        if (string == "string")  { return instance.is_string_type (); }
        if (string == "integer") { return instance.is_integer     (); }
        throw std::runtime_error ("internal error");
    }
} // }}} namespace detail

    class
validator_t
{
private: 
        std::set <schema_t>
    schemas_m;
        const schema_t*
    last_schema_m = nullptr;
        const schema_t*
    meta_schema_m = nullptr;
        std::unordered_map <std::string, const schema_t*>
    registered_schemas_m;
        std::unordered_map <const json_t*, const schema_t*>
    registered_references_m;
        std::unordered_set <const schema_t*>
    analysed_schemas_m;

        auto
    add_meta_schema () 
        -> const schema_t*
    {
            auto&&
        [ it, inserted ] = schemas_m.insert (
            tao::json::from_string (detail::draft_07_schema)
        );
            auto const&
        schema = *it;
        if (inserted)
        {
            meta_schema_m = &schema;
        }
        register_schema (schema, "http://json-schema.org/draft-07/schema");
        analyse (
              schema
            , "http://json-schema.org/draft-07/schema"
        );
        return &schema;
    }

        auto
    add_schema_impl (json_t const& json, uri_t const& document_uri)
        -> const schema_t*
    {
            auto&&
        [ it_schema, inserted ] = schemas_m.insert (json);
        if (inserted)
        {
            register_schema (*it_schema, document_uri);
            analyse (
                  *it_schema
                , document_uri
            );

        }
        return &*it_schema;
    }

        auto
    add_schema_impl (json_t&& json, uri_t const& document_uri)
        -> const schema_t*
    {
            auto&&
        [ it_schema, inserted ] = schemas_m.insert (std::move (json));
        if (inserted)
        {
            register_schema (*it_schema, document_uri);
            analyse (
                  *it_schema
                , document_uri
            );
        }
        return &*it_schema;
    }

        auto
    register_schema (schema_t const& schema, uri_t const& uri)
        -> void
    {
        registered_schemas_m[uri.string ()] = &schema;
    }

        auto
    register_reference (json_t const& reference, schema_t const& schema)
        -> void
    {
        registered_references_m[&reference] = &schema;
    }

        auto
    resolve_reference (uri_t const& target)
        -> std::pair <const schema_t*, uri_t>
    {
            auto
        absolute = target.absolute ();
        if ( 
                auto&& 
              i = registered_schemas_m.find (absolute)
            ; i != registered_schemas_m.end ()
        ){
                auto&
            root_schema = i->second;
            try
            {
                    tao::json::pointer
                pointer { uri::decode_percent (target.fragment ()) };
                return { &(root_schema->at (pointer)), absolute };
            }
            catch (std::exception const& e)
            {
                throw (std::runtime_error { format (
                      "{}:{}: Resolution of pointer {} in schema {} failed: {}."
                    , __FILE__
                    , __LINE__
                    , target.fragment ()
                    , absolute.string ()
                    , e.what ()
                )});
            }      
        }
        throw (std::runtime_error { format (
              "Couldn't resolve reference {}."
            , target.string ()
        )});
    }

        auto
    resolve_reference (std::string const& ref, uri_t const& base_uri)
        -> std::pair <const schema_t*, uri_t>
    {
            auto
        target = base_uri.resolve (ref);
        if (
                auto&& 
              i = registered_schemas_m.find (target)
            ; i != registered_schemas_m.end ()
        ){
            return { i->second, base_uri };
        }
        return resolve_reference (target);
    }

        void
    analyse (schema_t const& schema, uri_t const& current_base_uri)
    {
        if (schema.is_boolean ()) return; 
        if (!schema.is_object ())
        {
            throw (std::runtime_error { format (
                  "{}:{}: Schema \"{}\" is not an object."
                , __FILE__
                , __LINE__
                , to_string (schema)
            )});
        }
        analysed_schemas_m.insert (&schema);
            auto const&
        schema_object = schema.get_object ();
            auto
        base_uri = current_base_uri;
            json_t::object_t::const_iterator
        it;
        if (
              it = schema_object.find ("$id")
            ; it != schema_object.end ()
        ){
                auto&
            id = it->second.get_string ();
            base_uri = base_uri.resolve (id);
            register_schema (schema, base_uri);
        }
        if (
              it = schema_object.find ("$ref")
            ; it != schema_object.end ()
                && registered_references_m.count (&(it->second)) == 0
        ){
                auto&
            ref = it->second;
                auto&
            ref_string = ref.get_string ();
                auto
            [ schema, uri ] = resolve_reference (ref_string, base_uri);
            register_reference (ref, *schema);
            // Some references might point to places whe do not analyse.
            if (analysed_schemas_m.count (schema) == 0) 
            {
                analyse (*schema, uri);
            }
        }
        for (auto&& [name, value]: schema_object)
        {
                const std::unordered_set <std::string>
            contains_a_schema 
            {
                  "not"                  
                , "if"                   
                , "then"                 
                , "else"                 
                , "additionalProperties" 
                , "propertyNames"        
                , "additionalItems"      
                , "contains"             
            };
                const std::unordered_set <std::string>
            contains_an_object_of_schemas
            {
                  "$defs"                
                , "definitions"          // deprecated
                , "dependentSchemas"     
                , "properties"           
                , "patternProperties"    
            };
                const std::unordered_set <std::string>
            contains_an_array_of_schemas
            {
                  "allOf"                
                , "anyOf"                
                , "oneOf"                
            };
            if (contains_a_schema.count (name) > 0) 
            {
                analyse (value, base_uri);
                continue;
            }
            if (contains_an_object_of_schemas.count (name) > 0) 
            {
                for (auto&& [key, subschema]: value.get_object ())
                {
                    analyse (subschema, base_uri);
                }
                continue;
            }
            if (contains_an_array_of_schemas.count (name) > 0) 
            {
                    int
                i = 0;
                for (auto&& subschema: value.get_array ())
                {
                    analyse (subschema, base_uri);
                    ++i;
                }
                continue;
            }
            // deprecated
            if (name == "dependencies")
            {
                for (auto&& [key, subvalue]: value.get_object ())
                {
                    if (subvalue.is_object ())
                    {
                        analyse (subvalue, base_uri);
                    }
                }
            }
            if (name == "items")
            {
                if (value.is_array ())
                {
                        int
                    i = 0;
                    for (auto&& subschema: value.get_array ())
                    {
                        analyse (subschema, base_uri);
                        ++i;
                    }
                    continue;
                }
                analyse (value, base_uri);
                continue;
            }
        }
    };

        [[nodiscard]]
        auto
    validate_impl (
          const instance_t&  instance
        , const std::string& instance_location
        , const schema_t&    schema
        , const std::string& schema_location
    )
        -> std::pair <bool, json_t>
    {
            auto
        state = true;
            json_t
        errors = tao::json::null;
            auto
        report = [&](
              std::string const& sub_schema_location
            , std::string const& message
            , json_t const& sub_schema_errors = tao::json::null
        ){
            state = false;
                json_t
            v = {
                  { "schemaLocation", schema_location + sub_schema_location }
                , { "instanceLocation", instance_location }
                , { "message" , message }
            };
            if (!sub_schema_errors.is_null ())
            {
                v["errors"] = sub_schema_errors;
            }
            if (errors.is_null ())
            {
                errors = v;
                return;
            }
            if (errors.is_array ())
            {
                errors.push_back (v);
                return;
            }
        };

        // Boolean schema
        if (schema.is_boolean ())
        {
                auto
            value = schema.get_boolean ();
            return { 
                  schema.get_boolean ()
                , value
                    ? tao::json::null
                    : tao::json::from_string (R"("boolean schema is false")")
            };
        }
        // Object schema
            auto&
        schema_object = schema.get_object ();
            json_t::object_t::const_iterator
        it;
        if (
              it = schema_object.find ("$ref")
            ; it != schema_object.end ()
        ){
                auto&
            ref = it->second;
            if (
                    auto&& 
                  i = registered_references_m.find (&ref)
                ; i != registered_references_m.end ()
            ){
                    auto&
                schema = i->second;
                if (
                        auto&&
                      [is_valid, e] = validate_impl (
                          instance
                        , instance_location
                        , *schema
                        , schema_location + "/$ref"
                      )
                    ; !is_valid
                ){
                    report (
                          "/$ref"
                        , "Sub-schema does not validates the instance" 
                        , e
                    );
                }
            }
            else
            {
                throw (std::runtime_error { format (
                      "Resolution of reference \"{}\" failed."
                    , ref.get_string ()
                )});
            }
            return { state, errors };
        }
        // Keywords for Applying Subschemas in Place
        if (
              it = schema_object.find ("allOf")
            ; it != schema_object.end ()
        ){
                std::size_t
            index = 0;
                std::vector <std::size_t>
            failures;
                json_t::array_t
            sub_errors;
            for (auto&& sub_schema: it->second.get_array ())
            {
                if (
                      auto&& [is_valid, e] = validate_impl (
                          instance
                        , instance_location
                        , sub_schema
                        , schema_location + format ("/allOf/{}", index)
                      )
                      ; !is_valid
                ){
                    failures.push_back (index);
                    sub_errors.push_back (std::move (e));
                    //detail::concatenate (sub_errors, std::move (e));
                }
                ++index;
            }
            if (!failures.empty ())
            {
                report (
                      "/allof"
                    , format ("not all sub-schemas validate the instance: ", failures)
                    , sub_errors
                );
            }
        }
        if (
              it = schema_object.find ("anyOf")
            ; it != schema_object.end ()
        ){
                std::size_t
            index = 0;
                bool
            is_any_valid;
            for (auto&& sub_schema: it->second.get_array ())
            {
                std::tie (is_any_valid, std::ignore) = validate_impl (
                      instance
                    , instance_location
                    , sub_schema
                    , schema_location + format ("/anyOf/{}", index)
                );
                if (is_any_valid){
                    break;
                };
                ++index;
            }
            if (!is_any_valid)
            {
                report (
                      "/anyOf"
                    , "No sub-schema validate the instance"
                );
            }
        }
        if (
              it = schema_object.find ("oneOf")
            ; it != schema_object.end ()
        ){
                std::size_t
            index = 0;
                std::vector <std::size_t>
            successes;
                json_t::array_t
            sub_errors;
            for (auto&& sub_schema: it->second.get_array ())
            {
                    auto&&
                [is_valid, e] = validate_impl (
                      instance
                    , instance_location
                    , sub_schema
                    , schema_location + format ("/oneOf/{}", index)
                );
                if (is_valid){
                    successes.push_back (index);
                }
                else
                {
                    sub_errors.push_back (std::move (e));
                }
                ++index;
            }
            if (successes.size () != 1)
            {
                report (
                      "/oneOf"
                    , successes.size () == 0 
                        ? "No sub-schema validate the instance" 
                        : format (
                              "More than one sub-schema validate the instance: {}" 
                            , successes
                          )
                    , sub_errors
                );
            }
        }
        if (
              it = schema_object.find ("not")
            ; it != schema_object.end ()
        ){
            if (
                  auto&& [is_valid, e] = validate_impl (
                      instance
                    , instance_location
                    , it->second
                    , schema_location + "/not"
                  )
                ; is_valid
            ){
                report (
                      "/not"
                    , "Sub-schema validates the instance" 
                );
            }

        }
        if (
              it = schema_object.find ("if")
            ; it != schema_object.end ()
        ){
            if (
                    auto&& 
                  [is_valid, e] = validate_impl (
                      instance
                    , instance_location
                    , it->second
                    , schema_location + "/if"
                  )
                ; is_valid
            ){
                if (
                      it = schema_object.find ("then")
                    ; it != schema_object.end ()
                ){
                    if (
                            auto&&
                          [is_valid, e] =validate_impl (
                              instance
                            , instance_location
                            , it->second
                            , schema_location + "/then"
                          )
                        ; !is_valid
                    ){
                        report (
                              "/then"
                            , "Sub-schema does not validates the instance" 
                            , e
                        );
                    }
                }
            }
            else
            {
                if (
                      it = schema_object.find ("else")
                    ; it != schema_object.end ()
                ){
                    if (
                            auto&&
                          [is_valid, e] = validate_impl (
                              instance
                            , instance_location
                            , it->second
                            , schema_location + "/then"
                          )
                        ; !is_valid
                    ){
                        report (
                              "/then"
                            , "Sub-schema does not validates the instance" 
                            , e
                        );
                    }
                }
            }
            
        }
        // Validation Keywords for Any Instance Type
        if (
              it = schema_object.find ("type")
            ; it != schema_object.end ()
        ){
            if (it->second.is_string ())
            {
                if (!detail::type_match (it->second, instance))
                {
                    report (
                          "/type"
                        , format (
                              "Type mismatch, schema requires {}, got {}" 
                            , it->second.get_string ()
                            , tao::json::to_string (instance.type ())
                          )
                    );
                }
            }
            else
            {
                    auto&
                type_array = it->second.get_array ();
                    bool
                r = std::any_of (
                      std::begin (type_array)
                    , std::end   (type_array)
                    , [&](auto x){ return detail::type_match (x, instance); }
                );
                if (!r)
                {
                    report (
                         "/type"
                        , format (
                              "Type mismatch, schema requires one of {}, instance is \"{}\""
                            , it->second.get_array ()
                            , tao::json::to_string (instance.type ())
                          )
                    );
                }
            }
        }
        if (
              it = schema_object.find ("enum")
            ; it != schema_object.end ()
        ){
                auto&
            array = it->second.get_array ();
            if (!std::any_of (
                  std::begin (array)
                , std::end (array)
                , [&](auto&& x){ return instance == x; }
            )){
                report ("/enum", "Value no in enum");
            }
        }
        if (
              it = schema_object.find ("const")
            ; it != schema_object.end ()
        ){
            if (instance != it->second)
            {
                report ("/const", "Value does not match \"const\"");
            }
        }

        // Instance is an object
        if (instance.is_object ())
        {
                auto&
            instance_object = instance.get_object ();
            // DEPRECATED in draft-08 XXX
            if (
                  it = schema_object.find ("dependencies")
                ; it != schema_object.end ()
            ){
                    json_t::array_t
                sub_errors;
                    std::vector <std::string>
                failures;
                for (auto&& [property, x]: it->second.get_object ())
                {
                    if (x.is_array ())
                    {
                        if (instance_object.count (property) > 0)
                        {
                            for (auto&& i: x.get_array ())
                            {
                                if (instance_object.count (i.get_string ()) < 1)
                                {
                                    failures.push_back (property);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (instance_object.count (property) > 0)
                        {
                            if (
                                    auto&&
                                  [is_valid, e] = validate_impl (
                                      instance
                                    , instance_location
                                    , x
                                    , schema_location + format ("/dependencies/{}", property)
                                  )
                                ; !is_valid
                            ){
                                failures.push_back (property);
                                sub_errors.push_back (std::move (e));
                                //detail::concatenate (sub_errors, std::move (e));
                            }
                        }

                    }
                }
                if (!failures.empty ())
                {
                    report (
                          "/dependencies"
                        , format ("not all dependencies validate the instance: ", failures)
                        , sub_errors
                    );
                }
            }
            // end of deprecated section XXX
            if (
                  it = schema_object.find ("dependentSchemas")
                ; it != schema_object.end ()
            ){
                    json_t::array_t
                sub_errors;
                    std::vector <std::string>
                failures;
                for (auto&& [property, sub_schema]: it->second.get_object ())
                {
                    if (instance_object.count (property) > 0)
                    {
                        if (
                                auto&&
                              [is_valid, e] = validate_impl (
                                  instance
                                , instance_location
                                , sub_schema
                                , schema_location + format ("/dependentSchemas/{}", property)
                              )
                            ; !is_valid
                        ){
                            failures.push_back (property);
                            sub_errors.push_back (std::move (e));
                            //detail::concatenate (sub_errors, std::move (e));
                        }
                    }
                }
                if (!failures.empty ())
                {
                    report (
                          "/dependentSchemas"
                        , format ("not all dependent sub-schemas validate the instance: ", failures)
                        , sub_errors
                    );
                }
            }
                auto
            get_object_if_property_exists = [&](std::string const& name) 
                -> const json_t::object_t*
            {
                    const json_t*
                p = schema.find (name);
                return p ? &(p->get_object ()) : nullptr;
            };
                const json_t::object_t*
            properties = get_object_if_property_exists ("properties");
                const json_t::object_t*
            pattern_properties = get_object_if_property_exists ("patternProperties");
                const json_t*
            additional_properties = schema.find ("additionalProperties");
                const json_t*
            property_names = schema.find ("propertyNames");
            for (auto&& [property, value]: instance_object)
            {
                    bool
                apply_additional = true;
                if (properties && properties->count (property) > 0)
                {
                    if (
                            auto&&
                          [is_valid, e] = validate_impl (
                              value
                            , instance_location + "/" + property
                            , properties->at (property)
                            , schema_location + "/properties/" + property
                          )
                        ; !is_valid
                    ){
                        report (
                              "/properties/" + property
                            , "Sub-schema does not validates the instance" 
                            , e
                        );
                    }
                    apply_additional = false;
                }
                if (pattern_properties)
                {
                    for (auto&& [pattern, schema]: *pattern_properties)
                    {
                            std::regex
                        re { pattern };
                        if (std::regex_search (property, re))
                        {
                            if (
                                    auto&&
                                  [is_valid, e] = validate_impl (
                                      value
                                    , instance_location + "/" + property
                                    , schema
                                    , schema_location + "/patternProperties/" + pattern
                                  )
                                ; !is_valid
                            ){
                                report (
                                      "/patternProperties/" + pattern
                                    , "Sub-schema does not validates the instance" 
                                    , e
                                );
                            }
                            apply_additional = false;
                        }
                    }
                }
                if (apply_additional && additional_properties)
                {
                    if (
                            auto&&
                          [is_valid, e] = validate_impl (
                              value
                            , instance_location + "/" + property
                            , *additional_properties
                            , schema_location + "/additionalProperties"
                          )
                        ; !is_valid
                    ){
                        report (
                              "/additionalProperties"
                            , "Sub-schema does not validates the instance" 
                            , e
                        );
                    }
                }
                if (property_names)
                {
                    if (
                            auto&&
                          [is_valid, e] = validate_impl (
                              property
                            , instance_location + "/" + property
                            , *property_names
                            , schema_location + "/propertyNames"
                          )
                        ; !is_valid
                    ){
                        report (
                              "/propertyNames"
                            , "Sub-schema does not validates the instance" 
                            , e
                        );
                    }
                }
            }
            if (
                  it = schema_object.find ("maxProperties")
                ; it != schema_object.end ()
            ){
                if (instance_object.size () > it->second.get_unsigned ())
                {
                    report ("maxProperties", "Object has too many properties");
                }
            }
            if (
                  it = schema_object.find ("minProperties")
                ; it != schema_object.end ()
            ){
                if (instance_object.size () < it->second.get_unsigned ())
                {
                    report ("minProperties", "Object has too few properties");
                }
            }
            if (
                  it = schema_object.find ("required")
                ; it != schema_object.end ()
            ){
                    std::size_t
                index = 0;
                for (auto&& i: it->second.get_array ())
                {
                        auto&
                    property = i.get_string ();
                    if (instance_object.count (property) == 0)
                    {
                        report (
                              format ("/required/{}", index)
                            , format ("Missing required property \"{}\"", property)
                        );
                    }
                    ++index;
                }
            }
            if (
                  it = schema_object.find ("dependentRequired")
                ; it != schema_object.end ()
            ){
                    auto&
                sub_schema_object = it->second.get_object ();
                for (auto&& [property, value]: instance_object)
                {
                    if (
                          auto&& array = sub_schema_object.find (property)
                        ; array != sub_schema_object.end ()
                    ){
                        for (auto&& i: array->second.get_array ())
                        {
                                auto&&
                            dependent_property = i.get_string ();
                            if (instance_object.count (dependent_property) < 1)
                            {
                                report (
                                      "/dependentRequired"
                                    , format ("Missing property \"{}\", required by the presence of \"{}\"", dependent_property, property)
                                );
                            }
                        }
                    }
                }

            }
        }
        // Instance is an array
        if (instance.is_array ())
        {
                auto&
            instance_array = instance.get_array ();
            if (
                  it = schema_object.find ("items")
                ; it != schema_object.end ()
            ){
                    json_t::array_t
                sub_errors;
                    std::vector <std::size_t>
                failures;
                    std::size_t
                index = 0;
                if (it->second.is_array ())
                {
                        auto&
                    schema_array = it->second.get_array ();
                    for (; 
                             index < instance_array.size () 
                          && index < schema_array.size ()
                        ; ++index
                    ){
                        if (
                                auto&&
                              [is_valid, e] = validate_impl (
                                  instance_array[index]
                                , instance_location + format ("/{}", index)
                                , schema_array[index]
                                , schema_location + format ("/items/{}", index)
                              )
                            ; !is_valid
                        ){
                            failures.push_back (index);
                            sub_errors.push_back (std::move (e));
                            //detail::concatenate (sub_errors, std::move (e));
                        }
                    }
                }
                else
                {
                    for (; index < instance_array.size (); ++index)
                    {
                        if (
                                auto&&
                              [is_valid, e] = validate_impl (
                                  instance_array[index]
                                , instance_location + "/" + format ("{}", index)
                                , it->second
                                , schema_location + format ("/items")
                              )
                            ; !is_valid
                        ){
                            failures.push_back (index);
                            sub_errors.push_back (std::move (e));
                            //detail::concatenate (sub_errors, std::move (e));
                        }
                    }
                }
                if (!failures.empty ())
                {
                    report (
                          "/items"
                        , format ("Not all dependent sub-schemas validate the instance: ", failures)
                        , sub_errors
                    );
                }
                failures.clear ();
                sub_errors.clear ();
                if (
                      it = schema_object.find ("additionalItems")
                    ; it != schema_object.end ()
                ){
                    for (; index < instance_array.size (); ++index)
                    {
                        if (
                                auto&&
                              [is_valid, e] = validate_impl (
                                  instance_array[index]
                                , instance_location + "/" + format ("{}", index)
                                , it->second
                                , schema_location + format ("additionalItems")
                              )
                            ; !is_valid
                        ){
                            failures.push_back (index);
                            sub_errors.push_back (std::move (e));
                            //detail::concatenate (sub_errors, std::move (e));
                        }
                    }
                    if (!failures.empty ())
                    {
                        report (
                              "/additionalItems"
                            , format ("Not all dependent sub-schemas validate the instance: ", failures)
                            , sub_errors
                        );
                    }
                }

            }
            if (
                  it = schema_object.find ("contains")
                ; it != schema_object.end ()
            ){
                    std::size_t
                contains_count = 0;
                for (
                      std::size_t index = 0 
                    ; index < instance_array.size ()
                    ; ++index
                ){
                    if (
                            auto&&
                          [is_valid, e] = validate_impl (
                              instance_array[index]
                            , instance_location + format ("/{}", index)
                            , it->second
                            , schema_location + format ("/items/{}", index)
                          )
                        ; is_valid
                    ){
                        ++contains_count;
                    }
                }
                if (contains_count == 0)
                {
                    report (
                          "/contains"
                        , "No items of the instance validates the sub-schema"
                    );
                }
                if (
                      it = schema_object.find ("maxContains")
                    ; it != schema_object.end ()
                ){
                    if (contains_count > it->second.get_unsigned ())
                    {
                        report ("/maxContains", "Max contains exceeded");
                    }
                }
                if (
                      it = schema_object.find ("minContains")
                    ; it != schema_object.end ()
                ){
                    if (contains_count < it->second.get_unsigned ())
                    {
                        report ("/minContains", "Min contains subceeded");
                    }
                }
            }
            if (
                  it = schema_object.find ("maxItems")
                ; it != schema_object.end ()
            ){
                if (instance_array.size () > it->second.get_unsigned ())
                {
                    report ("/maxItems", "Array has too many items");
                }
            }
            if (
                  it = schema_object.find ("minItems")
                ; it != schema_object.end ()
            ){
                if (instance_array.size () < it->second.get_unsigned ())
                {
                    report ("/minItems", "Array has too few items");
                }
            }
            if (
                  it = schema_object.find ("uniqueItems")
                ; it != schema_object.end ()
            ){
                if (it->second.get_boolean ())
                {
                        std::set <json_t>
                    set;
                    for (auto&& i: instance_array)
                    {
                        if (set.count (i) > 0)
                        {
                            report ("/uniqueItems", "Duplicate items found");
                        }
                        set.insert (i);
                    }
                }
            }

        }
        // Instance is a number
        if (instance.is_number ())
        {
            if (
                  it = schema_object.find ("multipleOf")
                ; it != schema_object.end ()
            ){
                if (remainder (instance.as <double> (), it->second.as <double> ()) != 0)
                {
                    report ("multipleOf", "Failed");
                }
            }
            if (
                  it = schema_object.find ("maximum")
                ; it != schema_object.end ()
            ){
                if (instance > it->second)
                {
                    report ("/maximum", "Maximum value exceeded");
                }
            }
            if (
                  it = schema_object.find ("exclusiveMaximum")
                ; it != schema_object.end ()
            ){
                if (instance >= it->second)
                {
                    report ("/exclusiveMaximum", "Exclusive maximum value exceeded");
                }
            }
            if (
                  it = schema_object.find ("minimum")
                ; it != schema_object.end ()
            ){
                if (instance < it->second)
                {
                    report ("/minimum", "Minimum value subceeded");
                }
            }
            if (
                  it = schema_object.find ("exclusiveMinimum")
                ; it != schema_object.end ()
            ){
                if (instance <= it->second)
                {
                    report ("/exclusiveMinimum", "Exclusive minimum value subceeded");
                }
            }
        }
        // Instance is a string
        if (instance.is_string ())
        {
            if (
                  it = schema_object.find ("maxLength")
                ; it != schema_object.end ()
            ){
                if (instance.get_string ().length () > it->second.get_unsigned ())
                {
                    report ("/maxLength", "String too long");
                }
            }
            if (
                  it = schema_object.find ("minLength")
                ; it != schema_object.end ()
            ){
                if (instance.get_string ().length () < it->second.get_unsigned ())
                {
                    report ("/minLength", "String too short");
                }
            }
            if (
                  it = schema_object.find ("pattern")
                ; it != schema_object.end ()
            ){
                    std::regex
                re { it->second.get_string () };
                if (!std::regex_search (instance.get_string (), re))
                {
                    report ("/pattern", "String does not match pattern");
                }
            }
            if (
                  it = schema_object.find ("format")
                ; it != schema_object.end ()
            ){
                // TODO
            }
            if (
                  it = schema_object.find ("contentEncoding")
                ; it != schema_object.end ()
            ){
                // TODO
            }
            if (
                  it = schema_object.find ("contentMediaType")
                ; it != schema_object.end ()
            ){
                // TODO
            }
        }
        return { state, errors };
    }

// }}} private:
public:

    validator_t ()
    {
        last_schema_m = add_meta_schema ();
    }

        auto
    add_schema (const json_t& json, std::string const& document_uri)
    {
        last_schema_m = add_schema_impl (json, document_uri);
    }

        auto
    add_schema (json_t&& json, std::string const& document_uri)
    {
        last_schema_m = add_schema_impl (std::move (json), document_uri);
    }

        auto
    validate (const instance_t& instance, std::string const& schema_uri = "")
        -> std::pair <bool, json_t>
    {
            auto
        schema = last_schema_m;
        if (!schema_uri.empty ())
        {
                std::tie
            (schema, std::ignore) = resolve_reference (schema_uri);
        }
        if (schema)
        {
            return validate_impl (
                  instance
                , "/"
                , *schema
                , "#"
            );
        }
        throw std::runtime_error {"schema not found"};
    }

        std::pair <bool, json_t>
    validate_schema (const schema_t& schema)
    {
        return validate_impl (
              schema
            , "/"
            , *meta_schema_m
            , "#"
        );
    }
};

} // namespace isto::json_validator
