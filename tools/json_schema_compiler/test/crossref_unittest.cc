// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/simple_api.h"
#include "tools/json_schema_compiler/test/crossref.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::crossref;

namespace {

static scoped_ptr<DictionaryValue> CreateTestTypeDictionary() {
  DictionaryValue* value(new DictionaryValue());
  value->SetWithoutPathExpansion("number", Value::CreateDoubleValue(1.1));
  value->SetWithoutPathExpansion("integer", Value::CreateIntegerValue(4));
  value->SetWithoutPathExpansion("string", Value::CreateStringValue("bling"));
  value->SetWithoutPathExpansion("boolean", Value::CreateBooleanValue(true));
  return scoped_ptr<DictionaryValue>(value);
}

}  // namespace

TEST(JsonSchemaCompilerCrossrefTest, TestTypeOptionalParamCreate) {
  scoped_ptr<ListValue> params_value(new ListValue());
  scoped_ptr<DictionaryValue> test_type_value = CreateTestTypeDictionary();
  params_value->Append(test_type_value.release());
  scoped_ptr<TestTypeOptionalParam::Params> params(
      TestTypeOptionalParam::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_TRUE(params->test_type.get());
  EXPECT_TRUE(
      CreateTestTypeDictionary()->Equals(params->test_type->ToValue().get()));
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeOptionalParamFail) {
  scoped_ptr<ListValue> params_value(new ListValue());
  scoped_ptr<DictionaryValue> test_type_value = CreateTestTypeDictionary();
  test_type_value->RemoveWithoutPathExpansion("number", NULL);
  params_value->Append(test_type_value.release());
  scoped_ptr<TestTypeOptionalParam::Params> params(
      TestTypeOptionalParam::Params::Create(*params_value));
  EXPECT_FALSE(params.get());
}

TEST(JsonSchemaCompilerCrossrefTest, GetTestType) {
  scoped_ptr<DictionaryValue> value = CreateTestTypeDictionary();
  scoped_ptr<test::api::simple_api::TestType> test_type(
      new test::api::simple_api::TestType());
  EXPECT_TRUE(
      test::api::simple_api::TestType::Populate(*value, test_type.get()));
  scoped_ptr<Value> result(GetTestType::Result::Create(*test_type));
  EXPECT_TRUE(value->Equals(result.get()));
}
