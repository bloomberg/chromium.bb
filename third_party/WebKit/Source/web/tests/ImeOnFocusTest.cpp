// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLElement.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebDocument.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::FrameTestHelpers::LoadFrame;
using blink::testing::RunPendingTasks;
using blink::URLTestHelpers::RegisterMockedURLLoadFromBase;

namespace blink {

class ImeRequestTrackingWebViewClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  ImeRequestTrackingWebViewClient() : virtual_keyboard_request_count_(0) {}

  // WebWidgetClient methods
  void ShowVirtualKeyboardOnElementFocus() override {
    ++virtual_keyboard_request_count_;
  }

  // Local methds
  void Reset() { virtual_keyboard_request_count_ = 0; }

  int VirtualKeyboardRequestCount() { return virtual_keyboard_request_count_; }

 private:
  int virtual_keyboard_request_count_;
};

class ImeOnFocusTest : public ::testing::Test {
 public:
  ImeOnFocusTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void SendGestureTap(WebView*, IntPoint);
  void Focus(const AtomicString& element);
  void RunImeOnFocusTest(std::string file_name,
                         int,
                         IntPoint tap_point = IntPoint(-1, -1),
                         const AtomicString& focus_element = g_null_atom,
                         std::string frame = "");

  std::string base_url_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
  Persistent<Document> document_;
};

void ImeOnFocusTest::SendGestureTap(WebView* web_view, IntPoint client_point) {
  WebGestureEvent web_gesture_event(WebInputEvent::kGestureTap,
                                    WebInputEvent::kNoModifiers,
                                    WebInputEvent::kTimeStampForTesting);
  // GestureTap is only ever from touch screens.
  web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
  web_gesture_event.x = client_point.X();
  web_gesture_event.y = client_point.Y();
  web_gesture_event.global_x = client_point.X();
  web_gesture_event.global_y = client_point.Y();
  web_gesture_event.data.tap.tap_count = 1;
  web_gesture_event.data.tap.width = 10;
  web_gesture_event.data.tap.height = 10;

  web_view->HandleInputEvent(WebCoalescedInputEvent(web_gesture_event));
  RunPendingTasks();
}

void ImeOnFocusTest::Focus(const AtomicString& element) {
  document_->body()->getElementById(element)->focus();
}

void ImeOnFocusTest::RunImeOnFocusTest(
    std::string file_name,
    int expected_virtual_keyboard_request_count,
    IntPoint tap_point,
    const AtomicString& focus_element,
    std::string frame) {
  ImeRequestTrackingWebViewClient client;
  RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url_),
                                testing::WebTestDataPath(),
                                WebString::FromUTF8(file_name));
  WebViewBase* web_view =
      web_view_helper_.Initialize(nullptr, nullptr, &client);
  web_view->Resize(WebSize(800, 1200));
  LoadFrame(web_view->MainFrameImpl(), base_url_ + file_name);
  document_ = web_view_helper_.WebView()
                  ->MainFrameImpl()
                  ->GetDocument()
                  .Unwrap<Document>();

  if (!focus_element.IsNull())
    Focus(focus_element);
  EXPECT_EQ(0, client.VirtualKeyboardRequestCount());

  if (tap_point.X() >= 0 && tap_point.Y() >= 0)
    SendGestureTap(web_view, tap_point);

  if (!frame.empty()) {
    RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url_),
                                  testing::WebTestDataPath(),
                                  WebString::FromUTF8(frame));
    WebLocalFrame* child_frame =
        web_view->MainFrame()->FirstChild()->ToWebLocalFrame();
    LoadFrame(child_frame, base_url_ + frame);
  }

  if (!focus_element.IsNull())
    Focus(focus_element);
  EXPECT_EQ(expected_virtual_keyboard_request_count,
            client.VirtualKeyboardRequestCount());

  web_view_helper_.Reset();
}

TEST_F(ImeOnFocusTest, OnLoad) {
  RunImeOnFocusTest("ime-on-focus-on-load.html", 0);
}

TEST_F(ImeOnFocusTest, OnAutofocus) {
  RunImeOnFocusTest("ime-on-focus-on-autofocus.html", 0);
}

TEST_F(ImeOnFocusTest, OnUserGesture) {
  RunImeOnFocusTest("ime-on-focus-on-user-gesture.html", 1, IntPoint(50, 50));
}

TEST_F(ImeOnFocusTest, AfterFirstGesture) {
  RunImeOnFocusTest("ime-on-focus-after-first-gesture.html", 1,
                    IntPoint(50, 50), "input");
}

TEST_F(ImeOnFocusTest, AfterNavigationWithinPage) {
  RunImeOnFocusTest("ime-on-focus-after-navigation-within-page.html", 1,
                    IntPoint(50, 50), "input");
}

TEST_F(ImeOnFocusTest, AfterFrameLoadOnGesture) {
  RunImeOnFocusTest("ime-on-focus-after-frame-load-on-gesture.html", 1,
                    IntPoint(50, 50), "input", "frame.html");
}

}  // namespace blink
