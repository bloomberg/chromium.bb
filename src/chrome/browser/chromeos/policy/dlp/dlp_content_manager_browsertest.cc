// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace policy {

namespace {
const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot);
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen);
const DlpContentRestrictionSet kPrintRestricted(DlpContentRestriction::kPrint);
const DlpContentRestrictionSet kVideoCaptureRestricted(
    DlpContentRestriction::kVideoCapture);
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare);

constexpr char kScreenCapturePausedNotificationId[] =
    "screen_capture_dlp_paused";
constexpr char kScreenCaptureResumedNotificationId[] =
    "screen_capture_dlp_resumed";
}  // namespace

class DlpContentManagerBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentManagerBrowserTest() = default;
  ~DlpContentManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCaptureMode);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsRestricted) {
  DlpContentManager* manager = DlpContentManager::Get();
  ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  ScreenshotArea fullscreen = ScreenshotArea::CreateForAllRootWindows();
  ScreenshotArea window =
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView());
  const gfx::Rect web_contents_rect = web_contents->GetContainerBounds();
  gfx::Rect out_rect(web_contents_rect);
  out_rect.Offset(web_contents_rect.width(), web_contents_rect.height());
  gfx::Rect in_rect(web_contents_rect);
  in_rect.Offset(web_contents_rect.width() / 2, web_contents_rect.height() / 2);
  ScreenshotArea partial_out =
      ScreenshotArea::CreateForPartialWindow(root_window, out_rect);
  ScreenshotArea partial_in =
      ScreenshotArea::CreateForPartialWindow(root_window, in_rect);

  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  manager->OnConfidentialityChanged(web_contents, kScreenshotRestricted);
  EXPECT_TRUE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  web_contents->WasHidden();
  manager->OnVisibilityChanged(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  web_contents->WasShown();
  manager->OnVisibilityChanged(web_contents);
  EXPECT_TRUE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_TRUE(manager->IsScreenshotRestricted(window));
  EXPECT_TRUE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));

  manager->OnWebContentsDestroyed(web_contents);
  EXPECT_FALSE(manager->IsScreenshotRestricted(fullscreen));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_in));
  EXPECT_FALSE(manager->IsScreenshotRestricted(partial_out));
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureStoppedWhenConfidentialWindowResized) {
  DlpContentManager* manager = DlpContentManager::Get();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  manager->OnConfidentialityChanged(web_contents1, kVideoCaptureRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), run_loop.QuitClosure());

  // Move first window with confidential content to make it visible.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  capture_mode_delegate->StopObservingRestrictedContent();
  browser2->window()->Close();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureStoppedWhenNonConfidentialWindowResized) {
  DlpContentManager* manager = DlpContentManager::Get();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  manager->OnConfidentialityChanged(web_contents1, kVideoCaptureRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), run_loop.QuitClosure());

  // Move second window to make first window with confidential content visible.
  browser2->window()->SetBounds(gfx::Rect(150, 150, 700, 700));

  // Check that capture was requested to be stopped via callback.
  run_loop.Run();

  capture_mode_delegate->StopObservingRestrictedContent();
  browser2->window()->Close();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       VideoCaptureNotStoppedWhenConfidentialWindowHidden) {
  DlpContentManager* manager = DlpContentManager::Get();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();

  // Open first browser window.
  Browser* browser1 = browser();
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1, GURL("https://example.com"));
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  // Open second browser window.
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2, GURL("https://google.com"));

  // Resize browsers so that second window covers the first one.
  // Browser window can't have width less than 500.
  browser1->window()->SetBounds(gfx::Rect(100, 100, 500, 500));
  browser2->window()->SetBounds(gfx::Rect(0, 0, 700, 700));

  // Make first window content as confidential.
  manager->OnConfidentialityChanged(web_contents1, kVideoCaptureRestricted);

  // Start capture of the whole screen.
  base::RunLoop run_loop;
  auto* capture_mode_delegate = ChromeCaptureModeDelegate::Get();
  capture_mode_delegate->StartObservingRestrictedContent(
      root_window, root_window->bounds(), base::BindOnce([] {
        FAIL() << "Video capture stop callback shouldn't be called";
      }));

  // Move first window, but keep confidential content hidden.
  browser1->window()->SetBounds(gfx::Rect(150, 150, 500, 500));

  // Check that capture was not requested to be stopped via callback.
  run_loop.RunUntilIdle();
  capture_mode_delegate->StopObservingRestrictedContent();

  browser2->window()->Close();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest,
                       ScreenCaptureNotification) {
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  DlpContentManager* manager = DlpContentManager::Get();
  ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  aura::Window* root_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  const auto media_id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, root_window);
  manager->OnScreenCaptureStarted("label", {media_id}, base::DoNothing());

  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));

  manager->OnConfidentialityChanged(web_contents, kScreenShareRestricted);

  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));

  manager->OnConfidentialityChanged(web_contents, kEmptyRestrictionSet);

  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_TRUE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));

  manager->OnScreenCaptureStopped("label", media_id);

  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCapturePausedNotificationId));
  EXPECT_FALSE(display_service_tester.GetNotification(
      kScreenCaptureResumedNotificationId));
}

class DlpContentManagerPolicyBrowserTest : public PolicyTest {
 public:
  DlpContentManagerPolicyBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerPolicyBrowserTest,
                       GetRestrictionSetForURL) {
  const std::string kAllowedUrl = "https://example.com";
  const std::string kUrl1 = "https://example1.com";
  const std::string kUrl2 = "https://example2.com";
  const std::string kUrl3 = "https://example3.com";
  const std::string kUrl4 = "https://example4.com";

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kUrl1);
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenshotRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block", std::move(src_urls1),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kUrl2);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrivacyScreenRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Block", std::move(src_urls2),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  base::Value src_urls3(base::Value::Type::LIST);
  src_urls3.Append(kUrl3);
  base::Value restrictions3(base::Value::Type::LIST);
  restrictions3.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kPrintingRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #3", "Block", std::move(src_urls3),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions3)));

  base::Value src_urls4(base::Value::Type::LIST);
  src_urls4.Append(kUrl4);
  base::Value restrictions4(base::Value::Type::LIST);
  restrictions4.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kScreenShareRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #4", "Block", std::move(src_urls4),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions4)));

  PolicyMap policies;
  policies.Set(key::kDataLeakPreventionRulesList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(rules),
               nullptr);
  UpdateProviderPolicy(policies);

  DlpContentRestrictionSet screenshot_and_videocapture(kScreenshotRestricted);
  screenshot_and_videocapture.SetRestriction(
      DlpContentRestriction::kVideoCapture);
  EXPECT_EQ(screenshot_and_videocapture,
            DlpContentManager::Get()->GetRestrictionSetForURL(GURL(kUrl1)));
  EXPECT_EQ(kPrivacyScreenEnforced,
            DlpContentManager::Get()->GetRestrictionSetForURL(GURL(kUrl2)));
  EXPECT_EQ(kPrintRestricted,
            DlpContentManager::Get()->GetRestrictionSetForURL(GURL(kUrl3)));
  EXPECT_EQ(kScreenShareRestricted,
            DlpContentManager::Get()->GetRestrictionSetForURL(GURL(kUrl4)));
  EXPECT_EQ(
      DlpContentRestrictionSet(),
      DlpContentManager::Get()->GetRestrictionSetForURL(GURL(kAllowedUrl)));
}

}  // namespace policy
