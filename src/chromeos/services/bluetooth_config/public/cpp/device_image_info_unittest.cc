// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/public/cpp/device_image_info.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {

namespace {

constexpr char kTestDefaultImage[] = "default";
constexpr char kTestLeftBudImage[] = "left_bud";
constexpr char kTestRightBudImage[] = "right_bud";
constexpr char kTestCaseImage[] = "case_image";

}  // namespace

TEST(DeviceImageInfoTest, ToAndFromDictionaryValueValid) {
  DeviceImageInfo image_info(kTestDefaultImage, kTestLeftBudImage,
                             kTestRightBudImage, kTestCaseImage);

  base::Value image_info_dict = image_info.ToDictionaryValue();
  absl::optional<DeviceImageInfo> image_info_copy =
      DeviceImageInfo::FromDictionaryValue(image_info_dict);
  EXPECT_TRUE(image_info_copy);

  EXPECT_EQ(kTestDefaultImage, image_info_copy->default_image());
  EXPECT_EQ(kTestLeftBudImage, image_info_copy->left_bud_image());
  EXPECT_EQ(kTestRightBudImage, image_info_copy->right_bud_image());
  EXPECT_EQ(kTestCaseImage, image_info_copy->case_image());
}

TEST(DeviceImageInfoTest, ToAndFromDictionaryValueValidDefaultConstructor) {
  // Ensure the default construction of DeviceImageInfo works with the
  // to/from dictionary methods.
  DeviceImageInfo image_info;

  base::Value image_info_dict = image_info.ToDictionaryValue();
  absl::optional<DeviceImageInfo> image_info_copy =
      DeviceImageInfo::FromDictionaryValue(image_info_dict);
  EXPECT_TRUE(image_info_copy);

  EXPECT_TRUE(image_info_copy->default_image().empty());
  EXPECT_TRUE(image_info_copy->left_bud_image().empty());
  EXPECT_TRUE(image_info_copy->right_bud_image().empty());
  EXPECT_TRUE(image_info_copy->case_image().empty());
}

TEST(DeviceImageInfoTest, FromDictionaryValueInvalid) {
  // Should correctly handle dictionaries with missing fields.
  base::Value invalid_dict(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(DeviceImageInfo::FromDictionaryValue(invalid_dict));

  // Should correctly handle non-dictionary values.
  base::Value not_a_dict(777);
  EXPECT_FALSE(DeviceImageInfo::FromDictionaryValue(not_a_dict));
}

}  // namespace bluetooth_config
}  // namespace chromeos