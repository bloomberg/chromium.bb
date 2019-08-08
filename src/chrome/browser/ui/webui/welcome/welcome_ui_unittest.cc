// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/welcome_ui.h"

#include "testing/gtest/include/gtest/gtest.h"

class TestWelcomeUI : public WelcomeUI {
 public:
  using WelcomeUI::IsGzipped;
};

TEST(WelcomeUITest, IsGzipped) {
  // Default resource.
  EXPECT_TRUE(TestWelcomeUI::IsGzipped(""));
  EXPECT_TRUE(TestWelcomeUI::IsGzipped("welcome.html"));
  EXPECT_TRUE(TestWelcomeUI::IsGzipped("new-user"));
  EXPECT_TRUE(TestWelcomeUI::IsGzipped("returning-user"));

  // Images are intentionally not gzipped.
  EXPECT_FALSE(TestWelcomeUI::IsGzipped("images/youtube_1x.png"));

  // This is a dynamic path that fetches from the network and should not be
  // considered gzipped.
  EXPECT_FALSE(TestWelcomeUI::IsGzipped("preview-background.jpg"));
}
