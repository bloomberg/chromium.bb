// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/choices.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::choices;

TEST(JsonSchemaCompilerChoicesTest, TakesIntegersParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateBooleanValue(true));
    scoped_ptr<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateIntegerValue(6));
    scoped_ptr<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->nums.as_array);
    EXPECT_EQ(6, *params->nums.as_integer);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<ListValue> integers(new ListValue());
    integers->Append(Value::CreateIntegerValue(6));
    integers->Append(Value::CreateIntegerValue(8));
    params_value->Append(integers.release());
    scoped_ptr<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->nums.as_array);
    EXPECT_EQ((size_t) 2, params->nums.as_array->size());
    EXPECT_EQ(6, params->nums.as_array->at(0));
    EXPECT_EQ(8, params->nums.as_array->at(1));
  }
}

TEST(JsonSchemaCompilerChoicesTest, ObjectWithChoicesParamsCreate) {
  {
    scoped_ptr<DictionaryValue> object_param(new DictionaryValue());
    object_param->SetString("strings", "asdf");
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(object_param.release());
    scoped_ptr<ObjectWithChoices::Params> params(
        ObjectWithChoices::Params::Create(*params_value));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->string_info.strings.as_array);
    EXPECT_EQ("asdf", *params->string_info.strings.as_string);
    EXPECT_FALSE(params->string_info.integers);
  }
  {
    scoped_ptr<DictionaryValue> object_param(new DictionaryValue());
    object_param->SetString("strings", "asdf");
    object_param->SetInteger("integers", 6);
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(object_param.release());
    scoped_ptr<ObjectWithChoices::Params> params(
        ObjectWithChoices::Params::Create(*params_value));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->string_info.strings.as_array);
    EXPECT_EQ("asdf", *params->string_info.strings.as_string);
    ASSERT_TRUE(params->string_info.integers);
    EXPECT_FALSE(params->string_info.integers->as_array);
    EXPECT_EQ(6, *params->string_info.integers->as_integer);
  }
}

TEST(JsonSchemaCompilerChoicesTest, ObjectWithChoicesParamsCreateFail) {
  {
    scoped_ptr<DictionaryValue> object_param(new DictionaryValue());
    object_param->SetWithoutPathExpansion("strings",
        Value::CreateIntegerValue(5));
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(object_param.release());
    scoped_ptr<ObjectWithChoices::Params> params(
        ObjectWithChoices::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    scoped_ptr<DictionaryValue> object_param(new DictionaryValue());
    object_param->SetWithoutPathExpansion("strings",
        Value::CreateStringValue("asdf"));
    object_param->SetWithoutPathExpansion("integers",
        Value::CreateStringValue("asdf"));
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(object_param.release());
    scoped_ptr<ObjectWithChoices::Params> params(
        ObjectWithChoices::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    scoped_ptr<DictionaryValue> object_param(new DictionaryValue());
    object_param->SetWithoutPathExpansion("integers",
        Value::CreateIntegerValue(6));
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(object_param.release());
    scoped_ptr<ObjectWithChoices::Params> params(
        ObjectWithChoices::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerChoicesTest, PopulateChoiceType) {
  std::vector<std::string> strings;
  strings.push_back("list");
  strings.push_back("of");
  strings.push_back("strings");

  ListValue* strings_value = new ListValue();
  for (size_t i = 0; i < strings.size(); ++i)
    strings_value->Append(Value::CreateStringValue(strings[i]));

  DictionaryValue value;
  value.SetInteger("integers", 4);
  value.Set("strings", strings_value);

  ChoiceType out;
  ASSERT_TRUE(ChoiceType::Populate(value, &out));
  ASSERT_TRUE(out.integers.as_integer.get());
  EXPECT_FALSE(out.integers.as_array.get());
  EXPECT_EQ(4, *out.integers.as_integer);

  EXPECT_FALSE(out.strings->as_string.get());
  ASSERT_TRUE(out.strings->as_array.get());
  EXPECT_EQ(strings, *out.strings->as_array);
}

TEST(JsonSchemaCompilerChoicesTest, ChoiceTypeToValue) {
  ListValue* strings_value = new ListValue();
  strings_value->Append(Value::CreateStringValue("list"));
  strings_value->Append(Value::CreateStringValue("of"));
  strings_value->Append(Value::CreateStringValue("strings"));

  DictionaryValue value;
  value.SetInteger("integers", 5);
  value.Set("strings", strings_value);

  ChoiceType out;
  ASSERT_TRUE(ChoiceType::Populate(value, &out));

  EXPECT_TRUE(value.Equals(out.ToValue().get()));
}

TEST(JsonSchemaCompilerChoicesTest, ReturnChoices) {
  {
    ReturnChoices::Results::Result results;
    results.as_array.reset(new std::vector<int>());
    results.as_array->push_back(1);
    results.as_array->push_back(2);

    scoped_ptr<base::Value> results_value = results.ToValue();
    ASSERT_TRUE(results_value);

    base::ListValue expected;
    expected.AppendInteger(1);
    expected.AppendInteger(2);

    EXPECT_TRUE(expected.Equals(results_value.get()));
  }
  {
    ReturnChoices::Results::Result results;
    results.as_integer.reset(new int(5));

    scoped_ptr<base::Value> results_value = results.ToValue();
    ASSERT_TRUE(results_value);

    base::FundamentalValue expected(5);

    EXPECT_TRUE(expected.Equals(results_value.get()));
  }
}
