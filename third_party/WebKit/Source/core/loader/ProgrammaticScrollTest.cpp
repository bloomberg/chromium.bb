// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ProgrammaticScrollTest : public ::testing::Test {
 public:
  ProgrammaticScrollTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  std::string base_url_;
};

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithScale) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(kFrameLoadTypeBackForward);

  web_view->SetPageScaleFactor(3.0f);
  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.GetDocumentLoader()->GetHistoryItem()->SetPageScaleFactor(2);
  loader.GetDocumentLoader()->GetHistoryItem()->SetScrollOffset(
      ScrollOffset(0, 200));

  // Flip back the wasScrolledByUser flag which was set to true by
  // setPageScaleFactor because otherwise
  // FrameLoader::restoreScrollPositionAndViewState does nothing.
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.RestoreScrollPositionAndViewState();

  // Expect that both scroll and scale were restored.
  EXPECT_EQ(2.0f, web_view->PageScaleFactor());
  EXPECT_EQ(200, web_view->MainFrameImpl()->GetScrollOffset().height);
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithoutScale) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(kFrameLoadTypeBackForward);

  web_view->SetPageScaleFactor(3.0f);
  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.GetDocumentLoader()->GetHistoryItem()->SetPageScaleFactor(0);
  loader.GetDocumentLoader()->GetHistoryItem()->SetScrollOffset(
      ScrollOffset(0, 400));

  // FrameLoader::restoreScrollPositionAndViewState flows differently if scale
  // is zero.
  loader.RestoreScrollPositionAndViewState();

  // Expect that only the scroll position was restored.
  EXPECT_EQ(3.0f, web_view->PageScaleFactor());
  EXPECT_EQ(400, web_view->MainFrameImpl()->GetScrollOffset().height);
}

class ProgrammaticScrollSimTest : public ::testing::WithParamInterface<bool>,
                                  private ScopedRootLayerScrollingForTest,
                                  public SimTest {
 public:
  ProgrammaticScrollSimTest() : ScopedRootLayerScrollingForTest(GetParam()) {}
};

INSTANTIATE_TEST_CASE_P(All, ProgrammaticScrollSimTest, ::testing::Bool());

TEST_P(ProgrammaticScrollSimTest, NavigateToHash) {
  WebView().Resize(WebSize(800, 600));
  SimRequest main_resource("https://example.com/test.html#target", "text/html");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  LoadURL("https://example.com/test.html#target");

  // Finish loading the main document before the stylesheet is loaded so that
  // rendering is blocked when parsing finishes. This will delay closing the
  // document until the load event.
  main_resource.Start();
  main_resource.Write(
      "<!DOCTYPE html><link id=link rel=stylesheet href=test.css>");
  css_resource.Start();
  main_resource.Write(R"HTML(
    <style>
      body {
        height: 4000px;
      }
      h2 {
        position: absolute;
        top: 3000px;
      }
    </style>
    <h2 id="target">Target</h2>
  )HTML");
  main_resource.Finish();
  css_resource.Complete();
  Compositor().BeginFrame();

  // Run pending tasks to fire the load event and close the document. This
  // should cause the document to scroll to the hash.
  test::RunPendingTasks();

  ScrollableArea* layout_viewport =
      GetDocument().View()->LayoutViewportScrollableArea();
  EXPECT_EQ(3001, layout_viewport->GetScrollOffset().Height());
}

}  // namespace blink
