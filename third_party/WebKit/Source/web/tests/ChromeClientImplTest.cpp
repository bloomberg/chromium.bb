/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/loader/FrameLoadRequest.h"
#include "core/page/Page.h"
#include "core/page/ScopedPageSuspender.h"
#include "public/platform/WebInputEvent.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebView.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

void SetCurrentInputEventForTest(const WebInputEvent* event) {
  WebViewImpl::current_input_event_ = event;
}

namespace {

class TestWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  explicit TestWebViewClient(WebNavigationPolicy* target) : target_(target) {}
  ~TestWebViewClient() override {}

  void Show(WebNavigationPolicy policy) override { *target_ = policy; }

 private:
  WebNavigationPolicy* target_;
};

}  // anonymous namespace

class GetNavigationPolicyTest : public testing::Test {
 public:
  GetNavigationPolicyTest()
      : result_(kWebNavigationPolicyIgnore), web_view_client_(&result_) {}

 protected:
  void SetUp() override {
    web_view_ = ToWebViewImpl(
        WebView::Create(&web_view_client_, kWebPageVisibilityStateVisible));
    web_view_->SetMainFrame(WebLocalFrame::Create(
        WebTreeScopeType::kDocument, &web_frame_client_, nullptr, nullptr));
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
    result_ = kWebNavigationPolicyIgnore;
  }

  void TearDown() override { web_view_->Close(); }

  WebNavigationPolicy GetNavigationPolicyWithMouseEvent(
      int modifiers,
      WebMouseEvent::Button button,
      bool as_popup) {
    WebMouseEvent event(WebInputEvent::kMouseUp, modifiers,
                        WebInputEvent::kTimeStampForTesting);
    event.button = button;
    SetCurrentInputEventForTest(&event);
    chrome_client_impl_->SetToolbarsVisible(!as_popup);
    chrome_client_impl_->Show(kNavigationPolicyIgnore);
    SetCurrentInputEventForTest(0);
    return result_;
  }

  bool IsNavigationPolicyPopup() {
    chrome_client_impl_->Show(kNavigationPolicyIgnore);
    return result_ == kWebNavigationPolicyNewPopup;
  }

 protected:
  WebNavigationPolicy result_;
  TestWebViewClient web_view_client_;
  WebViewImpl* web_view_;
  FrameTestHelpers::TestWebFrameClient web_frame_client_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

TEST_F(GetNavigationPolicyTest, LeftClick) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kWebNavigationPolicyNewForegroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, LeftClickPopup) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kWebNavigationPolicyNewPopup,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, ShiftLeftClick) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kWebNavigationPolicyNewWindow,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, ShiftLeftClickPopup) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kWebNavigationPolicyNewPopup,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, ControlOrMetaLeftClick) {
#if OS(MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kWebNavigationPolicyNewBackgroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, ControlOrMetaLeftClickPopup) {
#if OS(MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kWebNavigationPolicyNewBackgroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, ControlOrMetaAndShiftLeftClick) {
#if OS(MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  modifiers |= WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kWebNavigationPolicyNewForegroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, ControlOrMetaAndShiftLeftClickPopup) {
#if OS(MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  modifiers |= WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kWebNavigationPolicyNewForegroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, MiddleClick) {
  int modifiers = 0;
  bool as_popup = false;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kWebNavigationPolicyNewBackgroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, MiddleClickPopup) {
  int modifiers = 0;
  bool as_popup = true;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kWebNavigationPolicyNewBackgroundTab,
            GetNavigationPolicyWithMouseEvent(modifiers, button, as_popup));
}

TEST_F(GetNavigationPolicyTest, NoToolbarsForcesPopup) {
  chrome_client_impl_->SetToolbarsVisible(false);
  EXPECT_TRUE(IsNavigationPolicyPopup());
  chrome_client_impl_->SetToolbarsVisible(true);
  EXPECT_FALSE(IsNavigationPolicyPopup());
}

TEST_F(GetNavigationPolicyTest, NoStatusbarIsNotPopup) {
  chrome_client_impl_->SetStatusbarVisible(false);
  EXPECT_FALSE(IsNavigationPolicyPopup());
  chrome_client_impl_->SetStatusbarVisible(true);
  EXPECT_FALSE(IsNavigationPolicyPopup());
}

TEST_F(GetNavigationPolicyTest, NoMenubarIsNotPopup) {
  chrome_client_impl_->SetMenubarVisible(false);
  EXPECT_FALSE(IsNavigationPolicyPopup());
  chrome_client_impl_->SetMenubarVisible(true);
  EXPECT_FALSE(IsNavigationPolicyPopup());
}

TEST_F(GetNavigationPolicyTest, NotResizableIsNotPopup) {
  chrome_client_impl_->SetResizable(false);
  EXPECT_FALSE(IsNavigationPolicyPopup());
  chrome_client_impl_->SetResizable(true);
  EXPECT_FALSE(IsNavigationPolicyPopup());
}

class ViewCreatingClient : public FrameTestHelpers::TestWebViewClient {
 public:
  WebView* CreateView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      bool) override {
    return web_view_helper_.InitializeWithOpener(opener, true);
  }

 private:
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

class CreateWindowTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_ = ToWebViewImpl(
        WebView::Create(&web_view_client_, kWebPageVisibilityStateVisible));
    main_frame_ = WebLocalFrame::Create(WebTreeScopeType::kDocument,
                                        &web_frame_client_, nullptr, nullptr);
    web_view_->SetMainFrame(main_frame_);
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
  }

  void TearDown() override { web_view_->Close(); }

  ViewCreatingClient web_view_client_;
  WebViewImpl* web_view_;
  WebLocalFrame* main_frame_;
  FrameTestHelpers::TestWebFrameClient web_frame_client_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

TEST_F(CreateWindowTest, CreateWindowFromSuspendedPage) {
  ScopedPageSuspender suspender;
  LocalFrame* frame = ToWebLocalFrameImpl(main_frame_)->GetFrame();
  FrameLoadRequest request(frame->GetDocument());
  WindowFeatures features;
  EXPECT_EQ(nullptr,
            chrome_client_impl_->CreateWindow(
                frame, request, features, kNavigationPolicyNewForegroundTab));
}

}  // namespace blink
