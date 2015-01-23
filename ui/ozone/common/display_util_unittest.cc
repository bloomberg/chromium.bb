// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/display_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

typedef testing::Test DisplayUtilTest;

namespace ui {
namespace {

// EDID from HP ZR30w
const unsigned char kTestEDID[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";

}  // namespace

TEST_F(DisplayUtilTest, BasicEDID) {
  DisplaySnapshot_Params params;
  //  -1 to skip NULL byte.
  std::vector<uint8_t> edid(kTestEDID, kTestEDID + arraysize(kTestEDID) - 1);

  // External Display
  EXPECT_TRUE(CreateSnapshotFromEDID(true, edid, &params));
  EXPECT_EQ("HP ZR30w", params.display_name);
  EXPECT_EQ(1u, params.modes.size());
  EXPECT_TRUE(params.has_current_mode);
  EXPECT_EQ("2560x1600", params.current_mode.size.ToString());
  EXPECT_EQ("641x400", params.physical_size.ToString());
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, params.type);

  // Reset
  params = DisplaySnapshot_Params();

  // External Display
  EXPECT_TRUE(CreateSnapshotFromEDID(false, edid, &params));
  EXPECT_EQ("HP ZR30w", params.display_name);
  EXPECT_EQ(1u, params.modes.size());
  EXPECT_TRUE(params.has_current_mode);
  EXPECT_EQ("2560x1600", params.current_mode.size.ToString());
  EXPECT_EQ("641x400", params.physical_size.ToString());
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_VGA, params.type);
}

TEST_F(DisplayUtilTest, EmptyEDID) {
  DisplaySnapshot_Params params;
  std::vector<uint8_t> edid;

  EXPECT_FALSE(CreateSnapshotFromEDID(true, std::vector<uint8_t>(), &params));
}

}  // namespace ui
