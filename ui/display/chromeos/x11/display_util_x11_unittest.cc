// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/display_util_x11.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(DisplayUtilX11Test, GetOutputTypeFromName) {
  EXPECT_EQ(OUTPUT_TYPE_INTERNAL, GetOutputTypeFromName("LVDS"));
  EXPECT_EQ(OUTPUT_TYPE_INTERNAL, GetOutputTypeFromName("eDP"));
  EXPECT_EQ(OUTPUT_TYPE_INTERNAL, GetOutputTypeFromName("DSI"));
  EXPECT_EQ(OUTPUT_TYPE_INTERNAL, GetOutputTypeFromName("LVDSxx"));
  EXPECT_EQ(OUTPUT_TYPE_INTERNAL, GetOutputTypeFromName("eDPzz"));
  EXPECT_EQ(OUTPUT_TYPE_INTERNAL, GetOutputTypeFromName("DSIyy"));

  EXPECT_EQ(OUTPUT_TYPE_VGA, GetOutputTypeFromName("VGA"));
  EXPECT_EQ(OUTPUT_TYPE_VGA, GetOutputTypeFromName("VGAxx"));
  EXPECT_EQ(OUTPUT_TYPE_HDMI, GetOutputTypeFromName("HDMI"));
  EXPECT_EQ(OUTPUT_TYPE_HDMI, GetOutputTypeFromName("HDMIyy"));
  EXPECT_EQ(OUTPUT_TYPE_DVI, GetOutputTypeFromName("DVI"));
  EXPECT_EQ(OUTPUT_TYPE_DVI, GetOutputTypeFromName("DVIzz"));
  EXPECT_EQ(OUTPUT_TYPE_DISPLAYPORT, GetOutputTypeFromName("DP"));
  EXPECT_EQ(OUTPUT_TYPE_DISPLAYPORT, GetOutputTypeFromName("DPww"));

  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("xyz"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("abcLVDS"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("cdeeDP"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("abcDSI"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("LVD"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("eD"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("DS"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("VG"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("HDM"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("DV"));
  EXPECT_EQ(OUTPUT_TYPE_UNKNOWN, GetOutputTypeFromName("D"));
}

}  // namespace ui
