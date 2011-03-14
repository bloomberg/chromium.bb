// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "views/controls/progress_bar.h"

namespace views {

TEST(ProgressBarTest, ProgressProperty) {
  ProgressBar bar;
  bar.SetProgress(-1);
  int progress = bar.GetProgress();
  EXPECT_EQ(0, progress);
  bar.SetProgress(300);
  progress = bar.GetProgress();
  EXPECT_EQ(100, progress);
  bar.SetProgress(62);
  progress = bar.GetProgress();
  EXPECT_EQ(62, progress);
}

TEST(ProgressBarTest, AddProgressMethod) {
  ProgressBar bar;
  bar.SetProgress(10);
  bar.AddProgress(22);
  int progress = bar.GetProgress();
  EXPECT_EQ(32, progress);
  bar.AddProgress(200);
  progress = bar.GetProgress();
  EXPECT_EQ(100, progress);
}

TEST(ProgressBarTest, TooltipTextProperty) {
  ProgressBar bar;
  std::wstring tooltip = L"Some text";
  EXPECT_FALSE(bar.GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(L"", tooltip);
  std::wstring tooltip_text = L"My progress";
  bar.SetTooltipText(tooltip_text);
  EXPECT_TRUE(bar.GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
}

TEST(ProgressBarTest, Accessibility) {
  ProgressBar bar;
  bar.SetProgress(62);

  ui::AccessibleViewState state;
  bar.GetAccessibleState(&state);
  EXPECT_EQ(ui::AccessibilityTypes::ROLE_PROGRESSBAR, state.role);
  EXPECT_EQ(string16(), state.name);
  EXPECT_TRUE(ui::AccessibilityTypes::STATE_READONLY & state.state);
}

}  // namespace views
