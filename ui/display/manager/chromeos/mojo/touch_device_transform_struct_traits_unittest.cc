// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/mojo/touch_device_transform_struct_traits.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/chromeos/mojo/touch_device_transform.mojom.h"
#include "ui/display/manager/chromeos/touch_device_transform.h"

namespace display {

TEST(TouchDeviceTransformStructTraitsTest, SerializeAndDeserialize) {
  TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = 101;
  touch_device_transform.device_id = 202;
  touch_device_transform.transform =
      gfx::Transform(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  TouchDeviceTransform deserialized;
  ASSERT_TRUE(mojom::TouchDeviceTransform::Deserialize(
      mojom::TouchDeviceTransform::Serialize(&touch_device_transform),
      &deserialized));
  EXPECT_EQ(touch_device_transform.display_id, deserialized.display_id);
  EXPECT_EQ(touch_device_transform.device_id, deserialized.device_id);
  EXPECT_EQ(touch_device_transform.transform, deserialized.transform);
}

}  // namespace display
