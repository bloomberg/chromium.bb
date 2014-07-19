// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/arrays.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/enums.h"

using namespace test::api::arrays;

namespace {

// TODO(calamity): Change to AppendString etc once kalman's patch goes through
static scoped_ptr<base::DictionaryValue> CreateBasicArrayTypeDictionary() {
  base::DictionaryValue* value = new base::DictionaryValue();
  base::ListValue* strings_value = new base::ListValue();
  strings_value->Append(new base::StringValue("a"));
  strings_value->Append(new base::StringValue("b"));
  strings_value->Append(new base::StringValue("c"));
  strings_value->Append(new base::StringValue("it's easy as"));
  base::ListValue* integers_value = new base::ListValue();
  integers_value->Append(new base::FundamentalValue(1));
  integers_value->Append(new base::FundamentalValue(2));
  integers_value->Append(new base::FundamentalValue(3));
  base::ListValue* booleans_value = new base::ListValue();
  booleans_value->Append(new base::FundamentalValue(false));
  booleans_value->Append(new base::FundamentalValue(true));
  base::ListValue* numbers_value = new base::ListValue();
  numbers_value->Append(new base::FundamentalValue(6.1));
  value->Set("numbers", numbers_value);
  value->Set("booleans", booleans_value);
  value->Set("strings", strings_value);
  value->Set("integers", integers_value);
  return scoped_ptr<base::DictionaryValue>(value);
}

static base::Value* CreateItemValue(int val) {
  base::DictionaryValue* value(new base::DictionaryValue());
  value->Set("val", new base::FundamentalValue(val));
  return value;
}

}  // namespace

TEST(JsonSchemaCompilerArrayTest, BasicArrayType) {
  {
    scoped_ptr<base::DictionaryValue> value = CreateBasicArrayTypeDictionary();
    scoped_ptr<BasicArrayType> basic_array_type(new BasicArrayType());
    ASSERT_TRUE(BasicArrayType::Populate(*value, basic_array_type.get()));
    EXPECT_TRUE(value->Equals(basic_array_type->ToValue().get()));
  }
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayType) {
  // { "types": ["one", "two", "three"] }
  base::ListValue* types = new base::ListValue();
  types->AppendString("one");
  types->AppendString("two");
  types->AppendString("three");
  base::DictionaryValue value;
  value.Set("types", types);

  EnumArrayType enum_array_type;

  // Test Populate.
  ASSERT_TRUE(EnumArrayType::Populate(value, &enum_array_type));
  {
    EnumArrayType::TypesType enums[] = {
      EnumArrayType::TYPES_TYPE_ONE,
      EnumArrayType::TYPES_TYPE_TWO,
      EnumArrayType::TYPES_TYPE_THREE,
    };
    std::vector<EnumArrayType::TypesType> enums_vector(
        enums, enums + arraysize(enums));
    EXPECT_EQ(enums_vector, enum_array_type.types);
  }

  // Test ToValue.
  scoped_ptr<base::Value> as_value(enum_array_type.ToValue());
  EXPECT_TRUE(value.Equals(as_value.get())) << value << " != " << *as_value;
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayReference) {
  // { "types": ["one", "two", "three"] }
  base::ListValue* types = new base::ListValue();
  types->AppendString("one");
  types->AppendString("two");
  types->AppendString("three");
  base::DictionaryValue value;
  value.Set("types", types);

  EnumArrayReference enum_array_reference;

  // Test Populate.
  ASSERT_TRUE(EnumArrayReference::Populate(value, &enum_array_reference));

  Enumeration expected_types[] = {ENUMERATION_ONE, ENUMERATION_TWO,
                                  ENUMERATION_THREE};
  EXPECT_EQ(std::vector<Enumeration>(
                expected_types, expected_types + arraysize(expected_types)),
            enum_array_reference.types);

  // Test ToValue.
  scoped_ptr<base::Value> as_value(enum_array_reference.ToValue());
  EXPECT_TRUE(value.Equals(as_value.get())) << value << " != " << *as_value;
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayMixed) {
  // { "types": ["one", "two", "three"] }
  base::ListValue* inline_enums = new base::ListValue();
  inline_enums->AppendString("one");
  inline_enums->AppendString("two");
  inline_enums->AppendString("three");

  base::ListValue* infile_enums = new base::ListValue();
  infile_enums->AppendString("one");
  infile_enums->AppendString("two");
  infile_enums->AppendString("three");

  base::ListValue* external_enums = new base::ListValue();
  external_enums->AppendString("one");
  external_enums->AppendString("two");
  external_enums->AppendString("three");

  base::DictionaryValue value;
  value.Set("inline_enums", inline_enums);
  value.Set("infile_enums", infile_enums);
  value.Set("external_enums", external_enums);

  EnumArrayMixed enum_array_mixed;

  // Test Populate.
  ASSERT_TRUE(EnumArrayMixed::Populate(value, &enum_array_mixed));

  EnumArrayMixed::Inline_enumsType expected_inline_types[] = {
      EnumArrayMixed::INLINE_ENUMS_TYPE_ONE,
      EnumArrayMixed::INLINE_ENUMS_TYPE_TWO,
      EnumArrayMixed::INLINE_ENUMS_TYPE_THREE};
  EXPECT_EQ(std::vector<EnumArrayMixed::Inline_enumsType>(
                expected_inline_types,
                expected_inline_types + arraysize(expected_inline_types)),
            enum_array_mixed.inline_enums);

  Enumeration expected_infile_types[] = {ENUMERATION_ONE, ENUMERATION_TWO,
                                         ENUMERATION_THREE};
  EXPECT_EQ(std::vector<Enumeration>(
                expected_infile_types,
                expected_infile_types + arraysize(expected_infile_types)),
            enum_array_mixed.infile_enums);

  test::api::enums::Enumeration expected_external_types[] = {
      test::api::enums::ENUMERATION_ONE, test::api::enums::ENUMERATION_TWO,
      test::api::enums::ENUMERATION_THREE};
  EXPECT_EQ(std::vector<test::api::enums::Enumeration>(
                expected_external_types,
                expected_external_types + arraysize(expected_external_types)),
            enum_array_mixed.external_enums);

  // Test ToValue.
  scoped_ptr<base::Value> as_value(enum_array_mixed.ToValue());
  EXPECT_TRUE(value.Equals(as_value.get())) << value << " != " << *as_value;
}

TEST(JsonSchemaCompilerArrayTest, OptionalEnumArrayType) {
  {
    std::vector<OptionalEnumArrayType::TypesType> enums;
    enums.push_back(OptionalEnumArrayType::TYPES_TYPE_ONE);
    enums.push_back(OptionalEnumArrayType::TYPES_TYPE_TWO);
    enums.push_back(OptionalEnumArrayType::TYPES_TYPE_THREE);

    scoped_ptr<base::ListValue> types(new base::ListValue());
    for (size_t i = 0; i < enums.size(); ++i) {
      types->Append(new base::StringValue(
          OptionalEnumArrayType::ToString(enums[i])));
    }

    base::DictionaryValue value;
    value.Set("types", types.release());

    OptionalEnumArrayType enum_array_type;
    ASSERT_TRUE(OptionalEnumArrayType::Populate(value, &enum_array_type));
    EXPECT_EQ(enums, *enum_array_type.types);
  }
  {
    base::DictionaryValue value;
    scoped_ptr<base::ListValue> enum_array(new base::ListValue());
    enum_array->Append(new base::StringValue("invalid"));

    value.Set("types", enum_array.release());
    OptionalEnumArrayType enum_array_type;
    ASSERT_FALSE(OptionalEnumArrayType::Populate(value, &enum_array_type));
    EXPECT_TRUE(enum_array_type.types->empty());
  }
}

TEST(JsonSchemaCompilerArrayTest, RefArrayType) {
  {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
    scoped_ptr<base::ListValue> ref_array(new base::ListValue());
    ref_array->Append(CreateItemValue(1));
    ref_array->Append(CreateItemValue(2));
    ref_array->Append(CreateItemValue(3));
    value->Set("refs", ref_array.release());
    scoped_ptr<RefArrayType> ref_array_type(new RefArrayType());
    EXPECT_TRUE(RefArrayType::Populate(*value, ref_array_type.get()));
    ASSERT_EQ(3u, ref_array_type->refs.size());
    EXPECT_EQ(1, ref_array_type->refs[0]->val);
    EXPECT_EQ(2, ref_array_type->refs[1]->val);
    EXPECT_EQ(3, ref_array_type->refs[2]->val);
  }
  {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
    scoped_ptr<base::ListValue> not_ref_array(new base::ListValue());
    not_ref_array->Append(CreateItemValue(1));
    not_ref_array->Append(new base::FundamentalValue(3));
    value->Set("refs", not_ref_array.release());
    scoped_ptr<RefArrayType> ref_array_type(new RefArrayType());
    EXPECT_FALSE(RefArrayType::Populate(*value, ref_array_type.get()));
  }
}

TEST(JsonSchemaCompilerArrayTest, IntegerArrayParamsCreate) {
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  scoped_ptr<base::ListValue> integer_array(new base::ListValue());
  integer_array->Append(new base::FundamentalValue(2));
  integer_array->Append(new base::FundamentalValue(4));
  integer_array->Append(new base::FundamentalValue(8));
  params_value->Append(integer_array.release());
  scoped_ptr<IntegerArray::Params> params(
      IntegerArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(3u, params->nums.size());
  EXPECT_EQ(2, params->nums[0]);
  EXPECT_EQ(4, params->nums[1]);
  EXPECT_EQ(8, params->nums[2]);
}

TEST(JsonSchemaCompilerArrayTest, AnyArrayParamsCreate) {
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  scoped_ptr<base::ListValue> any_array(new base::ListValue());
  any_array->Append(new base::FundamentalValue(1));
  any_array->Append(new base::StringValue("test"));
  any_array->Append(CreateItemValue(2));
  params_value->Append(any_array.release());
  scoped_ptr<AnyArray::Params> params(
      AnyArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(3u, params->anys.size());
  int int_temp = 0;
  EXPECT_TRUE(params->anys[0]->GetAsInteger(&int_temp));
  EXPECT_EQ(1, int_temp);
}

TEST(JsonSchemaCompilerArrayTest, ObjectArrayParamsCreate) {
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  scoped_ptr<base::ListValue> item_array(new base::ListValue());
  item_array->Append(CreateItemValue(1));
  item_array->Append(CreateItemValue(2));
  params_value->Append(item_array.release());
  scoped_ptr<ObjectArray::Params> params(
      ObjectArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(2u, params->objects.size());
  EXPECT_EQ(1, params->objects[0]->additional_properties["val"]);
  EXPECT_EQ(2, params->objects[1]->additional_properties["val"]);
}

TEST(JsonSchemaCompilerArrayTest, RefArrayParamsCreate) {
  scoped_ptr<base::ListValue> params_value(new base::ListValue());
  scoped_ptr<base::ListValue> item_array(new base::ListValue());
  item_array->Append(CreateItemValue(1));
  item_array->Append(CreateItemValue(2));
  params_value->Append(item_array.release());
  scoped_ptr<RefArray::Params> params(
      RefArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(2u, params->refs.size());
  EXPECT_EQ(1, params->refs[0]->val);
  EXPECT_EQ(2, params->refs[1]->val);
}

TEST(JsonSchemaCompilerArrayTest, ReturnIntegerArrayResultCreate) {
  std::vector<int> integers;
  integers.push_back(1);
  integers.push_back(2);
  scoped_ptr<base::ListValue> results =
      ReturnIntegerArray::Results::Create(integers);

  base::ListValue expected;
  base::ListValue* expected_argument = new base::ListValue();
  expected_argument->Append(new base::FundamentalValue(1));
  expected_argument->Append(new base::FundamentalValue(2));
  expected.Append(expected_argument);
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerArrayTest, ReturnRefArrayResultCreate) {
  std::vector<linked_ptr<Item> > items;
  items.push_back(linked_ptr<Item>(new Item()));
  items.push_back(linked_ptr<Item>(new Item()));
  items[0]->val = 1;
  items[1]->val = 2;
  scoped_ptr<base::ListValue> results =
      ReturnRefArray::Results::Create(items);

  base::ListValue expected;
  base::ListValue* expected_argument = new base::ListValue();
  base::DictionaryValue* first = new base::DictionaryValue();
  first->SetInteger("val", 1);
  expected_argument->Append(first);
  base::DictionaryValue* second = new base::DictionaryValue();
  second->SetInteger("val", 2);
  expected_argument->Append(second);
  expected.Append(expected_argument);
  EXPECT_TRUE(results->Equals(&expected));
}
