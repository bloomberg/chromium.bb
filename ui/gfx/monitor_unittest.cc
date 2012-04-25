// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/monitor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/insets.h"

namespace {

TEST(MonitorTest, WorkArea) {
  gfx::Monitor monitor(0, gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ("0,0 100x100", monitor.work_area().ToString());

  monitor.set_work_area(gfx::Rect(3, 4, 90, 80));
  EXPECT_EQ("3,4 90x80", monitor.work_area().ToString());

  monitor.SetBoundsAndUpdateWorkArea(gfx::Rect(10, 20, 50, 50));
  EXPECT_EQ("13,24 40x30", monitor.work_area().ToString());

  monitor.SetSizeAndUpdateWorkArea(gfx::Size(200, 200));
  EXPECT_EQ("13,24 190x180", monitor.work_area().ToString());

  monitor.UpdateWorkAreaWithInsets(gfx::Insets(3, 4, 5, 6));
  EXPECT_EQ("14,23 190x192", monitor.work_area().ToString());
}

}
