/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/StaticNodeList.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "platform/graphics/TouchAction.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebTouchEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebView.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidgetClient.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::testing::RunPendingTasks;

namespace blink {

class TouchActionTrackingWebWidgetClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  TouchActionTrackingWebWidgetClient()
      : action_set_count_(0), action_(TouchAction::kTouchActionAuto) {}

  // WebWidgetClient methods
  void SetTouchAction(TouchAction touch_action) override {
    action_set_count_++;
    action_ = touch_action;
  }

  // Local methods
  void Reset() {
    action_set_count_ = 0;
    action_ = TouchAction::kTouchActionAuto;
  }

  int TouchActionSetCount() { return action_set_count_; }

  TouchAction LastTouchAction() { return action_; }

 private:
  int action_set_count_;
  TouchAction action_;
};

const int kKfakeTouchId = 7;

class TouchActionTest : public ::testing::Test {
 public:
  TouchActionTest() : base_url_("http://www.test.com/") {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::CoreTestDataPath(),
        "touch-action-tests.css");
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::CoreTestDataPath(),
        "touch-action-tests.js");
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::CoreTestDataPath(),
        "white-1x1.png");
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void RunTouchActionTest(std::string file);
  void RunShadowDOMTest(std::string file);
  void RunIFrameTest(std::string file);
  void SendTouchEvent(WebView*, WebInputEvent::Type, IntPoint client_point);
  WebViewBase* SetupTest(std::string file, TouchActionTrackingWebWidgetClient&);
  void RunTestOnTree(ContainerNode* root,
                     WebView*,
                     TouchActionTrackingWebWidgetClient&);

  std::string base_url_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

void TouchActionTest::RunTouchActionTest(std::string file) {
  TouchActionTrackingWebWidgetClient client;

  // runTouchActionTest() loads a document in a frame, setting up a
  // nested run loop. Should any Oilpan GC happen while it is in
  // effect, the implicit assumption that we're outside any event
  // loop (=> there being no pointers on the stack needing scanning)
  // when that GC strikes will no longer hold.
  //
  // To ensure that the references on the stack are also traced, we
  // turn them into persistent, stack allocated references. This
  // workaround is sufficient to handle this artificial test
  // scenario.
  WebViewBase* web_view = SetupTest(file, client);

  Persistent<Document> document =
      static_cast<Document*>(web_view->MainFrameImpl()->GetDocument());
  RunTestOnTree(document.Get(), web_view, client);

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

void TouchActionTest::RunShadowDOMTest(std::string file) {
  TouchActionTrackingWebWidgetClient client;

  WebViewBase* web_view = SetupTest(file, client);

  DummyExceptionStateForTesting es;

  // Oilpan: see runTouchActionTest() comment why these are persistent
  // references.
  Persistent<Document> document =
      static_cast<Document*>(web_view->MainFrameImpl()->GetDocument());
  Persistent<StaticElementList> host_nodes =
      document->QuerySelectorAll("[shadow-host]", es);
  ASSERT_FALSE(es.HadException());
  ASSERT_GE(host_nodes->length(), 1u);

  for (unsigned index = 0; index < host_nodes->length(); index++) {
    ShadowRoot* shadow_root = host_nodes->item(index)->openShadowRoot();
    RunTestOnTree(shadow_root, web_view, client);
  }

  // Projections show up in the main document.
  RunTestOnTree(document.Get(), web_view, client);

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

void TouchActionTest::RunIFrameTest(std::string file) {
  TouchActionTrackingWebWidgetClient client;

  WebViewBase* web_view = SetupTest(file, client);
  WebFrame* cur_frame = web_view->MainFrame()->FirstChild();
  ASSERT_TRUE(cur_frame);

  for (; cur_frame; cur_frame = cur_frame->NextSibling()) {
    // Oilpan: see runTouchActionTest() comment why these are persistent
    // references.
    Persistent<Document> content_doc =
        static_cast<Document*>(cur_frame->ToWebLocalFrame()->GetDocument());
    RunTestOnTree(content_doc.Get(), web_view, client);
  }

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

WebViewBase* TouchActionTest::SetupTest(
    std::string file,
    TouchActionTrackingWebWidgetClient& client) {
  URLTestHelpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url_),
                                                testing::CoreTestDataPath(),
                                                WebString::FromUTF8(file));
  // Note that JavaScript must be enabled for shadow DOM tests.
  WebViewBase* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + file, nullptr, nullptr, &client);

  // Set size to enable hit testing, and avoid line wrapping for consistency
  // with browser.
  web_view->Resize(WebSize(800, 1200));

  // Scroll to verify the code properly transforms windows to client co-ords.
  const int kScrollOffset = 100;
  Document* document =
      static_cast<Document*>(web_view->MainFrameImpl()->GetDocument());
  document->GetFrame()->View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, kScrollOffset), kProgrammaticScroll);

  return web_view;
}

IntRect WindowClipRect(const LocalFrameView& frame_view) {
  LayoutRect clip_rect(
      LayoutPoint(),
      LayoutSize(frame_view.VisibleContentSize(kExcludeScrollbars)));
  frame_view.GetLayoutViewItem().MapToVisualRectInAncestorSpace(
      &frame_view.GetLayoutView()->ContainerForPaintInvalidation(), clip_rect);
  return EnclosingIntRect(clip_rect);
}

void TouchActionTest::RunTestOnTree(
    ContainerNode* root,
    WebView* web_view,
    TouchActionTrackingWebWidgetClient& client) {
  // Find all elements to test the touch-action of in the document.
  DummyExceptionStateForTesting es;

  // Oilpan: see runTouchActionTest() comment why these are persistent
  // references.
  Persistent<StaticElementList> elements =
      root->QuerySelectorAll("[expected-action]", es);
  ASSERT_FALSE(es.HadException());

  for (unsigned index = 0; index < elements->length(); index++) {
    Element* element = elements->item(index);
    element->scrollIntoViewIfNeeded();

    std::string failure_context("Test case: ");
    if (element->HasID()) {
      failure_context.append(element->GetIdAttribute().Ascii().data());
    } else if (element->firstChild()) {
      failure_context.append("\"");
      failure_context.append(element->firstChild()
                                 ->textContent(false)
                                 .StripWhiteSpace()
                                 .Ascii()
                                 .data());
      failure_context.append("\"");
    } else {
      failure_context += "<missing ID>";
    }

    // Run each test three times at different positions in the element.
    // Note that we don't want the bounding box because our tests sometimes have
    // elements with multiple border boxes with other elements in between. Use
    // the first border box (which we can easily visualize in a browser for
    // debugging).
    Persistent<ClientRectList> rects = element->getClientRects();
    ASSERT_GE(rects->length(), 0u) << failure_context;
    Persistent<ClientRect> r = rects->item(0);
    FloatRect client_float_rect =
        FloatRect(r->left(), r->top(), r->width(), r->height());
    IntRect client_rect = EnclosedIntRect(client_float_rect);
    for (int loc_idx = 0; loc_idx < 3; loc_idx++) {
      IntPoint frame_point;
      std::stringstream context_stream;
      context_stream << failure_context << " (";
      switch (loc_idx) {
        case 0:
          frame_point = client_rect.Center();
          context_stream << "center";
          break;
        case 1:
          frame_point = client_rect.Location();
          context_stream << "top-left";
          break;
        case 2:
          frame_point = client_rect.MaxXMaxYCorner();
          frame_point.Move(-1, -1);
          context_stream << "bottom-right";
          break;
        default:
          FAIL() << "Invalid location index.";
      }

      IntPoint window_point =
          root->GetDocument().GetFrame()->View()->ConvertToRootFrame(
              frame_point);
      context_stream << "=" << window_point.X() << "," << window_point.Y()
                     << ").";
      std::string failure_context_pos = context_stream.str();

      LocalFrame* main_frame =
          ToLocalFrame(WebFrame::ToCoreFrame(*web_view->MainFrame()));
      LocalFrameView* main_frame_view = main_frame->View();
      IntRect visible_rect = WindowClipRect(*main_frame_view);
      ASSERT_TRUE(visible_rect.Contains(window_point))
          << failure_context_pos
          << " Test point not contained in visible area: " << visible_rect.X()
          << "," << visible_rect.Y() << "-" << visible_rect.MaxX() << ","
          << visible_rect.MaxY();

      // First validate that a hit test at this point will really hit the
      // element we intended. This is the easiest way for a test to be broken,
      // but has nothing really to do with touch action.  Note that we can't use
      // WebView's hit test API because it doesn't look into shadow DOM.
      IntPoint doc_point(main_frame_view->FrameToContents(window_point));
      HitTestResult result = main_frame->GetEventHandler().HitTestResultAtPoint(
          doc_point, HitTestRequest::kReadOnly | HitTestRequest::kActive);
      ASSERT_EQ(element, result.InnerElement())
          << "Unexpected hit test result " << failure_context_pos
          << "  Got element: \""
          << result.InnerElement()
                 ->outerHTML()
                 .StripWhiteSpace()
                 .Left(80)
                 .Ascii()
                 .data()
          << "\"" << std::endl
          << "Document render tree:" << std::endl
          << ExternalRepresentation(root->GetDocument().GetFrame())
                 .Utf8()
                 .data();

      // Now send the touch event and check any touch action result.
      SendTouchEvent(web_view, WebInputEvent::kTouchStart, window_point);

      AtomicString expected_action = element->getAttribute("expected-action");
      if (expected_action == "auto") {
        // Auto is the default - no action set.
        EXPECT_EQ(0, client.TouchActionSetCount()) << failure_context_pos;
        EXPECT_EQ(TouchAction::kTouchActionAuto, client.LastTouchAction())
            << failure_context_pos;
      } else {
        // Should have received exactly one touch action.
        EXPECT_EQ(1, client.TouchActionSetCount()) << failure_context_pos;
        if (client.TouchActionSetCount()) {
          if (expected_action == "none") {
            EXPECT_EQ(TouchAction::kTouchActionNone, client.LastTouchAction())
                << failure_context_pos;
          } else if (expected_action == "pan-x") {
            EXPECT_EQ(TouchAction::kTouchActionPanX, client.LastTouchAction())
                << failure_context_pos;
          } else if (expected_action == "pan-y") {
            EXPECT_EQ(TouchAction::kTouchActionPanY, client.LastTouchAction())
                << failure_context_pos;
          } else if (expected_action == "pan-x-y") {
            EXPECT_EQ((TouchAction::kTouchActionPan), client.LastTouchAction())
                << failure_context_pos;
          } else if (expected_action == "manipulation") {
            EXPECT_EQ((TouchAction::kTouchActionManipulation),
                      client.LastTouchAction())
                << failure_context_pos;
          } else {
            FAIL() << "Unrecognized expected-action \""
                   << expected_action.Ascii().data() << "\" "
                   << failure_context_pos;
          }
        }
      }

      // Reset webview touch state.
      client.Reset();
      SendTouchEvent(web_view, WebInputEvent::kTouchCancel, window_point);
      EXPECT_EQ(0, client.TouchActionSetCount());
    }
  }
}
void TouchActionTest::SendTouchEvent(WebView* web_view,
                                     WebInputEvent::Type type,
                                     IntPoint client_point) {
  ASSERT_TRUE(type == WebInputEvent::kTouchStart ||
              type == WebInputEvent::kTouchCancel);

  WebTouchEvent web_touch_event(type, WebInputEvent::kNoModifiers,
                                WebInputEvent::kTimeStampForTesting);
  if (type == WebInputEvent::kTouchCancel)
    web_touch_event.dispatch_type = WebInputEvent::kEventNonBlocking;
  web_touch_event.touches_length = 1;
  web_touch_event.touches[0].state =
      (type == WebInputEvent::kTouchStart ? WebTouchPoint::kStatePressed
                                          : WebTouchPoint::kStateCancelled);
  web_touch_event.touches[0].id = kKfakeTouchId;
  web_touch_event.touches[0].SetPositionInScreen(client_point.X(),
                                                 client_point.Y());
  web_touch_event.touches[0].SetPositionInWidget(client_point.X(),
                                                 client_point.Y());
  web_touch_event.touches[0].radius_x = 10;
  web_touch_event.touches[0].radius_y = 10;
  web_touch_event.touches[0].force = 1.0;

  web_view->HandleInputEvent(WebCoalescedInputEvent(web_touch_event));
  RunPendingTasks();
}

// crbug.com/411038
TEST_F(TouchActionTest, Simple) {
  RunTouchActionTest("touch-action-simple.html");
}

TEST_F(TouchActionTest, Overflow) {
  RunTouchActionTest("touch-action-overflow.html");
}

TEST_F(TouchActionTest, IFrame) {
  RunIFrameTest("touch-action-iframe.html");
}

TEST_F(TouchActionTest, ShadowDOM) {
  RunShadowDOMTest("touch-action-shadow-dom.html");
}

TEST_F(TouchActionTest, Pan) {
  RunTouchActionTest("touch-action-pan.html");
}

}  // namespace blink
