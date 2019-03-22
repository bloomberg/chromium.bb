// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Tests app usage recorded by DemoSessionMetricsRecorder.
// Mocks out the timer to control the sampling cycle. Tests will create and
// activate different window types to test that samples are attributed to the
// correct apps. Tests will also fire the timer continuously without user
// activity to simulate idle time and verify that idle samples are dropped.
class DemoSessionMetricsRecorderTest : public AshTestBase {
 public:
  DemoSessionMetricsRecorderTest() = default;
  ~DemoSessionMetricsRecorderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create mock timer to be passed into DemoSessionMetricsRecorder.
    std::unique_ptr<base::RepeatingTimer> mock_timer =
        std::make_unique<base::MockRepeatingTimer>();
    // Store a pointer to the timer before moving it.
    mock_timer_ = static_cast<base::MockRepeatingTimer*>(mock_timer.get());
    metrics_recorder_ =
        std::make_unique<DemoSessionMetricsRecorder>(std::move(mock_timer));

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    metrics_recorder_.reset();
    AshTestBase::TearDown();
  }

  // Fires the timer, if it's running. (If it's stopped, we can assume any
  // amount of time passes here.)
  void FireTimer() {
    if (mock_timer_->IsRunning())
      mock_timer_->Fire();
  }

  // Simulates user activity.
  void SendUserActivity() { metrics_recorder_->OnUserActivity(nullptr); }

  void ClearHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Clears the metrics recorder and the timer to simulate the session ending.
  void DeleteMetricsRecorder() {
    mock_timer_ = nullptr;
    metrics_recorder_.reset();
  }

  // Creates a browser window.
  std::unique_ptr<aura::Window> CreateBrowserWindow() {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::BROWSER));
    return window;
  }

  // Creates a browser window associated with a hosted app.
  std::unique_ptr<aura::Window> CreateHostedAppBrowserWindow(
      const std::string& app_id) {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::BROWSER));
    window->SetProperty(
        kShelfIDKey,
        new std::string(ShelfID(app_id, std::string()).Serialize()));
    return window;
  }

  // Creates a normal Chrome platform app window.
  std::unique_ptr<aura::Window> CreateChromeAppWindow(
      const std::string& app_id) {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::CHROME_APP));
    window->SetProperty(
        kShelfIDKey,
        new std::string(ShelfID(app_id, std::string()).Serialize()));
    return window;
  }

  // Creates a normal ARC++ app window.
  std::unique_ptr<aura::Window> CreateArcWindow(
      const std::string& package_name) {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::ARC_APP));

    // ARC++ shelf app IDs are hashes of package_name#activity_name formatted as
    // extension IDs. The point is that they are opaque to the metrics recorder.
    const std::string app_id = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    window->SetProperty(
        kShelfIDKey,
        new std::string(ShelfID(app_id, std::string()).Serialize()));
    window->SetProperty(kArcPackageNameKey, new std::string(package_name));
    return window;
  }

  // Creates a popup type window.
  std::unique_ptr<aura::Window> CreatePopupWindow() {
    std::unique_ptr<aura::Window> window(
        CreateTestWindowInShellWithDelegateAndType(
            aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
            aura::client::WINDOW_TYPE_POPUP, 0, gfx::Rect(0, 0, 10, 10)));
    return window;
  }

 protected:
  // Captures histograms.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // The test target.
  std::unique_ptr<DemoSessionMetricsRecorder> metrics_recorder_;

  // Owned by metics_recorder_.
  base::MockRepeatingTimer* mock_timer_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoSessionMetricsRecorderTest);
};

// Verify samples are correct when one app window is active.
TEST_F(DemoSessionMetricsRecorderTest, ActiveApp) {
  auto chrome_app_window =
      CreateChromeAppWindow(extension_misc::kHighlightsAppId);

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 5; i++)
    FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kHighlights, 5);
}

// Verify samples are correct when multiple browser windows become active.
TEST_F(DemoSessionMetricsRecorderTest, BrowserWindows) {
  auto browser_window = CreateBrowserWindow();
  auto browser_window2 = CreateBrowserWindow();

  // Browser windows should all be treated as the same type.
  wm::ActivateWindow(browser_window.get());
  FireTimer();
  wm::ActivateWindow(browser_window2.get());
  FireTimer();
  FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kBrowser,
      3);
}

// Verify samples are correct when multiple windows types become active.
TEST_F(DemoSessionMetricsRecorderTest, AppTypes) {
  auto browser_window = CreateBrowserWindow();
  auto chrome_app_window = CreateChromeAppWindow(extension_misc::kCameraAppId);
  auto hosted_app_browser_window =
      CreateHostedAppBrowserWindow(extension_misc::kYoutubeAppId);
  auto arc_window = CreateArcWindow("com.google.Photos");

  wm::ActivateWindow(browser_window.get());
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kBrowser,
      1);

  wm::ActivateWindow(chrome_app_window.get());
  FireTimer();
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kCamera,
      2);

  wm::ActivateWindow(hosted_app_browser_window.get());
  FireTimer();
  FireTimer();
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kYouTube,
      3);

  wm::ActivateWindow(arc_window.get());
  FireTimer();
  FireTimer();
  FireTimer();
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 4);

  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 10);
}

// Verify popup windows are categorized as kOtherWindow.
TEST_F(DemoSessionMetricsRecorderTest, PopupWindows) {
  auto chrome_app_window = CreateChromeAppWindow(extension_misc::kCameraAppId);
  auto popup_window = CreatePopupWindow();

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 5; i++)
    FireTimer();

  wm::ActivateWindow(popup_window.get());
  for (int i = 0; i < 3; i++)
    FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kCamera,
      5);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherWindow, 3);
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 8);
}

// Verify unknown apps are categorized as "other" Chrome/ARC apps.
TEST_F(DemoSessionMetricsRecorderTest, OtherApps) {
  auto chrome_app_window =
      CreateChromeAppWindow("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  auto arc_window = CreateArcWindow("com.foo.bar");

  wm::ActivateWindow(chrome_app_window.get());
  FireTimer();

  wm::ActivateWindow(arc_window.get());
  FireTimer();
  FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherChromeApp, 1);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherArcApp, 2);
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 3);
}

// Verify samples are discarded after no user activity.
TEST_F(DemoSessionMetricsRecorderTest, DiscardAfterInactivity) {
  auto chrome_app_window = CreateChromeAppWindow(extension_misc::kCameraAppId);
  auto arc_window = CreateChromeAppWindow("com.google.Photos");

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 5; i++)
    FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kCamera,
      5);
  ClearHistograms();

  // Have no user activity for 20 seconds.
  for (int i = 0; i < 20; i++)
    FireTimer();

  // After user activity, the active window from the idle time isn't reported.
  SendUserActivity();
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

// Verify sample collection resumes after user activity.
TEST_F(DemoSessionMetricsRecorderTest, ResumeAfterActivity) {
  auto chrome_app_window = CreateChromeAppWindow(extension_misc::kCameraAppId);

  wm::ActivateWindow(chrome_app_window.get());

  // Have no user activity for 20 seconds.
  for (int i = 0; i < 20; i++)
    FireTimer();

  // Now send user activity.
  SendUserActivity();
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);

  // Sample collection should resume.
  for (int i = 0; i < 5; i++)
    FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kCamera,
      5);
}

// Verify window activation during idle time doesn't trigger reporting.
TEST_F(DemoSessionMetricsRecorderTest, ActivateWindowWhenIdle) {
  auto chrome_app_window = CreateChromeAppWindow(extension_misc::kCameraAppId);
  auto chrome_app_window2 =
      CreateChromeAppWindow(extension_misc::kGoogleKeepAppId);

  wm::ActivateWindow(chrome_app_window.get());

  // Even if the active window changes, which can happen automatically, these
  // samples shouldn't be reported.
  for (int i = 0; i < 10; i++)
    FireTimer();
  wm::ActivateWindow(chrome_app_window2.get());
  for (int i = 0; i < 10; i++)
    FireTimer();

  SendUserActivity();
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

TEST_F(DemoSessionMetricsRecorderTest, RepeatedUserActivity) {
  auto chrome_app_window = CreateChromeAppWindow(extension_misc::kCameraAppId);
  auto arc_window = CreateArcWindow("com.google.Photos");

  wm::ActivateWindow(chrome_app_window.get());

  FireTimer();
  SendUserActivity();
  SendUserActivity();

  // Switching between windows in between samples isn't recorded, even with user
  // action.
  FireTimer();
  SendUserActivity();
  wm::ActivateWindow(arc_window.get());
  SendUserActivity();
  wm::ActivateWindow(chrome_app_window.get());
  SendUserActivity();

  FireTimer();
  wm::ActivateWindow(arc_window.get());
  SendUserActivity();
  SendUserActivity();
  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kCamera,
      3);
}

// Verify remaining samples are recorded on exit.
TEST_F(DemoSessionMetricsRecorderTest, RecordOnExit) {
  auto chrome_app_window =
      CreateChromeAppWindow(extension_misc::kGoogleKeepAppId);
  auto arc_window = CreateArcWindow("com.google.Photos");

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 2; i++)
    FireTimer();
  wm::ActivateWindow(arc_window.get());
  for (int i = 0; i < 4; i++)
    FireTimer();

  DeleteMetricsRecorder();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGoogleKeep, 2);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 4);
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 6);
}

// Verify remaining samples are not recorded on exit because the user became
// idle.
TEST_F(DemoSessionMetricsRecorderTest, IgnoreOnIdleExit) {
  auto chrome_app_window =
      CreateChromeAppWindow(extension_misc::kFilesManagerAppId);

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 10; i++)
    FireTimer();
  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kFiles,
      10);
  ClearHistograms();

  for (int i = 0; i < 20; i++)
    FireTimer();

  DeleteMetricsRecorder();

  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

// Verify remaining samples are not recorded on exit when the user was idle the
// whole time.
TEST_F(DemoSessionMetricsRecorderTest, IgnoreOnIdleSession) {
  auto chrome_app_window =
      CreateChromeAppWindow(extension_misc::kHighlightsAppId);

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 20; i++)
    FireTimer();

  DeleteMetricsRecorder();

  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

}  // namespace
}  // namespace ash
