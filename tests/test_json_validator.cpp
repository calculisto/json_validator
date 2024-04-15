#include <doctest/doctest.h>
#include "../include/calculisto/json_validator/json_validator.hpp"
    using namespace calculisto::json_validator;
#include <filesystem>
    namespace fs = std::filesystem;
    using namespace std::literals;
    namespace json = tao::json;



// The path to the JSON Schema repository. Adjust to your needs.
    auto const
json_schema_path = fs::path { "../../../external/json-schema-org/" };

// Recursivley load all the remote shcemas used in the JSON Schema test suite.
    void
load_remote_schemas (validator_t& validator, fs::path const& root_path)
{
        // Can't do that, this is a recursive lambda.
        // auto
        std::function <void (fs::path const&)>
    recurse = [&](fs::path const& rel_path)
    {
        for (auto&& r: fs::directory_iterator (root_path / rel_path))
        {
            if (r.is_directory ())
            {
                    auto
                i = r.path ().end ();
                --i;
                recurse (*i);
            }
            if (!r.is_regular_file ()) 
            {
                continue;
            }
                const auto
            filename = rel_path / r.path ().filename ();
                const auto
            document_uri = "http://localhost:1234/"s + filename.native ();
            validator.add_schema (
                  tao::json::from_file (root_path / filename)
                , document_uri
            );
        }
    };
    recurse ("");
}

TEST_CASE("json_validator.hpp")
{
            auto const
    root = json_schema_path / "json-schema-test-suite/tests/draft7";
    for(auto&& p: fs::directory_iterator(root))
    {
        if (!p.is_regular_file ()) 
        {
            continue;
        }
            const auto
        test_file = json::from_file (p.path ());
        for (auto&& test_suite: test_file.get_array ())
        {
                validator_t
            validator;
            if (p.path ().filename ().string () == "refRemote.json")
            {
                load_remote_schemas (
                      validator
                    , json_schema_path / "json-schema-test-suite/remotes"
                );
            }
            validator.add_schema (
                  test_suite.at ("schema")
                , "http://example.com/dummy"
            );
            for (auto&& test: test_suite.at ("tests").get_array ())
            {
                    auto&&
                [result, errors] = validator.validate (test.at ("data"));
                    auto
                expected = test.at ("valid").get_boolean ();
                CHECK_MESSAGE (result == expected, fmt::format ("in file {} with schema {}, {} with data {}\n", p.path ().native (), test_suite.at ("schema"), test.at ("description"), test.at ("data")));
            }
        }
    }
} // TEST_CASE("json_validator.hpp")
