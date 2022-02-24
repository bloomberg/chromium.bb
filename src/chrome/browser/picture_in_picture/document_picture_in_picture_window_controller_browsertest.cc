// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_picture_in_picture_window_controller.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/overlay/document_overlay_window_views.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/features.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/hit_test.h"
#endif

using content::EvalJs;
using content::ExecJs;
using ::testing::_;

namespace {

class DocumentPictureInPictureWindowControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  DocumentPictureInPictureWindowControllerBrowserTest() = default;

  DocumentPictureInPictureWindowControllerBrowserTest(
      const DocumentPictureInPictureWindowControllerBrowserTest&) = delete;
  DocumentPictureInPictureWindowControllerBrowserTest& operator=(
      const DocumentPictureInPictureWindowControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PictureInPictureV2");
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kPictureInPictureV2);
    InProcessBrowserTest::SetUp();
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(web_contents);
  }

  content::DocumentPictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

  DocumentOverlayWindowViews* GetOverlayWindow() {
    return static_cast<DocumentOverlayWindowViews*>(
        window_controller()->GetWindowForTesting());
  }

  void LoadTabAndEnterPictureInPicture(Browser* browser,
                                       const base::FilePath& file_path) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory), file_path);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    ASSERT_EQ(true, EvalJs(active_web_contents,
                           "window.requestPictureInPictureWindow("
                           "  {width: 200, height: 200}"
                           ").then(w => true)"));
  }

  void ClickButton(views::Button* button) {
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

 private:
  raw_ptr<content::DocumentPictureInPictureWindowController>
      pip_window_controller_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

const base::FilePath::CharType kPictureInPictureWindowSizePage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/window-size.html");

// Checks the creation of the window controller, as well as basic window
// creation, visibility and activation.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CreationAndVisibilityAndActivation) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller() != nullptr);

  ASSERT_TRUE(window_controller()->GetWindowForTesting() == nullptr);
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "window.requestPictureInPictureWindow("
                         "  {width: 200, height: 200}"
                         ").then(w => true)"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  gfx::NativeWindow native_window = GetOverlayWindow()->GetNativeWindow();
  EXPECT_FALSE(platform_util::IsWindowActive(native_window));
}

// Regression test for https://crbug.com/1296780 - opening a picture-in-picture
// window twice in a row should work, closing the old window before opening the
// new one.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CreateTwice) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  gfx::NativeWindow native_window_1 = GetOverlayWindow()->GetNativeWindow();
  EXPECT_FALSE(platform_util::IsWindowActive(native_window_1));

  // Now open the window a second time, without previously closing the original
  // window.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "window.requestPictureInPictureWindow("
                         "  {width: 200, height: 200}"
                         ").then(w => true)"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  gfx::NativeWindow native_window_2 = GetOverlayWindow()->GetNativeWindow();
  EXPECT_FALSE(platform_util::IsWindowActive(native_window_2));

  // The two windows should be different.
  EXPECT_NE(native_window_1, native_window_2);
}

// Tests closing the document picture-in-picture window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CloseWindow) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  ASSERT_TRUE(window_controller());
  window_controller()->Close(/*should_pause_video=*/true);

  ASSERT_FALSE(window_controller()->GetWindowForTesting());
}
