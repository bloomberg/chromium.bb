// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/screen.h"

#if defined(USE_AURA)
#include "base/message_loop.h"
#include "ui/aura/desktop.h"
#endif

namespace {

#if defined(USE_AURA)
class ScreenTest : public testing::Test {
 public:
  ScreenTest() {
    aura::Desktop::GetInstance()->ShowDesktop();
  }

  virtual ~ScreenTest() {
    aura::Desktop::GetInstance()->DeleteInstanceForTesting();
  }

 private:
  MessageLoopForUI message_loop_;
};
#else
typedef testing::Test ScreenTest;
#endif

TEST_F(ScreenTest, GetPrimaryMonitorSize) {
  // We aren't actually testing that it's correct, just that it's sane.
  const gfx::Size size = gfx::Screen::GetPrimaryMonitorSize();
  EXPECT_GE(size.width(), 1);
  EXPECT_GE(size.height(), 1);
}

TEST_F(ScreenTest, GetNumMonitors) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(gfx::Screen::GetNumMonitors(), 1);
}

}  // namespace
