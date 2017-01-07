// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/x11/display_util_x11.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace display {

TEST(DisplayUtilX11Test, GetDisplayConnectionTypeFromName) {
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL,
            GetDisplayConnectionTypeFromName("LVDS"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL,
            GetDisplayConnectionTypeFromName("eDP"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL,
            GetDisplayConnectionTypeFromName("DSI"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL,
            GetDisplayConnectionTypeFromName("LVDSxx"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL,
            GetDisplayConnectionTypeFromName("eDPzz"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL,
            GetDisplayConnectionTypeFromName("DSIyy"));

  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_VGA,
            GetDisplayConnectionTypeFromName("VGA"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_VGA,
            GetDisplayConnectionTypeFromName("VGAxx"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI,
            GetDisplayConnectionTypeFromName("HDMI"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI,
            GetDisplayConnectionTypeFromName("HDMIyy"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_DVI,
            GetDisplayConnectionTypeFromName("DVI"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_DVI,
            GetDisplayConnectionTypeFromName("DVIzz"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
            GetDisplayConnectionTypeFromName("DP"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
            GetDisplayConnectionTypeFromName("DPww"));

  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("xyz"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("abcLVDS"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("cdeeDP"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("abcDSI"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("LVD"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("eD"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("DS"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("VG"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("HDM"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("DV"));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN,
            GetDisplayConnectionTypeFromName("D"));
}

}  // namespace display
