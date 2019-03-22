// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Returns a string with the x-coordinate and width of each gfx::Rect in
// |tabs_bounds|. Each gfx::Rect is separated by a ','.
std::string TabsBoundsToString(const std::vector<gfx::Rect>& tabs_bounds) {
  std::string result;
  for (const auto& bounds : tabs_bounds) {
    if (!result.empty())
      result += ", ";
    result +=
        base::IntToString(bounds.x()) + " " + base::IntToString(bounds.width());
  }
  return result;
}

}  // namespace

TEST(TabStripLayoutTest, Tests) {
  TabSizeInfo tab_size_info;
  tab_size_info.pinned_tab_width = 10;
  tab_size_info.min_active_width = 20;
  tab_size_info.min_inactive_width = 14;
  tab_size_info.standard_size = gfx::Size(100, 10);
  tab_size_info.tab_overlap = 4;

  struct TestCases {
    int num_pinned_tabs;
    int num_tabs;
    int active_index;
    int start_x;
    int width;
    const char* expected_sizes;
    int expected_active_width;
    int expected_inactive_width;
  } test_cases[] = {
      // Ample space, all normal tabs.
      {0, 3, 0, 0, 1000, "0 100, 96 100, 192 100", 100, 100},

      // Ample space, all normal tabs, starting at a nonzero value.
      {0, 3, 0, 100, 1000, "100 100, 196 100, 292 100", 100, 100},

      // Ample space, all pinned.
      {3, 3, 0, 0, 1000, "0 10, 6 10, 12 10", 100, 100},

      // Ample space, one pinned and two normal tabs.
      {1, 3, 0, 0, 1000, "0 10, 6 100, 102 100", 100, 100},

      // Resize between min and max, no pinned and no rounding.
      {0, 4, 0, 0, 100, "0 28, 24 28, 48 28, 72 28", 28, 28},

      // Resize between min and max, pinned and no rounding.
      {1, 3, 0, 0, 100, "0 10, 6 49, 51 49", 49, 49},

      // Resize between min and max, no pinned, rounding.
      {0, 4, 0, 0, 102, "0 29, 25 29, 50 28, 74 28", 28, 28},

      // Resize between min and max, pinned and rounding.
      {1, 3, 0, 0, 101, "0 10, 6 50, 52 49", 49, 49},

      // Resize between active/inactive width, only one tab.
      {0, 1, 0, 0, 15, "0 20", 20, 15},

      // Resize between active/inactive width and no rounding.
      {0, 6, 0, 0, 90, "0 20, 16 18, 30 18, 44 18, 58 18, 72 18", 20, 18},

      // Resize between active/inactive width, rounding.
      {0, 6, 0, 0, 93, "0 20, 16 19, 31 19, 46 19, 61 18, 75 18", 20, 18},

      // Resize between active/inactive width, one pinned and active, no
      // rounding.
      {1, 6, 0, 0, 85, "0 10, 6 19, 21 19, 36 19, 51 19, 66 19", 20, 19},

      // Resize between active/inactive width, one pinned and active, rounding.
      {1, 6, 0, 0, 86, "0 10, 6 20, 22 19, 37 19, 52 19, 67 19", 20, 19},

      // Not enough space, all pinned.
      {3, 3, 0, 0, 10, "0 10, 6 10, 12 10", 100, 100},

      // Not enough space (for minimum widths), all normal.
      {0, 3, 0, 0, 10, "0 20, 16 14, 26 14", 20, 14},

      // Not enough space (for minimum widths), one pinned and two normal.
      {1, 3, 0, 0, 10, "0 10, 6 14, 16 14", 20, 14},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    int active_width;
    int inactive_width;
    std::vector<gfx::Rect> tabs_bounds = CalculateBounds(
        tab_size_info, test_cases[i].num_pinned_tabs, test_cases[i].num_tabs,
        test_cases[i].active_index, test_cases[i].start_x, test_cases[i].width,
        &active_width, &inactive_width);
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected_sizes, TabsBoundsToString(tabs_bounds));
    EXPECT_EQ(test_cases[i].expected_active_width, active_width);
    EXPECT_EQ(test_cases[i].expected_inactive_width, inactive_width);
    for (const auto& bounds : tabs_bounds) {
      EXPECT_EQ(0, bounds.y());
      EXPECT_EQ(tab_size_info.standard_size.height(), bounds.height());
    }
  }
}
