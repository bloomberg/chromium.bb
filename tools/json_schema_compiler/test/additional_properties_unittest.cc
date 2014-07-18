// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/additional_properties.h"

using namespace test::api::additional_properties;

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    AdditionalPropertiesTypePopulate) {
  {
    scoped_ptr<base::ListValue> list_value(new base::ListValue());
    list_value->Append(new base::StringValue("asdf"));
    list_value->Append(new base::FundamentalValue(4));
    scoped_ptr<base::DictionaryValue> type_value(new base::DictionaryValue());
    type_value->SetString("string", "value");
    type_value->SetInteger("other", 9);
    type_value->Set("another", list_value.release());
    scoped_ptr<AdditionalPropertiesType> type(new AdditionalPropertiesType());
    ASSERT_TRUE(AdditionalPropertiesType::Populate(*type_value, type.get()));
    EXPECT_TRUE(type->additional_properties.Equals(type_value.get()));
  }
  {
    scoped_ptr<base::DictionaryValue> type_value(new base::DictionaryValue());
    type_value->SetInteger("string", 3);
    scoped_ptr<AdditionalPropertiesType> type(new AdditionalPropertiesType());
    EXPECT_FALSE(AdditionalPropertiesType::Populate(*type_value, type.get()));
  }
}

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    AdditionalPropertiesParamsCreate) {
  scoped_ptr<base::DictionaryValue> param_object_value(
      new base::DictionaryValue());
  param_object_value->SetString("str", "a");
  param_object_value->SetInteger("num", 1);
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  params_value->Append(param_object_value->DeepCopy());
  scoped_ptr<AdditionalProperties::Params> params(
      AdditionalProperties::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_TRUE(params->param_object.additional_properties.Equals(
      param_object_value.get()));
}

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    ReturnAdditionalPropertiesResultCreate) {
  ReturnAdditionalProperties::Results::ResultObject result_object;
  result_object.integer = 5;
  result_object.additional_properties["key"] = "value";

  base::ListValue expected;
  {
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetInteger("integer", 5);
    dict->SetString("key", "value");
    expected.Append(dict);
  }

  EXPECT_TRUE(base::Value::Equals(
      ReturnAdditionalProperties::Results::Create(result_object).get(),
      &expected));
}
