// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/registry_dict.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

TEST(RegistryDictTest, SetAndGetValue) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("one", int_value.Clone());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));

  test_dict.SetValue("two", string_value.Clone());
  EXPECT_EQ(2u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  EXPECT_EQ(string_value, *test_dict.GetValue("two"));

  absl::optional<base::Value> one(test_dict.RemoveValue("one"));
  ASSERT_TRUE(one.has_value());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, one.value());
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_EQ(string_value, *test_dict.GetValue("two"));

  test_dict.ClearValues();
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingValueNames) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("One", int_value.Clone());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("oNe"));

  auto entry = test_dict.values().begin();
  ASSERT_NE(entry, test_dict.values().end());
  EXPECT_EQ("One", entry->first);

  test_dict.SetValue("ONE", string_value.Clone());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(string_value, *test_dict.GetValue("one"));

  absl::optional<base::Value> removed_value(test_dict.RemoveValue("onE"));
  ASSERT_TRUE(removed_value.has_value());
  EXPECT_EQ(string_value, removed_value.value());
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, SetAndGetKeys) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("one", int_value.Clone());
  test_dict.SetKey("two", std::move(subdict));
  EXPECT_EQ(1u, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("one"));

  subdict = std::make_unique<RegistryDict>();
  subdict->SetValue("three", string_value.Clone());
  test_dict.SetKey("four", std::move(subdict));
  EXPECT_EQ(2u, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("one"));
  actual_subdict = test_dict.GetKey("four");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(string_value, *actual_subdict->GetValue("three"));

  test_dict.ClearKeys();
  EXPECT_FALSE(test_dict.GetKey("one"));
  EXPECT_FALSE(test_dict.GetKey("three"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingKeyNames) {
  RegistryDict test_dict;

  base::Value int_value(42);

  test_dict.SetKey("One", std::make_unique<RegistryDict>());
  EXPECT_EQ(1u, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(actual_subdict->values().empty());

  auto entry = test_dict.keys().begin();
  ASSERT_NE(entry, test_dict.keys().end());
  EXPECT_EQ("One", entry->first);

  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", int_value.Clone());
  test_dict.SetKey("ONE", std::move(subdict));
  EXPECT_EQ(1u, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("two"));

  std::unique_ptr<RegistryDict> removed_key(test_dict.RemoveKey("one"));
  ASSERT_TRUE(removed_key);
  EXPECT_EQ(int_value, *removed_key->GetValue("two"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, Merge) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  dict_a.SetValue("one", int_value.Clone());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.Clone());
  dict_a.SetKey("three", std::move(subdict));

  dict_b.SetValue("four", string_value.Clone());
  subdict = std::make_unique<RegistryDict>();
  subdict->SetValue("two", int_value.Clone());
  dict_b.SetKey("three", std::move(subdict));
  subdict = std::make_unique<RegistryDict>();
  subdict->SetValue("five", int_value.Clone());
  dict_b.SetKey("six", std::move(subdict));

  dict_a.Merge(dict_b);

  EXPECT_EQ(int_value, *dict_a.GetValue("one"));
  EXPECT_EQ(string_value, *dict_b.GetValue("four"));
  RegistryDict* actual_subdict = dict_a.GetKey("three");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("two"));
  actual_subdict = dict_a.GetKey("six");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("five"));
}

TEST(RegistryDictTest, Swap) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  dict_a.SetValue("one", int_value.Clone());
  dict_a.SetKey("two", std::make_unique<RegistryDict>());
  dict_b.SetValue("three", string_value.Clone());

  dict_a.Swap(&dict_b);

  EXPECT_EQ(int_value, *dict_b.GetValue("one"));
  EXPECT_TRUE(dict_b.GetKey("two"));
  EXPECT_FALSE(dict_b.GetValue("two"));

  EXPECT_EQ(string_value, *dict_a.GetValue("three"));
  EXPECT_FALSE(dict_a.GetValue("one"));
  EXPECT_FALSE(dict_a.GetKey("two"));
}

#if BUILDFLAG(IS_WIN)
TEST(RegistryDictTest, ConvertToJSON) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");
  base::Value string_zero("0");
  base::Value string_dict("{ \"key\": [ \"value\" ] }");

  test_dict.SetValue("one", int_value.Clone());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.Clone());
  test_dict.SetKey("three", std::move(subdict));
  std::unique_ptr<RegistryDict> list(new RegistryDict());
  list->SetValue("1", string_value.Clone());
  test_dict.SetKey("dict-to-list", std::move(list));
  test_dict.SetValue("int-to-bool", int_value.Clone());
  test_dict.SetValue("int-to-double", int_value.Clone());
  test_dict.SetValue("string-to-bool", string_zero.Clone());
  test_dict.SetValue("string-to-double", string_zero.Clone());
  test_dict.SetValue("string-to-int", string_zero.Clone());
  test_dict.SetValue("string-to-dict", string_dict.Clone());

  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"dict-to-list\": {"
      "      \"type\": \"array\","
      "      \"items\": { \"type\": \"string\" }"
      "    },"
      "    \"int-to-bool\": { \"type\": \"boolean\" },"
      "    \"int-to-double\": { \"type\": \"number\" },"
      "    \"string-to-bool\": { \"type\": \"boolean\" },"
      "    \"string-to-double\": { \"type\": \"number\" },"
      "    \"string-to-int\": { \"type\": \"integer\" },"
      "    \"string-to-dict\": { \"type\": \"object\" }"
      "  }"
      "}",
      &error);
  ASSERT_TRUE(schema.valid()) << error;

  std::unique_ptr<base::Value> actual(test_dict.ConvertToJSON(schema));

  base::DictionaryValue expected;
  expected.SetKey("one", int_value.Clone());
  auto expected_subdict = std::make_unique<base::DictionaryValue>();
  expected_subdict->SetKey("two", string_value.Clone());
  expected.Set("three", std::move(expected_subdict));
  auto expected_list = std::make_unique<base::ListValue>();
  expected_list->Append(std::make_unique<base::Value>(string_value.Clone()));
  expected.Set("dict-to-list", std::move(expected_list));
  expected.SetBoolean("int-to-bool", true);
  expected.SetDouble("int-to-double", 42.0);
  expected.SetBoolean("string-to-bool", false);
  expected.SetDouble("string-to-double", 0.0);
  expected.SetInteger("string-to-int", static_cast<int>(0));
  expected_list = std::make_unique<base::ListValue>();
  expected_list->Append(std::make_unique<base::Value>("value"));
  expected_subdict = std::make_unique<base::DictionaryValue>();
  expected_subdict->Set("key", std::move(expected_list));
  expected.Set("string-to-dict", std::move(expected_subdict));

  EXPECT_EQ(expected, *actual);
}

TEST(RegistryDictTest, NonSequentialConvertToJSON) {
  RegistryDict test_dict;

  std::unique_ptr<RegistryDict> list(new RegistryDict());
  list->SetValue("1", base::Value("1").Clone());
  list->SetValue("2", base::Value("2").Clone());
  list->SetValue("THREE", base::Value("3").Clone());
  list->SetValue("4", base::Value("4").Clone());
  test_dict.SetKey("dict-to-list", std::move(list));

  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"dict-to-list\": {"
      "      \"type\": \"array\","
      "      \"items\": { \"type\": \"string\" }"
      "    }"
      "  }"
      "}",
      &error);
  ASSERT_TRUE(schema.valid()) << error;

  std::unique_ptr<base::Value> actual(test_dict.ConvertToJSON(schema));

  base::DictionaryValue expected;
  std::unique_ptr<base::ListValue> expected_list(new base::ListValue());
  expected_list->Append(base::Value("1").Clone());
  expected_list->Append(base::Value("2").Clone());
  expected_list->Append(base::Value("4").Clone());
  expected.Set("dict-to-list", std::move(expected_list));

  EXPECT_EQ(expected, *actual);
}

TEST(RegistryDictTest, PatternPropertySchema) {
  RegistryDict test_dict;

  base::Value string_dict("[ \"*://*.google.com\" ]");
  base::Value version_string("1.0.0");

  std::unique_ptr<RegistryDict> policy_dict(new RegistryDict());
  std::unique_ptr<RegistryDict> subdict_id(new RegistryDict());
  // Values with schema are parsed even if the schema is a regexp property.
  subdict_id->SetValue("runtime_blocked_hosts", string_dict.Clone());
  subdict_id->SetValue("runtime_allowed_hosts", string_dict.Clone());
  // Regexp. validated properties are valid too.
  subdict_id->SetValue("minimum_version_required", version_string.Clone());
  policy_dict->SetKey("aaaabbbbaaaabbbbaaaabbbbaaaabbbb",
                      std::move(subdict_id));
  // Values that have no schema are left as strings regardless of structure.
  policy_dict->SetValue("invalid_key", string_dict.Clone());
  test_dict.SetKey("ExtensionSettings", std::move(policy_dict));

  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"ExtensionSettings\": {"
      "      \"type\": \"object\","
      "      \"patternProperties\": {"
      "        \"^[a-p]{32}(?:,[a-p]{32})*,?$\": {"
      "          \"type\": \"object\","
      "          \"properties\": {"
      "            \"minimum_version_required\": {"
      "              \"type\": \"string\","
      "              \"pattern\": \"^[0-9]+([.][0-9]+)*$\","
      "            },"
      "            \"runtime_blocked_hosts\": {"
      "              \"type\": \"array\","
      "              \"items\": {"
      "                \"type\": \"string\""
      "              },"
      "              \"id\": \"ListOfUrlPatterns\""
      "            },"
      "            \"runtime_allowed_hosts\": {"
      "              \"$ref\": \"ListOfUrlPatterns\""
      "            },"
      "          },"
      "        },"
      "      },"
      "    },"
      "  },"
      "}",
      &error);
  ASSERT_TRUE(schema.valid()) << error;

  std::unique_ptr<base::Value> actual(test_dict.ConvertToJSON(schema));

  base::DictionaryValue expected;
  std::unique_ptr<base::DictionaryValue> expected_extension_settings(
      new base::DictionaryValue());
  std::unique_ptr<base::ListValue> list_value(new base::ListValue());
  list_value->Append("*://*.google.com");
  std::unique_ptr<base::DictionaryValue> restrictions_properties(
      new base::DictionaryValue());
  restrictions_properties->Set(
      "runtime_blocked_hosts",
      base::Value::ToUniquePtrValue(list_value->Clone()));
  restrictions_properties->Set(
      "runtime_allowed_hosts",
      base::Value::ToUniquePtrValue(list_value->Clone()));
  restrictions_properties->Set(
      "minimum_version_required",
      base::Value::ToUniquePtrValue(version_string.Clone()));
  expected_extension_settings->Set("aaaabbbbaaaabbbbaaaabbbbaaaabbbb",
                                   std::move(restrictions_properties));
  expected_extension_settings->Set(
      "invalid_key", std::make_unique<base::Value>(std::move(string_dict)));
  expected.Set("ExtensionSettings", std::move(expected_extension_settings));

  // Needed so that the EXPECT below prints good values in case of a mismatch.
  const base::Value* expected_pointer = &expected;
  EXPECT_EQ(*expected_pointer, *actual);
}
#endif

TEST(RegistryDictTest, KeyValueNameClashes) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("one", int_value.Clone());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.Clone());
  test_dict.SetKey("one", std::move(subdict));

  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  RegistryDict* actual_subdict = test_dict.GetKey("one");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(string_value, *actual_subdict->GetValue("two"));
}

}  // namespace
}  // namespace policy
