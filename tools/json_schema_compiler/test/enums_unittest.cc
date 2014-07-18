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
    EXPECT_EQ(EnumType::TYPE_ONE, enum_type.type);
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
    InlineAndReferenceEnum enumeration;
    base::DictionaryValue value;
    ASSERT_FALSE(InlineAndReferenceEnum::Populate(value, &enumeration));

    value.Set("inline_enum", new base::StringValue("test2"));
    ASSERT_FALSE(InlineAndReferenceEnum::Populate(value, &enumeration));

    value.Set("reference_enum", new base::StringValue("one"));
    ASSERT_TRUE(InlineAndReferenceEnum::Populate(value, &enumeration));
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
    ReturnsEnum::Results::State state = ReturnsEnum::Results::STATE_FOO;
    scoped_ptr<base::Value> result(
        new base::StringValue(ReturnsEnum::Results::ToString(state)));
    scoped_ptr<base::Value> expected(new base::StringValue("foo"));
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    ReturnsEnum::Results::State state = ReturnsEnum::Results::STATE_FOO;
    scoped_ptr<base::ListValue> results = ReturnsEnum::Results::Create(state);
    base::ListValue expected;
    expected.Append(new base::StringValue("foo"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsTwoEnumsCreate) {
  {
    scoped_ptr<base::ListValue> results = ReturnsTwoEnums::Results::Create(
        ReturnsTwoEnums::Results::FIRST_STATE_FOO,
        ReturnsTwoEnums::Results::SECOND_STATE_HAM);
    base::ListValue expected;
    expected.Append(new base::StringValue("foo"));
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
    EXPECT_EQ(OptionalEnumType::TYPE_TWO, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    OptionalEnumType enum_type;
    base::DictionaryValue value;
    EXPECT_TRUE(OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(OptionalEnumType::TYPE_NONE, enum_type.type);
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
    params_value.Append(new base::StringValue("baz"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesEnum::Params::STATE_BAZ, params->state);
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
    params_value.Append(List(new base::StringValue("foo"),
                             new base::StringValue("bar")).release());
    scoped_ptr<TakesEnumArray::Params> params(
        TakesEnumArray::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(TakesEnumArray::Params::VALUES_TYPE_FOO, params->values[0]);
    EXPECT_EQ(TakesEnumArray::Params::VALUES_TYPE_BAR, params->values[1]);
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
    params_value.Append(new base::StringValue("baz"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesOptionalEnum::Params::STATE_BAZ, params->state);
  }
  {
    base::ListValue params_value;
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesOptionalEnum::Params::STATE_NONE, params->state);
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
    params_value.Append(new base::StringValue("foo"));
    params_value.Append(new base::StringValue("foo"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_FOO, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_FOO, params->type);
  }
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("foo"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_FOO, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_NONE, params->type);
  }
  {
    base::ListValue params_value;
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_NONE, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_NONE, params->type);
  }
  {
    base::ListValue params_value;
    params_value.Append(new base::StringValue("baz"));
    params_value.Append(new base::StringValue("invalid"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnEnumFiredCreate) {
  {
    OnEnumFired::SomeEnum some_enum = OnEnumFired::SOME_ENUM_FOO;
    scoped_ptr<base::Value> result(
        new base::StringValue(OnEnumFired::ToString(some_enum)));
    scoped_ptr<base::Value> expected(new base::StringValue("foo"));
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    OnEnumFired::SomeEnum some_enum = OnEnumFired::SOME_ENUM_FOO;
    scoped_ptr<base::ListValue> results(OnEnumFired::Create(some_enum));
    base::ListValue expected;
    expected.Append(new base::StringValue("foo"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnTwoEnumsFiredCreate) {
  {
    scoped_ptr<base::Value> results(OnTwoEnumsFired::Create(
        OnTwoEnumsFired::FIRST_ENUM_FOO,
        OnTwoEnumsFired::SECOND_ENUM_HAM));
    base::ListValue expected;
    expected.Append(new base::StringValue("foo"));
    expected.Append(new base::StringValue("ham"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}
