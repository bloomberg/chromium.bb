// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "url/gurl.h"

namespace {

class ChromeKeyboardUITest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeKeyboardUITest() = default;
  ~ChromeKeyboardUITest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    chrome_keyboard_ui_ = std::make_unique<ChromeKeyboardUI>(profile());
  }

  void TearDown() override {
    chrome_keyboard_ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<ChromeKeyboardUI> chrome_keyboard_ui_;
};

}  // namespace

// Ensure ChromeKeyboardContentsDelegate is successfully constructed and has
// a valid aura::Window when GetKeyboardWindow() is called.
TEST_F(ChromeKeyboardUITest, ChromeKeyboardContentsDelegate) {
  aura::Window* window = chrome_keyboard_ui_->GetKeyboardWindow();
  ASSERT_TRUE(window);
}
