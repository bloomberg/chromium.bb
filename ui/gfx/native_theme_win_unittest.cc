// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_theme_win.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(NativeThemeTest, Init) {
  ASSERT_TRUE(gfx::NativeThemeWin::instance() != NULL);
}
