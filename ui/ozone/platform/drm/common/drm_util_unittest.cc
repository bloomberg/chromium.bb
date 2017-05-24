// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

class DrmUtilTest : public testing::Test {};

TEST_F(DrmUtilTest, RoundTripEquivalence) {
  DisplayMode_Params orig_params;

  orig_params.size = gfx::Size(101, 42);
  orig_params.is_interlaced = true;
  orig_params.refresh_rate = 3.14;

  auto udm = CreateDisplayModeFromParams(orig_params);
  auto roundtrip_params = GetDisplayModeParams(*udm.get());

  EXPECT_EQ(orig_params.size.width(), roundtrip_params.size.width());
  EXPECT_EQ(orig_params.size.height(), roundtrip_params.size.height());
  EXPECT_EQ(orig_params.is_interlaced, roundtrip_params.is_interlaced);
  EXPECT_EQ(orig_params.refresh_rate, roundtrip_params.refresh_rate);
}

}  // namespace ui
