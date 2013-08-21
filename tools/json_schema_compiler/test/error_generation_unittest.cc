// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/error_generation.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

using namespace test::api::error_generation;
using base::FundamentalValue;
using json_schema_compiler::test_util::Dictionary;
using json_schema_compiler::test_util::List;

template <typename T>
base::string16 GetPopulateError(const base::Value& value) {
  base::string16 error;
  T test_type;
  T::Populate(value, &test_type, &error);
  return error;
}

testing::AssertionResult EqualsUtf16(const std::string& expected,
                                     const base::string16& actual) {
  if (ASCIIToUTF16(expected) != actual)
    return testing::AssertionFailure() << expected << " != " << actual;
  return testing::AssertionSuccess();
}

// GenerateTypePopulate errors

TEST(JsonSchemaCompilerErrorTest, RequiredPropertyPopulate) {
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "string", new StringValue("bling"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<TestType>(*value)));
  }
  {
    scoped_ptr<base::BinaryValue> value(new base::BinaryValue());
    EXPECT_TRUE(EqualsUtf16("expected dictionary, got binary",
        GetPopulateError<TestType>(*value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, UnexpectedTypePopulation) {
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    EXPECT_TRUE(EqualsUtf16("",
        GetPopulateError<ChoiceType::Integers>(*value)));
  }
  {
    scoped_ptr<base::BinaryValue> value(new base::BinaryValue());
    EXPECT_TRUE(EqualsUtf16("expected integers or integer, got binary",
        GetPopulateError<ChoiceType::Integers>(*value)));
  }
}

// GenerateTypePopulateProperty errors

TEST(JsonSchemaCompilerErrorTest, TypeIsRequired) {
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "integers", new FundamentalValue(5));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<ChoiceType>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
    EXPECT_TRUE(EqualsUtf16("'integers' is required",
        GetPopulateError<ChoiceType>(*value)));
  }
}

// GenerateParamsCheck errors

TEST(JsonSchemaCompilerErrorTest, TooManyParameters) {
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5));
    EXPECT_TRUE(TestFunction::Params::Create(*params_value));
  }
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5),
        new FundamentalValue(5));
    base::string16 error;
    EXPECT_FALSE(TestFunction::Params::Create(*params_value, &error));
    EXPECT_TRUE(EqualsUtf16("expected 1 arguments, got 2", error));
  }
}

// GenerateFunctionParamsCreate errors

TEST(JsonSchemaCompilerErrorTest, ParamIsRequired) {
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5));
    EXPECT_TRUE(TestFunction::Params::Create(*params_value));
  }
  {
    scoped_ptr<base::ListValue> params_value = List(
        Value::CreateNullValue());
    base::string16 error;
    EXPECT_FALSE(TestFunction::Params::Create(*params_value, &error));
    EXPECT_TRUE(EqualsUtf16("'num' is required", error));
  }
}

// GeneratePopulateVariableFromValue errors

TEST(JsonSchemaCompilerErrorTest, WrongPropertyValueType) {
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
      "string", new StringValue("yes"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<TestType>(*value)));
  }
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "string", new FundamentalValue(1.1));
    EXPECT_TRUE(EqualsUtf16("'string': expected string, got number",
        GetPopulateError<TestType>(*value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, WrongParameterCreationType) {
  {
    scoped_ptr<base::ListValue> params_value = List(
        new StringValue("Yeah!"));
    EXPECT_TRUE(TestString::Params::Create(*params_value));
  }
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5));
    base::string16 error;
    EXPECT_FALSE(TestTypeInObject::Params::Create(*params_value, &error));
    EXPECT_TRUE(EqualsUtf16("'paramObject': expected dictionary, got integer",
        error));
  }
}

TEST(JsonSchemaCompilerErrorTest, WrongTypeValueType) {
  {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<ObjectType>(*value)));
  }
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "otherType", new FundamentalValue(1.1));
    EXPECT_TRUE(EqualsUtf16("'otherType': expected dictionary, got number",
        GetPopulateError<ObjectType>(*value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, UnableToPopulateArray) {
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5));
    EXPECT_TRUE(EqualsUtf16("",
        GetPopulateError<ChoiceType::Integers>(*params_value)));
  }
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5),
        new FundamentalValue(false));
    EXPECT_TRUE(EqualsUtf16("unable to populate array 'integers'",
        GetPopulateError<ChoiceType::Integers>(*params_value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, BinaryTypeExpected) {
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "data", new base::BinaryValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<BinaryData>(*value)));
  }
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "data", new FundamentalValue(1.1));
    EXPECT_TRUE(EqualsUtf16("'data': expected binary, got number",
        GetPopulateError<BinaryData>(*value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, ListExpected) {
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "TheArray", new base::ListValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<ArrayObject>(*value)));
  }
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "TheArray", new FundamentalValue(5));
    EXPECT_TRUE(EqualsUtf16("'TheArray': expected list, got integer",
        GetPopulateError<ArrayObject>(*value)));
  }
}

// GenerateStringToEnumConversion errors

TEST(JsonSchemaCompilerErrorTest, BadEnumValue) {
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "enumeration", new StringValue("one"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<HasEnumeration>(*value)));
  }
  {
    scoped_ptr<DictionaryValue> value = Dictionary(
        "enumeration", new StringValue("bad sauce"));
    EXPECT_TRUE(EqualsUtf16("'enumeration': expected \"one\" or \"two\" "
              "or \"three\", got \"bad sauce\"",
        GetPopulateError<HasEnumeration>(*value)));
  }
}
