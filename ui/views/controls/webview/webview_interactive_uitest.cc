// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_surface.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/webview_test_helper.h"
#include "ui/views/test/widget_test.h"

namespace {

class WebViewTestViewsDelegate : public views::TestViewsDelegate {
 public:
  WebViewTestViewsDelegate() {}

  // Overriden from TestViewsDelegate.
  content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override {
    return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                             site_instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewTestViewsDelegate);
};

class WebViewInteractiveUiTest : public views::test::WidgetTest {
 public:
  WebViewInteractiveUiTest()
      : ui_thread_(content::BrowserThread::UI, base::MessageLoop::current()) {}

  void SetUp() override {
    gfx::GLSurface::InitializeOneOffForTests();
    // The ViewsDelegate is deleted when the ViewsTestBase class is torn down.
    WidgetTest::set_views_delegate(new WebViewTestViewsDelegate);
    WidgetTest::SetUp();
  }

 protected:
  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::TestBrowserContext browser_context_;
  views::WebViewTestHelper webview_test_helper_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebViewInteractiveUiTest);
};

TEST_F(WebViewInteractiveUiTest, TextInputClientIsUpToDate) {
  // WebViewInteractiveUiTest.TextInputClientIsUpToDate needs
  // kEnableTextInputFocusManager flag to be enabled.
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kEnableTextInputFocusManager);

  // Create a top level widget and a webview as its content.
  views::Widget* widget = CreateTopLevelFramelessPlatformWidget();
  views::WebView* webview = new views::WebView(browser_context());
  widget->SetContentsView(webview);
  widget->Show();
  webview->RequestFocus();

  // Mac needs to spin a run loop to activate. Don't fake it, so that the Widget
  // still gets didBecomeKey notifications. There is just one widget, so a
  // single spin of the runloop should be enough (it didn't flake in local tests
  // but a WidgetObserver might be needed).
#if defined(OS_MACOSX)
  RunPendingMessages();
#endif
  EXPECT_TRUE(widget->IsActive());

  ui::TextInputFocusManager* text_input_focus_manager =
      ui::TextInputFocusManager::GetInstance();
  EXPECT_EQ(nullptr, webview->GetTextInputClient());
  EXPECT_EQ(text_input_focus_manager->GetFocusedTextInputClient(),
            webview->GetTextInputClient());

  // Case 1: Creates a new WebContents.
  content::WebContents* web_contents1 = webview->GetWebContents();
  content::RenderViewHostTester::For(web_contents1->GetRenderViewHost())
      ->CreateRenderView(base::string16(), MSG_ROUTING_NONE, MSG_ROUTING_NONE,
                         -1, false);
  webview->RequestFocus();
  ui::TextInputClient* client1 = webview->GetTextInputClient();
  EXPECT_NE(nullptr, client1);
  EXPECT_EQ(text_input_focus_manager->GetFocusedTextInputClient(), client1);

  // Case 2: Replaces a WebContents by SetWebContents().
  scoped_ptr<content::WebContents> web_contents2(
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr));
  content::RenderViewHostTester::For(web_contents2->GetRenderViewHost())
      ->CreateRenderView(base::string16(), MSG_ROUTING_NONE, MSG_ROUTING_NONE,
                         -1, false);
  webview->SetWebContents(web_contents2.get());
  ui::TextInputClient* client2 = webview->GetTextInputClient();
  EXPECT_NE(nullptr, client2);
  EXPECT_EQ(text_input_focus_manager->GetFocusedTextInputClient(), client2);
  EXPECT_NE(client1, client2);

  widget->Close();
  RunPendingMessages();
}

}  // namespace
