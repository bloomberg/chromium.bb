// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/field_data_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/renderer_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormFieldData;
using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

class FieldDataManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    FormFieldData field1;
    field1.id_attribute = ASCIIToUTF16("name1");
    field1.value = ASCIIToUTF16("first");
    field1.form_control_type = "text";
    field1.unique_renderer_id = FieldRendererId(1);
    control_elements_.push_back(field1);

    FormFieldData field2;
    field2.id_attribute = ASCIIToUTF16("name2");
    field2.form_control_type = "password";
    field2.unique_renderer_id = FieldRendererId(2);
    control_elements_.push_back(field2);
  }

  void TearDown() override { control_elements_.clear(); }

  std::vector<FormFieldData> control_elements_;
};

TEST_F(FieldDataManagerTest, UpdateFieldDataMap) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(
      control_elements_[0].unique_renderer_id, control_elements_[0].value,
      FieldPropertiesFlags::kUserTyped);
  const FieldRendererId id(control_elements_[0].unique_renderer_id);
  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(UTF8ToUTF16("first"), field_data_manager->GetUserTypedValue(id));
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped,
            field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(
      control_elements_[0].unique_renderer_id, UTF8ToUTF16("newvalue"),
      FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(UTF8ToUTF16("newvalue"), field_data_manager->GetUserTypedValue(id));
  FieldPropertiesMask mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled;
  EXPECT_EQ(mask, field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(
      control_elements_[1].unique_renderer_id, control_elements_[1].value,
      FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(FieldPropertiesFlags::kNoFlags,
            field_data_manager->GetFieldPropertiesMask(
                FieldRendererId(control_elements_[1].unique_renderer_id)));

  field_data_manager->ClearData();
  EXPECT_FALSE(field_data_manager->HasFieldData(id));
}

TEST_F(FieldDataManagerTest, UpdateFieldDataMapWithNullValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMapWithNullValue(
      control_elements_[0].unique_renderer_id,
      FieldPropertiesFlags::kUserTyped);
  const FieldRendererId id(control_elements_[0].unique_renderer_id);
  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(base::string16(), field_data_manager->GetUserTypedValue(id));
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped,
            field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMapWithNullValue(
      control_elements_[0].unique_renderer_id,
      FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(base::string16(), field_data_manager->GetUserTypedValue(id));
  FieldPropertiesMask mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled;
  EXPECT_EQ(mask, field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(
      control_elements_[0].unique_renderer_id, control_elements_[0].value,
      FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(UTF8ToUTF16("first"), field_data_manager->GetUserTypedValue(id));
}

TEST_F(FieldDataManagerTest, FindMachedValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(
      control_elements_[0].unique_renderer_id, control_elements_[0].value,
      FieldPropertiesFlags::kUserTyped);
  EXPECT_TRUE(
      field_data_manager->FindMachedValue(UTF8ToUTF16("first_element")));
  EXPECT_FALSE(
      field_data_manager->FindMachedValue(UTF8ToUTF16("second_element")));
}

TEST_F(FieldDataManagerTest, UpdateFieldDataMapWithAutofilledValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  const FieldRendererId id(control_elements_[0].unique_renderer_id);
  // Add a typed value to make sure it will be cleared.
  field_data_manager->UpdateFieldDataMap(id, ASCIIToUTF16("typedvalue"), 0);

  field_data_manager->UpdateFieldDataWithAutofilledValue(
      id, ASCIIToUTF16("autofilled"),
      FieldPropertiesFlags::kAutofilledOnPageLoad);

  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(base::string16(), field_data_manager->GetUserTypedValue(id));
  EXPECT_EQ(UTF8ToUTF16("autofilled"),
            field_data_manager->GetAutofilledValue(id));
  EXPECT_EQ(FieldPropertiesFlags::kAutofilledOnPageLoad,
            field_data_manager->GetFieldPropertiesMask(id));
}

}  // namespace autofill
