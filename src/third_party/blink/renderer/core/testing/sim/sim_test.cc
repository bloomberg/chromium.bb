// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

SimTest::SimTest()
    :  // SimCompositor overrides the LayerTreeViewDelegate to respond to
       // BeginMainFrame(), which will update and paint the main frame of the
       // WebViewImpl given to SetWebView().
      web_widget_client_(&compositor_) {
  Document::SetThreadedParsingEnabledForTesting(false);
  // Use the mock theme to get more predictable code paths, this also avoids
  // the OS callbacks in ScrollAnimatorMac which can schedule frames
  // unpredictably since the OS will randomly call into blink for
  // updateScrollerStyleForNewRecommendedScrollerStyle which then does
  // FrameView::scrollbarStyleChanged and will adjust the scrollbar existence
  // in the middle of a test.
  WebTestSupport::SetMockThemeEnabledForTest(true);
  ScrollbarTheme::SetMockScrollbarsEnabled(true);
  // Threaded animations are usually enabled for blink. However these tests use
  // synchronous compositing, which can not run threaded animations.
  bool was_threaded_animation_enabled =
      content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(false);
  // If this fails, we'd be resetting IsThreadedAnimationEnabled() to the wrong
  // thing in the destructor.
  DCHECK(was_threaded_animation_enabled);
}

SimTest::~SimTest() {
  // Pump the message loop to process the load event.
  test::RunPendingTasks();

  Document::SetThreadedParsingEnabledForTesting(true);
  WebTestSupport::SetMockThemeEnabledForTest(false);
  ScrollbarTheme::SetMockScrollbarsEnabled(false);
  content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(true);
  WebCache::Clear();
}

void SimTest::SetUp() {
  Test::SetUp();

  web_view_helper_.Initialize(&web_frame_client_, &web_view_client_,
                              &web_widget_client_);
  compositor_.SetWebView(WebView(), *web_widget_client_.layer_tree_view(),
                         web_view_client_, web_widget_client_);
  page_.SetPage(WebView().GetPage());
}

void SimTest::LoadURL(const String& url_string) {
  KURL url(url_string);
  frame_test_helpers::LoadFrameDontWait(WebView().MainFrameImpl(), url);
  if (DocumentLoader::WillLoadUrlAsEmpty(url) || url.ProtocolIsData()) {
    // Empty documents and data urls are not using mocked out SimRequests,
    // but instead load data directly.
    frame_test_helpers::PumpPendingRequestsForFrameToLoad(
        WebView().MainFrameImpl());
  }
}

LocalDOMWindow& SimTest::Window() {
  return *GetDocument().domWindow();
}

SimPage& SimTest::GetPage() {
  return page_;
}

Document& SimTest::GetDocument() {
  return *WebView().MainFrameImpl()->GetFrame()->GetDocument();
}

WebViewImpl& SimTest::WebView() {
  return *web_view_helper_.GetWebView();
}

WebLocalFrameImpl& SimTest::MainFrame() {
  return *WebView().MainFrameImpl();
}

frame_test_helpers::TestWebViewClient& SimTest::WebViewClient() {
  return web_view_client_;
}

frame_test_helpers::TestWebWidgetClient& SimTest::WebWidgetClient() {
  return web_widget_client_;
}

frame_test_helpers::TestWebFrameClient& SimTest::WebFrameClient() {
  return web_frame_client_;
}

SimCompositor& SimTest::Compositor() {
  return compositor_;
}

Vector<String>& SimTest::ConsoleMessages() {
  return web_frame_client_.ConsoleMessages();
}

}  // namespace blink
