// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test ScreenTest;

TEST_F(ScreenTest, GetPrimaryMonitorSize) {
  // We aren't actually testing that it's correct, just that it's sane.
  const gfx::Size size = gfx::Screen::GetPrimaryMonitor().size();
  EXPECT_GE(size.width(), 1);
  EXPECT_GE(size.height(), 1);
}

TEST_F(ScreenTest, GetNumMonitors) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(gfx::Screen::GetNumMonitors(), 1);
}

}  // namespace
