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
#include "content/public/browser/render_widget_host_view.h"
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
#include "services/network/public/cpp/is_potentially_trustworthy.h"
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

const base::FilePath::CharType kPictureInPictureDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

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
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
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

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    if (!window_controller())
      return nullptr;

    if (auto* web_contents = window_controller()->GetChildWebContents())
      return web_contents->GetRenderWidgetHostView();

    return nullptr;
  }

  void LoadTabAndEnterPictureInPicture(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kPictureInPictureDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    ASSERT_EQ(true, EvalJs(active_web_contents, "createDocumentPipWindow()"));
    ASSERT_TRUE(window_controller() != nullptr);
    ASSERT_TRUE(GetRenderWidgetHostView()->IsShowing());
  }

  void ClickButton(views::Button* button) {
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  // Watch for destruction of a WebContents. `is_destroyed()` will report if the
  // WebContents has been destroyed yet.
  class DestructionObserver : public content::WebContentsObserver {
   public:
    explicit DestructionObserver(content::WebContents* web_contents)
        : content::WebContentsObserver(web_contents) {}

    void WebContentsDestroyed() override { Observe(/*web_contents=*/nullptr); }

    // If we've stopped observing, it's because the WebContents was destroyed.
    bool is_destroyed() const { return !web_contents(); }
  };

 private:
  raw_ptr<content::DocumentPictureInPictureWindowController>
      pip_window_controller_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Checks the creation of the window controller, as well as basic window
// creation, visibility and activation.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Document PiP is not supported in Lacros yet.
#define MAYBE_CreationAndVisibilityAndActivation \
  DISABLED_CreationAndVisibilityAndActivation
#else
#define MAYBE_CreationAndVisibilityAndActivation \
  CreationAndVisibilityAndActivation
#endif
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_CreationAndVisibilityAndActivation) {
  LoadTabAndEnterPictureInPicture(browser());

  ASSERT_TRUE(GetRenderWidgetHostView());
  EXPECT_TRUE(GetRenderWidgetHostView()->IsShowing());
  EXPECT_FALSE(GetRenderWidgetHostView()->HasFocus());
}

// Regression test for https://crbug.com/1296780 - opening a picture-in-picture
// window twice in a row should work, closing the old window before opening the
// new one.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Document PiP is not supported in Lacros yet.
#define MAYBE_CreateTwice DISABLED_CreateTwice
#else
#define MAYBE_CreateTwice CreateTwice
#endif
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_CreateTwice) {
  LoadTabAndEnterPictureInPicture(browser());

  ASSERT_TRUE(window_controller()->GetChildWebContents());
  DestructionObserver w(window_controller()->GetChildWebContents());

  // Now open the window a second time, without previously closing the original
  // window.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "window.requestPictureInPictureWindow()"
                         ".then(w => true)"));
  base::RunLoop().RunUntilIdle();

  // The first WebContents should be destroyed.
  EXPECT_TRUE(w.is_destroyed());

  // The new WebContents should be visible and unfocused.
  ASSERT_TRUE(GetRenderWidgetHostView());
  EXPECT_TRUE(GetRenderWidgetHostView()->IsShowing());
  EXPECT_TRUE(!GetRenderWidgetHostView()->HasFocus());
}

// Tests closing the document picture-in-picture window.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Document PiP is not supported in Lacros yet.
#define MAYBE_CloseWindow DISABLED_CloseWindow
#else
#define MAYBE_CloseWindow CloseWindow
#endif
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_CloseWindow) {
  LoadTabAndEnterPictureInPicture(browser());

  window_controller()->Close(/*should_pause_video=*/true);

  ASSERT_FALSE(window_controller()->GetChildWebContents());
}

// Tests navigating the opener closes the picture in picture window.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Document PiP is not supported in Lacros yet.
#define MAYBE_ClosePictureInPictureWhenOpenerNavigates \
  DISABLED_ClosePictureInPictureWhenOpenerNavigates
#else
#define MAYBE_ClosePictureInPictureWhenOpenerNavigates \
  ClosePictureInPictureWhenOpenerNavigates
#endif
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_ClosePictureInPictureWhenOpenerNavigates) {
  LoadTabAndEnterPictureInPicture(browser());
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureDocumentPipPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));
  ASSERT_FALSE(window_controller()->GetChildWebContents());
}

// Navigation by the pip window to a new document should close the pip window.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Document PiP is not supported in Lacros yet.
#define MAYBE_CloseOnPictureInPictureNavigationToNewDocument \
  DISABLED_CloseOnPictureInPictureNavigationToNewDocument
#else
#define MAYBE_CloseOnPictureInPictureNavigationToNewDocument \
  CloseOnPictureInPictureNavigationToNewDocument
#endif
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_CloseOnPictureInPictureNavigationToNewDocument) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "navigateInDocumentPipWindow('http://media/"
                         "picture_in_picture/blank.html');"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_controller()->GetChildWebContents());
}

// Navigation within the pip window's document should not close the pip window.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Document PiP is not supported in Lacros yet.
#define MAYBE_DoNotCloseOnPictureInPictureNavigationInsideDocument \
  DISABLED_DoNotCloseOnPictureInPictureNavigationInsideDocument
#else
#define MAYBE_DoNotCloseOnPictureInPictureNavigationInsideDocument \
  DoNotCloseOnPictureInPictureNavigationInsideDocument
#endif
IN_PROC_BROWSER_TEST_F(
    DocumentPictureInPictureWindowControllerBrowserTest,
    MAYBE_DoNotCloseOnPictureInPictureNavigationInsideDocument) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "navigateInDocumentPipWindow('#top');"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_controller()->GetChildWebContents());
}

// Make sure that document PiP fails without a secure context.
// Unlike other tests, this one does work on Lacros.
// TODO(crbug.com/1328840): Consider replacing this with a web platform test.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       RequiresSecureContext) {
  GURL test_page_url("http://media/picture-in-picture/blank.html");
  ASSERT_FALSE(network::IsUrlPotentiallyTrustworthy(test_page_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  // In an insecure context, there should not be a method.
  EXPECT_EQ(false, EvalJs(active_web_contents,
                          "'requestPictureInPictureWindow' in window"));
}
