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
  if (base::ASCIIToUTF16(expected) != actual)
    return testing::AssertionFailure() << expected << " != " << actual;
  return testing::AssertionSuccess();
}

// GenerateTypePopulate errors

TEST(JsonSchemaCompilerErrorTest, RequiredPropertyPopulate) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "string", new base::StringValue("bling"));
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
    scoped_ptr<base::DictionaryValue> value = Dictionary(
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
    base::string16 error;
    EXPECT_TRUE(TestFunction::Params::Create(*params_value, &error));
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
    base::string16 error;
    EXPECT_TRUE(TestFunction::Params::Create(*params_value, &error));
  }
  {
    scoped_ptr<base::ListValue> params_value = List(
        base::Value::CreateNullValue());
    base::string16 error;
    EXPECT_FALSE(TestFunction::Params::Create(*params_value, &error));
    EXPECT_TRUE(EqualsUtf16("'num' is required", error));
  }
}

// GeneratePopulateVariableFromValue errors

TEST(JsonSchemaCompilerErrorTest, WrongPropertyValueType) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
      "string", new base::StringValue("yes"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<TestType>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "string", new FundamentalValue(1.1));
    EXPECT_TRUE(EqualsUtf16("'string': expected string, got number",
        GetPopulateError<TestType>(*value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, WrongParameterCreationType) {
  {
    base::string16 error;
    scoped_ptr<base::ListValue> params_value = List(
        new base::StringValue("Yeah!"));
    EXPECT_TRUE(TestString::Params::Create(*params_value, &error));
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
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "otherType", new FundamentalValue(1.1));
    ObjectType out;
    base::string16 error;
    EXPECT_TRUE(ObjectType::Populate(*value, &out, &error));
    EXPECT_TRUE(EqualsUtf16("'otherType': expected dictionary, got number",
        error));
    EXPECT_EQ(NULL, out.other_type.get());
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
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "data", new base::BinaryValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<BinaryData>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "data", new FundamentalValue(1.1));
    EXPECT_TRUE(EqualsUtf16("'data': expected binary, got number",
        GetPopulateError<BinaryData>(*value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, ListExpected) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "TheArray", new base::ListValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<ArrayObject>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "TheArray", new FundamentalValue(5));
    EXPECT_TRUE(EqualsUtf16("'TheArray': expected list, got integer",
        GetPopulateError<ArrayObject>(*value)));
  }
}

// GenerateStringToEnumConversion errors

TEST(JsonSchemaCompilerErrorTest, BadEnumValue) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "enumeration", new base::StringValue("one"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<HasEnumeration>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "enumeration", new base::StringValue("bad sauce"));
    EXPECT_TRUE(EqualsUtf16("'Enumeration': expected \"one\" or \"two\" "
              "or \"three\", got \"bad sauce\"",
        GetPopulateError<HasEnumeration>(*value)));
  }
}

// Warn but don't fail out errors

TEST(JsonSchemaCompilerErrorTest, WarnOnOptionalFailure) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "string", new base::StringValue("bling"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<OptionalTestType>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "string", new base::FundamentalValue(1));

    OptionalTestType out;
    base::string16 error;
    EXPECT_TRUE(OptionalTestType::Populate(*value, &out, &error));
    EXPECT_TRUE(EqualsUtf16("'string': expected string, got integer",
        error));
    EXPECT_EQ(NULL, out.string.get());
  }
}

TEST(JsonSchemaCompilerErrorTest, OptionalBinaryTypeFailure) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "data", new base::BinaryValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<OptionalBinaryData>(*value)));
  }
  {
    // There's a bug with silent failures if the key doesn't exist.
    scoped_ptr<base::DictionaryValue> value = Dictionary("data",
        new base::FundamentalValue(1));

    OptionalBinaryData out;
    base::string16 error;
    EXPECT_TRUE(OptionalBinaryData::Populate(*value, &out, &error));
    EXPECT_TRUE(EqualsUtf16("'data': expected binary, got integer",
        error));
    EXPECT_EQ(NULL, out.data.get());
  }
}

TEST(JsonSchemaCompilerErrorTest, OptionalArrayTypeFailure) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "TheArray", new base::ListValue());
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<ArrayObject>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "TheArray", new FundamentalValue(5));
    ArrayObject out;
    base::string16 error;
    EXPECT_TRUE(ArrayObject::Populate(*value, &out, &error));
    EXPECT_TRUE(EqualsUtf16("'TheArray': expected list, got integer",
        error));
    EXPECT_EQ(NULL, out.the_array.get());
  }
}

TEST(JsonSchemaCompilerErrorTest, OptionalUnableToPopulateArray) {
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5));
    EXPECT_TRUE(EqualsUtf16("",
        GetPopulateError<OptionalChoiceType::Integers>(*params_value)));
  }
  {
    scoped_ptr<base::ListValue> params_value = List(
        new FundamentalValue(5),
        new FundamentalValue(false));
    OptionalChoiceType::Integers out;
    base::string16 error;
    EXPECT_TRUE(OptionalChoiceType::Integers::Populate(*params_value, &out,
        &error));
    EXPECT_TRUE(EqualsUtf16("unable to populate array 'integers'",
        error));
    EXPECT_EQ(NULL, out.as_integer.get());
  }
}

TEST(JsonSchemaCompilerErrorTest, MultiplePopulationErrors) {
  {

    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "TheArray", new FundamentalValue(5));
    ArrayObject out;
    base::string16 error;
    EXPECT_TRUE(ArrayObject::Populate(*value, &out, &error));
    EXPECT_TRUE(EqualsUtf16("'TheArray': expected list, got integer",
        error));
    EXPECT_EQ(NULL, out.the_array.get());

    EXPECT_TRUE(ArrayObject::Populate(*value, &out, &error));
    EXPECT_TRUE(EqualsUtf16("'TheArray': expected list, got integer; "
        "'TheArray': expected list, got integer",
        error));
    EXPECT_EQ(NULL, out.the_array.get());
  }
}

TEST(JsonSchemaCompilerErrorTest, TooManyKeys) {
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
      "string", new base::StringValue("yes"));
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<TestType>(*value)));
  }
  {
    scoped_ptr<base::DictionaryValue> value = Dictionary(
        "string", new base::StringValue("yes"),
        "ohno", new base::StringValue("many values"));
    EXPECT_TRUE(EqualsUtf16("found unexpected key 'ohno'",
        GetPopulateError<TestType>(*value)));
  }
}
