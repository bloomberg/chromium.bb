/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "core/paint/LinkHighlightImpl.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Node.h"
#include "core/events/WebInputEventConversion.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/input/EventHandler.h"
#include "core/page/Page.h"
#include "core/page/TouchDisambiguation.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

GestureEventWithHitTestResults GetTargetedEvent(WebViewBase* web_view_impl,
                                                WebGestureEvent& touch_event) {
  WebGestureEvent scaled_event = TransformWebGestureEvent(
      web_view_impl->MainFrameImpl()->GetFrameView(), touch_event);
  return web_view_impl->GetPage()
      ->DeprecatedLocalMainFrame()
      ->GetEventHandler()
      .TargetGestureEvent(scaled_event, true);
}

std::string RegisterMockedURLLoad() {
  WebURL url = URLTestHelpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://www.test.com/"), testing::WebTestDataPath(),
      WebString::FromUTF8("test_touch_link_highlight.html"));
  return url.GetString().Utf8();
}

}  // namespace

TEST(LinkHighlightImplTest, verifyWebViewImplIntegration) {
  const std::string url = RegisterMockedURLLoad();
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.InitializeAndLoad(url);
  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::kTimeStampForTesting);
  touch_event.source_device = kWebGestureDeviceTouchscreen;

  // The coordinates below are linked to absolute positions in the referenced
  // .html file.
  touch_event.x = 20;
  touch_event.y = 20;

  ASSERT_TRUE(
      web_view_impl->BestTapNode(GetTargetedEvent(web_view_impl, touch_event)));

  touch_event.y = 40;
  EXPECT_FALSE(
      web_view_impl->BestTapNode(GetTargetedEvent(web_view_impl, touch_event)));

  touch_event.y = 20;
  // Shouldn't crash.
  web_view_impl->EnableTapHighlightAtPoint(
      GetTargetedEvent(web_view_impl, touch_event));

  EXPECT_TRUE(web_view_impl->GetLinkHighlight(0));
  EXPECT_TRUE(web_view_impl->GetLinkHighlight(0)->ContentLayer());
  EXPECT_TRUE(web_view_impl->GetLinkHighlight(0)->ClipLayer());

  // Find a target inside a scrollable div
  touch_event.y = 100;
  web_view_impl->EnableTapHighlightAtPoint(
      GetTargetedEvent(web_view_impl, touch_event));
  ASSERT_TRUE(web_view_impl->GetLinkHighlight(0));

  // Don't highlight if no "hand cursor"
  touch_event.y = 220;  // An A-link with cross-hair cursor.
  web_view_impl->EnableTapHighlightAtPoint(
      GetTargetedEvent(web_view_impl, touch_event));
  ASSERT_EQ(0U, web_view_impl->NumLinkHighlights());

  touch_event.y = 260;  // A text input box.
  web_view_impl->EnableTapHighlightAtPoint(
      GetTargetedEvent(web_view_impl, touch_event));
  ASSERT_EQ(0U, web_view_impl->NumLinkHighlights());

  Platform::Current()
      ->GetURLLoaderMockFactory()
      ->UnregisterAllURLsAndClearMemoryCache();
}

namespace {

class FakeCompositingWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  FrameTestHelpers::TestWebFrameClient fake_web_frame_client_;
};

FakeCompositingWebViewClient* CompositingWebViewClient() {
  DEFINE_STATIC_LOCAL(FakeCompositingWebViewClient, client, ());
  return &client;
}

}  // anonymous namespace

TEST(LinkHighlightImplTest, resetDuringNodeRemoval) {
  const std::string url = RegisterMockedURLLoad();
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.InitializeAndLoad(
      url, nullptr, CompositingWebViewClient());

  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::kTimeStampForTesting);
  touch_event.source_device = kWebGestureDeviceTouchscreen;
  touch_event.x = 20;
  touch_event.y = 20;

  GestureEventWithHitTestResults targeted_event =
      GetTargetedEvent(web_view_impl, touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  ASSERT_TRUE(web_view_impl->GetLinkHighlight(0));

  GraphicsLayer* highlight_layer =
      web_view_impl->GetLinkHighlight(0)->CurrentGraphicsLayerForTesting();
  ASSERT_TRUE(highlight_layer);
  EXPECT_TRUE(highlight_layer->GetLinkHighlight(0));

  touch_node->remove(IGNORE_EXCEPTION_FOR_TESTING);
  web_view_impl->UpdateAllLifecyclePhases();
  ASSERT_EQ(0U, highlight_layer->NumLinkHighlights());

  Platform::Current()
      ->GetURLLoaderMockFactory()
      ->UnregisterAllURLsAndClearMemoryCache();
}

// A lifetime test: delete LayerTreeView while running LinkHighlights.
TEST(LinkHighlightImplTest, resetLayerTreeView) {
  FakeCompositingWebViewClient web_view_client;

  const std::string url = RegisterMockedURLLoad();
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl =
      web_view_helper.InitializeAndLoad(url, nullptr, &web_view_client);

  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::kTimeStampForTesting);
  touch_event.source_device = kWebGestureDeviceTouchscreen;
  touch_event.x = 20;
  touch_event.y = 20;

  GestureEventWithHitTestResults targeted_event =
      GetTargetedEvent(web_view_impl, touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  ASSERT_TRUE(web_view_impl->GetLinkHighlight(0));

  GraphicsLayer* highlight_layer =
      web_view_impl->GetLinkHighlight(0)->CurrentGraphicsLayerForTesting();
  ASSERT_TRUE(highlight_layer);
  EXPECT_TRUE(highlight_layer->GetLinkHighlight(0));

  // Mimic the logic from RenderWidget::Close:
  web_view_impl->WillCloseLayerTreeView();
  web_view_helper.Reset();

  Platform::Current()
      ->GetURLLoaderMockFactory()
      ->UnregisterAllURLsAndClearMemoryCache();
}

TEST(LinkHighlightImplTest, multipleHighlights) {
  const std::string url = RegisterMockedURLLoad();
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.InitializeAndLoad(
      url, nullptr, CompositingWebViewClient());

  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event;
  touch_event.x = 50;
  touch_event.y = 310;
  touch_event.data.tap.width = 30;
  touch_event.data.tap.height = 30;

  Vector<IntRect> good_targets;
  HeapVector<Member<Node>> highlight_nodes;
  IntRect bounding_box(touch_event.x - touch_event.data.tap.width / 2,
                       touch_event.y - touch_event.data.tap.height / 2,
                       touch_event.data.tap.width, touch_event.data.tap.height);
  FindGoodTouchTargets(bounding_box, web_view_impl->MainFrameImpl()->GetFrame(),
                       good_targets, highlight_nodes);

  web_view_impl->EnableTapHighlights(highlight_nodes);
  EXPECT_EQ(2U, web_view_impl->NumLinkHighlights());

  Platform::Current()
      ->GetURLLoaderMockFactory()
      ->UnregisterAllURLsAndClearMemoryCache();
}

}  // namespace blink
