// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_view_state.h"

namespace views {

TEST(ProgressBarTest, Accessibility) {
  ProgressBar bar;
  bar.SetValue(0.62);

  ui::AXViewState state;
  bar.GetAccessibleState(&state);
  EXPECT_EQ(ui::AX_ROLE_PROGRESS_INDICATOR, state.role);
  EXPECT_EQ(base::string16(), state.name);
  EXPECT_TRUE(state.HasStateFlag(ui::AX_STATE_READ_ONLY));
}

}  // namespace views
