// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTING_SCREENSHOT_TESTING_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTING_SCREENSHOT_TESTING_MIXIN_H_

#include <string>

#include "base/command_line.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/screenshot_testing/screenshot_tester.h"
#include "content/public/test/browser_test_base.h"

namespace chromeos {

// Base mixin class for tests which support testing with screenshots.
// Sets up everything required for taking screenshots.
// Provides functionality to deal with animation load: screenshots
// should be taken only when all the animation is loaded.
class ScreenshotTestingMixin : public InProcessBrowserTestMixin {
 public:
  explicit ScreenshotTestingMixin(InProcessBrowserTestMixinHost* host);
  ~ScreenshotTestingMixin() override;

  void RunScreenshotTesting(const std::string& test_name);
  void IgnoreArea(const SkIRect& area);

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  // It turns out that it takes some more time for the animation
  // to finish loading even after all the notifications have been sent.
  // That happens due to some properties of compositor.
  // This method should be used after getting all the necessary notifications
  // to wait for the actual load of animation.
  void SynchronizeAnimationLoadWithCompositor();

  // This method exists only because of the current implementation of
  // SynchronizeAnimationLoadWithCompositor.
  void HandleAnimationLoad();

  // Required for current implementation of
  // SynchronizeAnimationLoadWithCompositor()
  base::OneShotTimer timer_;
  base::Closure animation_waiter_quitter_;

  // Is true if testing with screenshots is turned on with all proper switches.
  bool enable_test_screenshots_ = false;

  // |screenshot_tester_ | does everything connected with taking, loading and
  // comparing screenshots
  ScreenshotTester screenshot_tester_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTestingMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTING_SCREENSHOT_TESTING_MIXIN_H_
