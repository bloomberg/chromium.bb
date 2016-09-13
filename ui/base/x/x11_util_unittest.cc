// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

class X11UtilTest : public testing::Test {
 public:
  X11UtilTest() {}
  ~X11UtilTest() override {}
};

TEST_F(X11UtilTest, ChooseVisualForWindow) {
  XDisplay* display = gfx::GetXDisplay();
  XWindowAttributes attribs;
  Window root = XDefaultRootWindow(display);
  Status status = XGetWindowAttributes(display, root, &attribs);
  DCHECK_NE(0, status);

  int depth = 0;
  bool has_compositing_manager = false;
  ui::ChooseVisualForWindow(has_compositing_manager, nullptr, &depth);
  EXPECT_EQ(attribs.depth, depth);

  // Setting to true has no effect because it has been called with false before.
  has_compositing_manager = true;
  ui::ChooseVisualForWindow(has_compositing_manager, nullptr, &depth);
  EXPECT_EQ(attribs.depth, depth);
}

}  // namespace ui
