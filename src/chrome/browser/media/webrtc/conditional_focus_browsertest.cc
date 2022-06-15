// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/switches.h"

// TODO(crbug.com/1215089): Enable this test suite on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using content::WebContents;

// Capturing page.
const char kCapturingPage[] = "/webrtc/conditional_focus_capturing_page.html";
// Captured page.
const char kCapturedPage[] = "/webrtc/conditional_focus_captured_page.html";

// The tab to be captured is detected using this string, which is hard-coded
// in the HTML file.
const char kCapturedPageTitle[] = "Conditional Focus Test - Captured Page";

enum class FocusEnumValue {
  kNoValue,               // ""
  kFocusCapturedSurface,  // "focus-captured-surface"
  kNoFocusChange          // "no-focus-change"
};

const char* ToString(FocusEnumValue focus_enum_value) {
  switch (focus_enum_value) {
    case FocusEnumValue::kNoValue:
      return "";
    case FocusEnumValue::kFocusCapturedSurface:
      return "focus-captured-surface";
    case FocusEnumValue::kNoFocusChange:
      return "no-focus-change";
  }
  NOTREACHED();
  return "";
}

enum class Tab { kUnknownTab, kCapturingTab, kCapturedTab };

}  // namespace

// Essentially depends on InProcessBrowserTest, but WebRtcTestBase provides
// detection of JS errors.
class ConditionalFocusBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    WebRtcTestBase::SetUpInProcessBrowserTestFixture();
    DetectErrorsInJavaScript();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kCapturedPageTitle);
    command_line->AppendSwitchASCII(blink::switches::kConditionalFocusWindowMs,
                                    "5000");
  }

  WebContents* OpenTestPageInNewTab(const std::string& test_url) {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
    GURL url = embedded_test_server()->GetURL(test_url);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUpTestTabs() {
    captured_tab_ = OpenTestPageInNewTab(kCapturedPage);
    capturing_tab_ = OpenTestPageInNewTab(kCapturingPage);

    TabStripModel* const tab_strip_model = browser()->tab_strip_model();

    EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), captured_tab_);
    EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), capturing_tab_);
    EXPECT_EQ(tab_strip_model->GetActiveWebContents(), capturing_tab_);
  }

  Tab ActiveTab() const {
    WebContents* const active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return active_web_contents == capturing_tab_
               ? Tab::kCapturingTab
               : active_web_contents == captured_tab_ ? Tab::kCapturedTab
                                                      : Tab::kUnknownTab;
  }

  // Performs the following actions, in order:
  // 1. Calls navigator.mediaDevices.getDisplayMedia(). The test fixture
  //    is set to simulate the user selecting |captured_tab_|. Expects success.
  // 2. getDisplayMedia() returns a Promise<MediaStream> which resolves in
  //    a microtask. We hold up the main thread for |busy_wait_ms| so as to
  //    simulate either (a) an application which performs some non-trivial
  //    computation on that task, (b) intentional delay by the app or
  //    (c) random CPU delays.
  // 3. Either avoids calling focus() or does so with the appropriate
  //    value, depending on |focus_enum_value|.
  // If !on_correct_microtask, calling focus() is done from a task that is
  // scheduled to be executed later.
  void Capture(int busy_wait_ms,
               FocusEnumValue focus_enum_value,
               bool on_correct_microtask = true,
               const std::string& expected_result = "capture-success") {
    std::string script_result;
    // TODO(crbug.com/1243764): Use EvalJs() instead.
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        capturing_tab_->GetPrimaryMainFrame(),
        base::StringPrintf("captureOtherTab(%d, \"%s\", %s);", busy_wait_ms,
                           ToString(focus_enum_value),
                           on_correct_microtask ? "true" : "false"),
        &script_result));
    EXPECT_EQ(script_result, expected_result);
  }

  void Wait(base::TimeDelta timeout) {
    base::RunLoop run_loop;
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, timeout, run_loop.QuitClosure());
    run_loop.Run();
    timer.Stop();
  }

  // Repeatedly polls until a focus-switch to the captured tab is detected.
  // If a switch is detected within 10s, returns true; otherwise, returns false.
  bool WaitForFocusSwitchToCapturedTab() {
    for (int i = 0; i < 20; ++i) {
      if (ActiveTab() == Tab::kCapturedTab) {
        return true;
      }
      Wait(base::Milliseconds(500));
    }
    return (ActiveTab() == Tab::kCapturedTab);
  }

  void CallFocusAndExpectError(const std::string& expected_error) {
    std::string script_result;
    // TODO(crbug.com/1243764): Use EvalJs() instead.
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        capturing_tab_->GetPrimaryMainFrame(), "callFocusAndExpectError();",
        &script_result));
    EXPECT_EQ(script_result, expected_error);
  }

 protected:
  raw_ptr<WebContents> captured_tab_ = nullptr;
  raw_ptr<WebContents> capturing_tab_ = nullptr;
};

// Flaky on Win bots and on linux release bots http://crbug.com/1264744
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) && defined(NDEBUG))
#define MAYBE_CapturedTabFocusedIfNoExplicitCallToFocus \
  DISABLED_CapturedTabFocusedIfNoExplicitCallToFocus
#else
#define MAYBE_CapturedTabFocusedIfNoExplicitCallToFocus \
  CapturedTabFocusedIfNoExplicitCallToFocus
#endif
IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       MAYBE_CapturedTabFocusedIfNoExplicitCallToFocus) {
  SetUpTestTabs();
  Capture(0, FocusEnumValue::kNoValue);
  EXPECT_TRUE(WaitForFocusSwitchToCapturedTab());
}

// Flaky on Win bots and on linux release bots http://crbug.com/1264744
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) && defined(NDEBUG))
#define MAYBE_CapturedTabFocusedIfExplicitlyCallingFocus \
  DISABLED_CapturedTabFocusedIfExplicitlyCallingFocus
#else
#define MAYBE_CapturedTabFocusedIfExplicitlyCallingFocus \
  CapturedTabFocusedIfExplicitlyCallingFocus
#endif
IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       MAYBE_CapturedTabFocusedIfExplicitlyCallingFocus) {
  SetUpTestTabs();
  Capture(0, FocusEnumValue::kFocusCapturedSurface);
  EXPECT_TRUE(WaitForFocusSwitchToCapturedTab());
}

IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       CapturedTabNotFocusedIfExplicitlyCallingNoFocus) {
  SetUpTestTabs();
  Capture(0, FocusEnumValue::kNoFocusChange);
  // Whereas calls to Wait() in previous tests served to minimize flakiness,
  // this one is to prove no false-positives. Namely, we allow enough time
  // for the focus-change, yet it does not occur.
  Wait(base::Milliseconds(10000));
  EXPECT_EQ(ActiveTab(), Tab::kCapturingTab);
}

#if BUILDFLAG(IS_WIN)
// Flaky on Win7 CI builder. See https://crbug.com/1255957.
#define MAYBE_CapturedTabFocusedIfAppWaitsTooLongBeforeCallingFocus \
  DISABLED_CapturedTabFocusedIfAppWaitsTooLongBeforeCallingFocus
#else
#define MAYBE_CapturedTabFocusedIfAppWaitsTooLongBeforeCallingFocus \
  CapturedTabFocusedIfAppWaitsTooLongBeforeCallingFocus
#endif

IN_PROC_BROWSER_TEST_F(
    ConditionalFocusBrowserTest,
    MAYBE_CapturedTabFocusedIfAppWaitsTooLongBeforeCallingFocus) {
  SetUpTestTabs();
  Capture(15000, FocusEnumValue::kNoFocusChange);
  EXPECT_TRUE(WaitForFocusSwitchToCapturedTab());
}

// This ensures that we don't have to wait |kConditionalFocusWindowMs| before
// focus occurs. Rather, that is just the hard-limit that is employed in case
// the application attempts abuse by blocking the main thread for too long.
IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest, FocusTriggeredByMicrotask) {
  SetUpTestTabs();
  Capture(0, FocusEnumValue::kNoValue);
  // Note that the Wait(), which is necessary in order to minimize flakiness,
  // has a duration less than |kConditionalFocusWindowMs|.
  Wait(base::Milliseconds(4500));
  // Focus-change already occurred before kConditionalFocusWindowMs.
  EXPECT_EQ(ActiveTab(), Tab::kCapturedTab);
}

IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       UserFocusChangeSuppressesFocusDecision) {
  SetUpTestTabs();

  // Recall that tab 0 is neither the capturing nor captured tab,
  // and it is initially inactive.
  ASSERT_NE(browser()->tab_strip_model()->GetWebContentsAt(0), captured_tab_);
  ASSERT_NE(browser()->tab_strip_model()->GetWebContentsAt(0), capturing_tab_);

  // Start capturing, but give some time for a manual focus-switch by the user.
  Capture(2500, FocusEnumValue::kFocusCapturedSurface);

  // Simulated manual user change of active tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // No additional focus-change - user activity has suppressed that.
  Wait(base::Milliseconds(7500));
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            browser()->tab_strip_model()->GetWebContentsAt(0));
}

// TODO(crbug.com/1285418): Flaky on Win and Linux bots.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_ExceptionRaisedIfFocusCalledMultipleTimes \
  DISABLED_ExceptionRaisedIfFocusCalledMultipleTimes
#else
#define MAYBE_ExceptionRaisedIfFocusCalledMultipleTimes \
  ExceptionRaisedIfFocusCalledMultipleTimes
#endif
IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       MAYBE_ExceptionRaisedIfFocusCalledMultipleTimes) {
  // Setup.
  SetUpTestTabs();
  Capture(0, FocusEnumValue::kFocusCapturedSurface);
  ASSERT_TRUE(WaitForFocusSwitchToCapturedTab());  // Verified by earlier test.

  // Test.
  CallFocusAndExpectError(
      "InvalidStateError: Failed to execute 'focus' on "
      "'BrowserCaptureMediaStreamTrack': Method may only be called once.");
}

IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       ExceptionRaisedIfFocusCalledOnClone) {
  SetUpTestTabs();

  // TODO(crbug.com/1243764): Use EvalJs() instead.
  std::string script_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      capturing_tab_->GetPrimaryMainFrame(), "captureCloneAndFocusClone();",
      &script_result));
  EXPECT_EQ(
      script_result,
      "InvalidStateError: Failed to execute 'focus' on "
      "'BrowserCaptureMediaStreamTrack': Method may not be invoked on clones.");
}

IN_PROC_BROWSER_TEST_F(ConditionalFocusBrowserTest,
                       ExceptionRaisedIfFocusCalledAfterMicrotaskExecutes) {
  // Setup.
  SetUpTestTabs();
  Capture(0, FocusEnumValue::kFocusCapturedSurface,
          /*on_correct_microtask=*/false,
          /*expected_result=*/
          "InvalidStateError: Failed to execute 'focus' on "
          "'BrowserCaptureMediaStreamTrack': The microtask on which the "
          "Promise was settled has terminated.");
}

#endif  //  !BUILDFLAG(IS_CHROMEOS_LACROS)
