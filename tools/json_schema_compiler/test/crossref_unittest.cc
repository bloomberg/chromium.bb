// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/simple_api.h"
#include "tools/json_schema_compiler/test/crossref.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::crossref;

namespace {

static scoped_ptr<base::DictionaryValue> CreateTestTypeDictionary() {
  base::DictionaryValue* value(new base::DictionaryValue());
  value->SetWithoutPathExpansion("number", new base::FundamentalValue(1.1));
  value->SetWithoutPathExpansion("integer", new base::FundamentalValue(4));
  value->SetWithoutPathExpansion("string", new base::StringValue("bling"));
  value->SetWithoutPathExpansion("boolean", new base::FundamentalValue(true));
  return scoped_ptr<base::DictionaryValue>(value);
}

}  // namespace

TEST(JsonSchemaCompilerCrossrefTest, CrossrefTypePopulate) {
  scoped_ptr<CrossrefType> crossref_type(new CrossrefType());
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("testType", CreateTestTypeDictionary().release());
  EXPECT_TRUE(CrossrefType::Populate(*value, crossref_type.get()));
  EXPECT_TRUE(crossref_type->test_type.get());
  EXPECT_TRUE(CreateTestTypeDictionary()->Equals(
        crossref_type->test_type->ToValue().get()));
  EXPECT_TRUE(value->Equals(crossref_type->ToValue().get()));
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeOptionalParamCreate) {
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  params_value->Append(CreateTestTypeDictionary().release());
  scoped_ptr<TestTypeOptionalParam::Params> params(
      TestTypeOptionalParam::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_TRUE(params->test_type.get());
  EXPECT_TRUE(
      CreateTestTypeDictionary()->Equals(params->test_type->ToValue().get()));
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeOptionalParamFail) {
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  scoped_ptr<base::DictionaryValue> test_type_value =
      CreateTestTypeDictionary();
  test_type_value->RemoveWithoutPathExpansion("number", NULL);
  params_value->Append(test_type_value.release());
  scoped_ptr<TestTypeOptionalParam::Params> params(
      TestTypeOptionalParam::Params::Create(*params_value));
  EXPECT_FALSE(params.get());
}

TEST(JsonSchemaCompilerCrossrefTest, GetTestType) {
  scoped_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
  scoped_ptr<test::api::simple_api::TestType> test_type(
      new test::api::simple_api::TestType());
  EXPECT_TRUE(
      test::api::simple_api::TestType::Populate(*value, test_type.get()));

  scoped_ptr<base::ListValue> results =
      GetTestType::Results::Create(*test_type);
  base::DictionaryValue* result_dict = NULL;
  results->GetDictionary(0, &result_dict);
  EXPECT_TRUE(value->Equals(result_dict));
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeInObjectParamsCreate) {
  {
    scoped_ptr<base::ListValue> params_value(new base::ListValue());
    scoped_ptr<base::DictionaryValue> param_object_value(
        new base::DictionaryValue());
    param_object_value->Set("testType", CreateTestTypeDictionary().release());
    param_object_value->Set("boolean", new base::FundamentalValue(true));
    params_value->Append(param_object_value.release());
    scoped_ptr<TestTypeInObject::Params> params(
        TestTypeInObject::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_TRUE(params->param_object.test_type.get());
    EXPECT_TRUE(params->param_object.boolean);
    EXPECT_TRUE(CreateTestTypeDictionary()->Equals(
          params->param_object.test_type->ToValue().get()));
  }
  {
    scoped_ptr<base::ListValue> params_value(new base::ListValue());
    scoped_ptr<base::DictionaryValue> param_object_value(
        new base::DictionaryValue());
    param_object_value->Set("boolean", new base::FundamentalValue(true));
    params_value->Append(param_object_value.release());
    scoped_ptr<TestTypeInObject::Params> params(
        TestTypeInObject::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->param_object.test_type.get());
    EXPECT_TRUE(params->param_object.boolean);
  }
  {
    scoped_ptr<base::ListValue> params_value(new base::ListValue());
    scoped_ptr<base::DictionaryValue> param_object_value(
        new base::DictionaryValue());
    param_object_value->Set("testType", new base::StringValue("invalid"));
    param_object_value->Set("boolean", new base::FundamentalValue(true));
    params_value->Append(param_object_value.release());
    scoped_ptr<TestTypeInObject::Params> params(
        TestTypeInObject::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    scoped_ptr<base::ListValue> params_value(new base::ListValue());
    scoped_ptr<base::DictionaryValue> param_object_value(
        new base::DictionaryValue());
    param_object_value->Set("testType", CreateTestTypeDictionary().release());
    params_value->Append(param_object_value.release());
    scoped_ptr<TestTypeInObject::Params> params(
        TestTypeInObject::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}
