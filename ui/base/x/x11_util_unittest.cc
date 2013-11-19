// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_util.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test XUtilTest;

namespace ui {

TEST_F(XUtilTest, TestBlackListedDisplay) {
  EXPECT_TRUE(IsXDisplaySizeBlackListed(10, 10));
  EXPECT_TRUE(IsXDisplaySizeBlackListed(40, 30));
  EXPECT_TRUE(IsXDisplaySizeBlackListed(50, 40));
  EXPECT_TRUE(IsXDisplaySizeBlackListed(160, 90));
  EXPECT_TRUE(IsXDisplaySizeBlackListed(160, 100));

  EXPECT_FALSE(IsXDisplaySizeBlackListed(50, 60));
  EXPECT_FALSE(IsXDisplaySizeBlackListed(100, 70));
  EXPECT_FALSE(IsXDisplaySizeBlackListed(272, 181));
}

}
