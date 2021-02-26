// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_ui.h"

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/views/widget/widget.h"
#endif

namespace {

// Extracts the |name| argument for ShowUi() from the current test case name.
// E.g. for InvokeUi_name (or DISABLED_InvokeUi_name) returns "name".
std::string NameFromTestCase() {
  const std::string name = base::TestNameWithoutDisabledPrefix(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  size_t underscore = name.find('_');
  return underscore == std::string::npos ? std::string()
                                         : name.substr(underscore + 1);
}

}  // namespace

TestBrowserUi::TestBrowserUi() {
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  // Default to fuzzy diff. The magic number is chosen based on
  // past experiments.
  SetPixelMatchAlgorithm(
      std::make_unique<ui::test::FuzzySkiaGoldMatchingAlgorithm>(20, 255 * 3));
#endif
}

TestBrowserUi::~TestBrowserUi() = default;

// TODO(https://crbug.com/958242) support Mac for pixel tests.
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
bool TestBrowserUi::VerifyPixelUi(views::Widget* widget,
                                  const std::string& screenshot_prefix,
                                  const std::string& screenshot_name) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "browser-ui-tests-verify-pixels"))
    return true;

  // Wait for painting complete.
  auto* compositor = widget->GetCompositor();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);

  BrowserSkiaGoldPixelDiff pixel_diff;
  pixel_diff.Init(widget, screenshot_prefix);
  return pixel_diff.CompareScreenshot(
      screenshot_name, widget->GetContentsView(), GetPixelMatchAlgorithm());
}

void TestBrowserUi::SetPixelMatchAlgorithm(
    std::unique_ptr<ui::test::SkiaGoldMatchingAlgorithm> algorithm) {
  algorithm_ = std::move(algorithm);
}
#endif

void TestBrowserUi::ShowAndVerifyUi() {
  PreShow();
  ShowUi(NameFromTestCase());
  ASSERT_TRUE(VerifyUi());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherInteractive))
    WaitForUserDismissal();
  else
    DismissUi();
}
