// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_item_view.h"

namespace {

struct ModelViewCalculateLayoutCase {
  gfx::Size screen_size;
  int num_of_tiles;
  gfx::Size expected_icon_size;
  int expected_rows;
  int expected_cols;
};

}  // namespace

namespace app_list {

TEST(AppListModelViewTest, ModelViewCalculateLayout) {
  // kMinTitleWidth is the average width of 15 chars of default size 12 font in
  // chromeos. Override here to avoid flakiness from different default font on
  // bots.
  const int kMinTitleWidth = 90;
  AppListItemView::SetMinTitleWidth(kMinTitleWidth);

  const int kLauncherHeight = 50;
  const ModelViewCalculateLayoutCase kCases[] = {
    { gfx::Size(1024, 768), 4, gfx::Size(128, 128), 2, 3 },
    { gfx::Size(1024, 768), 29, gfx::Size(64, 64), 5, 6 },
    { gfx::Size(1024, 768), 40, gfx::Size(48, 48), 5, 8 },
    { gfx::Size(1024, 768), 48, gfx::Size(48, 48), 6, 8 },

    { gfx::Size(1280, 1024), 4, gfx::Size(128, 128), 2, 3 },
    { gfx::Size(1280, 1024), 29, gfx::Size(64, 64), 5, 7 },
    { gfx::Size(1280, 1024), 50, gfx::Size(64, 64), 7, 8 },
    { gfx::Size(1280, 1024), 70, gfx::Size(48, 48), 7, 10 },
    { gfx::Size(1280, 1024), 99, gfx::Size(48, 48), 9, 11 },

    { gfx::Size(1600, 900), 4, gfx::Size(128, 128), 2, 3 },
    { gfx::Size(1600, 900), 29, gfx::Size(64, 64), 4, 8 },

    // Not able to fit into screen case.
    { gfx::Size(1024, 768), 50, gfx::Size(32, 32), 6, 9 },
    { gfx::Size(1280, 1024), 100, gfx::Size(32, 32), 10, 11 },
  };

  for (size_t i = 0; i < arraysize(kCases); ++i) {
    gfx::Size icon_size;
    int rows = 0;
    int cols = 0;
    gfx::Size content_size(kCases[i].screen_size.width(),
                           kCases[i].screen_size.height() - kLauncherHeight);
    AppListModelView::CalculateLayout(content_size,
                                      kCases[i].num_of_tiles,
                                      &icon_size,
                                      &rows,
                                      &cols);
    EXPECT_EQ(kCases[i].expected_icon_size, icon_size) << "i=" << i;
    EXPECT_EQ(kCases[i].expected_rows, rows) << "i=" << i;
    EXPECT_EQ(kCases[i].expected_cols, cols) << "i=" << i;
  }
}

}  // namespace app_list
