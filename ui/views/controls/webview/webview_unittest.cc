// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_content_browser_client.h"
#include "ui/aura/window.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/widget_test.h"

namespace {

// Provides functionality to create a test WebContents.
class WebViewTestViewsDelegate : public views::TestViewsDelegate {
 public:
  WebViewTestViewsDelegate() {}
  virtual ~WebViewTestViewsDelegate() {}

  // Overriden from TestViewsDelegate.
  virtual content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) OVERRIDE {
    return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                             site_instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewTestViewsDelegate);
};

// Provides functionality to test a WebView.
class WebViewUnitTest : public views::test::WidgetTest {
 public:
  WebViewUnitTest()
      : ui_thread_(content::BrowserThread::UI, base::MessageLoop::current()),
        file_blocking_thread_(content::BrowserThread::FILE_USER_BLOCKING,
                              base::MessageLoop::current()),
        io_thread_(content::BrowserThread::IO, base::MessageLoop::current()) {}

  virtual ~WebViewUnitTest() {}

  virtual void SetUp() OVERRIDE {
    // The ViewsDelegate is deleted when the ViewsTestBase class is torn down.
    WidgetTest::set_views_delegate(new WebViewTestViewsDelegate);
    browser_context_.reset(new content::TestBrowserContext);
    WidgetTest::SetUp();
    // Set the test content browser client to avoid pulling in needless
    // dependencies from content.
    SetBrowserClientForTesting(&test_browser_client_);
  }

  virtual void TearDown() OVERRIDE {
    browser_context_.reset(NULL);
    // Flush the message loop to execute pending relase tasks as this would
    // upset ASAN and Valgrind.
    RunPendingMessages();
    WidgetTest::TearDown();
  }

 protected:
  content::BrowserContext* browser_context() { return browser_context_.get(); }

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_blocking_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<content::TestBrowserContext> browser_context_;
  scoped_ptr<WebViewTestViewsDelegate> views_delegate_;
  content::TestContentBrowserClient test_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(WebViewUnitTest);
};

// Provides functionaity to observe events on a WebContents like WasShown/
// WasHidden/WebContentsDestroyed.
class WebViewTestWebContentsObserver : public content::WebContentsObserver {
 public:
  WebViewTestWebContentsObserver(content::WebContents* web_contents)
      : web_contents_(static_cast<content::WebContentsImpl*>(web_contents)),
        was_shown_(false),
        shown_count_(0),
        hidden_count_(0) {
    content::WebContentsObserver::Observe(web_contents);
  }

  virtual ~WebViewTestWebContentsObserver() {
    if (web_contents_)
      content::WebContentsObserver::Observe(NULL);
  }

  virtual void WebContentsDestroyed() OVERRIDE {
    DCHECK(web_contents_);
    content::WebContentsObserver::Observe(NULL);
    web_contents_ = NULL;
  }

  virtual void WasShown() OVERRIDE {
    valid_root_while_shown_ =
        web_contents()->GetNativeView()->GetRootWindow() != NULL;
    was_shown_ = true;
    ++shown_count_;
  }

  virtual void WasHidden() OVERRIDE {
    was_shown_ = false;
    ++hidden_count_;
  }

  bool was_shown() const { return was_shown_; }

  int shown_count() const { return shown_count_; }

  int hidden_count() const { return hidden_count_; }

  bool valid_root_while_shown() const { return valid_root_while_shown_; }

 private:
  content::WebContentsImpl* web_contents_;
  bool was_shown_;
  int32 shown_count_;
  int32 hidden_count_;
  // Set to true if the view containing the webcontents has a valid root window.
  bool valid_root_while_shown_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTestWebContentsObserver);
};

// Tests that attaching and detaching a WebContents to a WebView makes the
// WebContents visible and hidden respectively.
TEST_F(WebViewUnitTest, TestWebViewAttachDetachWebContents) {
  // Create a top level widget and a webview as its content.
  views::Widget* widget = CreateTopLevelFramelessPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 10, 100, 100));
  views::WebView* webview = new views::WebView(browser_context());
  widget->SetContentsView(webview);
  widget->Show();

  // Case 1: Create a new WebContents and set it in the webview via
  // SetWebContents. This should make the WebContents visible.
  content::WebContents::CreateParams params(browser_context());
  scoped_ptr<content::WebContents> web_contents1(
      content::WebContents::Create(params));
  WebViewTestWebContentsObserver observer1(web_contents1.get());
  EXPECT_FALSE(observer1.was_shown());

  webview->SetWebContents(web_contents1.get());
  EXPECT_TRUE(observer1.was_shown());
  EXPECT_TRUE(web_contents1->GetNativeView()->IsVisible());
  EXPECT_EQ(observer1.shown_count(), 1);
  EXPECT_EQ(observer1.hidden_count(), 0);
  EXPECT_TRUE(observer1.valid_root_while_shown());

  // Case 2: Create another WebContents and replace the current WebContents
  // via SetWebContents(). This should hide the current WebContents and show
  // the new one.
  content::WebContents::CreateParams params2(browser_context());
  scoped_ptr<content::WebContents> web_contents2(
      content::WebContents::Create(params2));

  WebViewTestWebContentsObserver observer2(web_contents2.get());
  EXPECT_FALSE(observer2.was_shown());

  // Setting the new WebContents should hide the existing one.
  webview->SetWebContents(web_contents2.get());
  EXPECT_FALSE(observer1.was_shown());
  EXPECT_TRUE(observer2.was_shown());
  EXPECT_TRUE(observer2.valid_root_while_shown());

  // WebContents1 should not get stray show calls when WebContents2 is set.
  EXPECT_EQ(observer1.shown_count(), 1);
  EXPECT_EQ(observer1.hidden_count(), 1);
  EXPECT_EQ(observer2.shown_count(), 1);
  EXPECT_EQ(observer2.hidden_count(), 0);

  widget->Close();
  RunPendingMessages();
}

}  // namespace
