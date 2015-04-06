// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/enums.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

using namespace test::api::enums;
using json_schema_compiler::test_util::List;

TEST(JsonSchemaCompilerEnumsTest, EnumTypePopulate) {
  {
    EnumType enum_type;
    base::DictionaryValue value;
    value.Set("type", new base::StringValue("one"));
    EXPECT_TRUE(EnumType::Populate(value, &enum_type));
    EXPECT_EQ(ENUMERATION_ONE, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    EnumType enum_type;
    base::DictionaryValue value;
    value.Set("type", new base::StringValue("invalid"));
    EXPECT_FALSE(EnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsAsTypes) {
  {
    base::ListValue args;
    args.Append(new base::StringValue("one"));

    scoped_ptr<TakesEnumAsType::Params> params(
        TakesEnumAsType::Params::Create(args));
    ASSERT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_ONE, params->enumeration);

    EXPECT_TRUE(args.Equals(ReturnsEnumAsType::Results::Create(
        ENUMERATION_ONE).get()));
  }
  {
    HasEnumeration enumeration;
    EXPECT_EQ(ENUMERATION_NONE, enumeration.enumeration);
    EXPECT_EQ(ENUMERATION_NONE, enumeration.optional_enumeration);
  }
  {
    HasEnumeration enumeration;
    base::DictionaryValue value;
    ASSERT_FALSE(HasEnumeration::Populate(value, &enumeration));

    value.Set("enumeration", new base::StringValue("one"));
    ASSERT_TRUE(HasEnumeration::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));

    value.Set("optional_enumeration", new base::StringValue("two"));
    ASSERT_TRUE(HasEnumeration::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));
  }
  {
    ReferenceEnum enumeration;
    base::DictionaryValue value;
    ASSERT_FALSE(ReferenceEnum::Populate(value, &enumeration));

    value.Set("reference_enum", new base::StringValue("one"));
    ASSERT_TRUE(ReferenceEnum::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsArrayAsType) {
  {
    base::ListValue params_value;
    params_value.Append(List(new base::StringValue("one"),
                             new base::StringValue("two")).release());
    scoped_ptr<TakesEnumArrayAsType::Params> params(
        TakesEnumArrayAsType::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(ENUMERATION_TWO, params->values[1]);
  }
  {
    base::ListValue params_value;
    params_value.Append(List(new base::StringValue("invalid")).release());
    scoped_ptr<TakesEnumArrayAsType::Params> params(
        TakesEnumArrayAsType::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsEnumCreate) {
  {
    Enumeration state = ENUMERATION_ONE;
    scoped_ptr<base::Value> result(new base::StringValue(ToString(state)));
    scoped_ptr<base::Value> expected(new base::StringValue("one"));
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    Enumeration state = ENUMERATION_ONE;
    scoped_ptr<base::ListValue> results = ReturnsEnum::Results::Create(state);
    base::ListValue expected;
    expected.Append(new base::StringValue("one"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsTwoEnumsCreate) {
  {
    scoped_ptr<base::ListValue> results = ReturnsTwoEnums::Results::Create(
        ENUMERATION_ONE, OTHER_ENUMERATION_HAM);
    base::ListValue expected;
    expected.Append(new base::StringValue("one"));
    expected.Append(new base::StringValue("ham"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OptionalEnumTypePopulate) {
  {
    OptionalEnumType enum_type;
    base::DictionaryValue value;
    value.Set("type", new base::StringValue("two"));
    EXPECT_TRUE(OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(ENUMERATION_TWO, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    OptionalEnumType enum_type;
    base::DictionaryValue value;
    EXPECT_TRUE(OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(ENUMERATION_NONE, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    OptionalEnumType enum_type;
    base::DictionaryValue value;
    value.Set("type", new base::StringValue("invalid"));
    EXPECT_FALSE(OptionalEnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumParamsCreate) {
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("two"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_TWO, params->state);
  }
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("invalid"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumArrayParamsCreate) {
  {
    base::ListValue params_value;
    params_value.Append(List(new base::StringValue("one"),
                             new base::StringValue("two")).release());
    scoped_ptr<TakesEnumArray::Params> params(
        TakesEnumArray::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(ENUMERATION_TWO, params->values[1]);
  }
  {
    base::ListValue params_value;
    params_value.Append(List(new base::StringValue("invalid")).release());
    scoped_ptr<TakesEnumArray::Params> params(
        TakesEnumArray::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesOptionalEnumParamsCreate) {
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("three"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_THREE, params->state);
  }
  {
    base::ListValue params_value;
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_NONE, params->state);
  }
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("invalid"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesMultipleOptionalEnumsParamsCreate) {
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("one"));
    params_value.Append(new base::StringValue("ham"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_ONE, params->state);
    EXPECT_EQ(OTHER_ENUMERATION_HAM, params->type);
  }
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("one"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_ONE, params->state);
    EXPECT_EQ(OTHER_ENUMERATION_NONE, params->type);
  }
  {
    base::ListValue params_value;
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_NONE, params->state);
    EXPECT_EQ(OTHER_ENUMERATION_NONE, params->type);
  }
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("three"));
    params_value.Append(new base::StringValue("invalid"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnEnumFiredCreate) {
  {
    Enumeration some_enum = ENUMERATION_ONE;
    scoped_ptr<base::Value> result(new base::StringValue(ToString(some_enum)));
    scoped_ptr<base::Value> expected(new base::StringValue("one"));
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    Enumeration some_enum = ENUMERATION_ONE;
    scoped_ptr<base::ListValue> results(OnEnumFired::Create(some_enum));
    base::ListValue expected;
    expected.Append(new base::StringValue("one"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnTwoEnumsFiredCreate) {
  {
    scoped_ptr<base::Value> results(
        OnTwoEnumsFired::Create(ENUMERATION_ONE, OTHER_ENUMERATION_HAM));
    base::ListValue expected;
    expected.Append(new base::StringValue("one"));
    expected.Append(new base::StringValue("ham"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}
