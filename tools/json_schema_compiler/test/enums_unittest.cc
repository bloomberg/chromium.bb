// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/enums.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::enums;

TEST(JsonSchemaCompilerEnumsTest, EnumTypePopulate) {
  {
    scoped_ptr<EnumType> enum_type(new EnumType());
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    value->Set("type", Value::CreateStringValue("one"));
    EXPECT_TRUE(EnumType::Populate(*value, enum_type.get()));
    EXPECT_EQ(EnumType::TYPE_ONE, enum_type->type);
    EXPECT_TRUE(value->Equals(enum_type->ToValue().get()));
  }
  {
    scoped_ptr<EnumType> enum_type(new EnumType());
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    value->Set("type", Value::CreateStringValue("invalid"));
    EXPECT_FALSE(EnumType::Populate(*value, enum_type.get()));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OptionalEnumTypePopulate) {
  {
    scoped_ptr<OptionalEnumType> enum_type(new OptionalEnumType());
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    value->Set("type", Value::CreateStringValue("two"));
    EXPECT_TRUE(OptionalEnumType::Populate(*value, enum_type.get()));
    EXPECT_EQ(OptionalEnumType::TYPE_TWO, enum_type->type);
    EXPECT_TRUE(value->Equals(enum_type->ToValue().get()));
  }
  {
    scoped_ptr<OptionalEnumType> enum_type(new OptionalEnumType());
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    EXPECT_TRUE(OptionalEnumType::Populate(*value, enum_type.get()));
    EXPECT_EQ(OptionalEnumType::TYPE_NONE, enum_type->type);
    EXPECT_TRUE(value->Equals(enum_type->ToValue().get()));
  }
  {
    scoped_ptr<OptionalEnumType> enum_type(new OptionalEnumType());
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    value->Set("type", Value::CreateStringValue("invalid"));
    EXPECT_FALSE(OptionalEnumType::Populate(*value, enum_type.get()));
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("baz"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesEnum::Params::STATE_BAZ, params->state);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("invalid"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesOptionalEnumParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("baz"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesOptionalEnum::Params::STATE_BAZ, params->state);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesOptionalEnum::Params::STATE_NONE, params->state);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("invalid"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesMultipleOptionalEnumsParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("foo"));
    params_value->Append(Value::CreateStringValue("foo"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_FOO, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_FOO, params->type);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("foo"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_FOO, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_NONE, params->type);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_NONE, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_NONE, params->type);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("baz"));
    params_value->Append(Value::CreateStringValue("invalid"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}
