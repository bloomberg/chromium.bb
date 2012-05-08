// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/monitor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/insets.h"

namespace {

TEST(MonitorTest, WorkArea) {
  gfx::Monitor monitor(0, gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ("0,0 100x100", monitor.bounds().ToString());
  EXPECT_EQ("0,0 100x100", monitor.work_area().ToString());

  monitor.set_work_area(gfx::Rect(3, 4, 90, 80));
  EXPECT_EQ("0,0 100x100", monitor.bounds().ToString());
  EXPECT_EQ("3,4 90x80", monitor.work_area().ToString());

  monitor.SetScaleAndBounds(1.0f, gfx::Rect(10, 20, 50, 50));
  EXPECT_EQ("0,0 50x50", monitor.bounds().ToString());
  EXPECT_EQ("3,4 40x30", monitor.work_area().ToString());

  monitor.SetSize(gfx::Size(200, 200));
  EXPECT_EQ("3,4 190x180", monitor.work_area().ToString());

  monitor.UpdateWorkAreaFromInsets(gfx::Insets(3, 4, 5, 6));
  EXPECT_EQ("4,3 190x192", monitor.work_area().ToString());
}

TEST(MonitorTest, Scale) {
  gfx::Monitor monitor(0, gfx::Rect(0, 0, 100, 100));
  monitor.set_work_area(gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ("0,0 100x100", monitor.bounds().ToString());
  EXPECT_EQ("10,10 80x80", monitor.work_area().ToString());

  // Scale it back to 2x
  monitor.SetScaleAndBounds(2.0f, gfx::Rect(0, 0, 140, 140));
  EXPECT_EQ("0,0 70x70", monitor.bounds().ToString());
  EXPECT_EQ("10,10 50x50", monitor.work_area().ToString());

  // Scale it back to 1x
  monitor.SetScaleAndBounds(1.0f, gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ("0,0 100x100", monitor.bounds().ToString());
  EXPECT_EQ("10,10 80x80", monitor.work_area().ToString());
}

}
