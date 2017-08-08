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

#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/forms/ColorChooserClient.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/html/forms/DateTimeChooserClient.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/ChromeClientImpl.h"
#include "core/page/Page.h"
#include "core/page/ScopedPageSuspender.h"
#include "platform/Language.h"
#include "public/platform/WebInputEvent.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebView.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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

class ViewCreatingClient : public FrameTestHelpers::TestWebViewClient {
 public:
  WebView* CreateView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      bool,
                      WebSandboxFlags) override {
    return web_view_helper_.InitializeWithOpener(opener);
  }

 private:
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

class CreateWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize(nullptr, &web_view_client_);
    main_frame_ = helper_.LocalMainFrame();
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
  }

  ViewCreatingClient web_view_client_;
  FrameTestHelpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  WebLocalFrame* main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

TEST_F(CreateWindowTest, CreateWindowFromSuspendedPage) {
  ScopedPageSuspender suspender;
  LocalFrame* frame = ToWebLocalFrameImpl(main_frame_)->GetFrame();
  FrameLoadRequest request(frame->GetDocument());
  WebWindowFeatures features;
  EXPECT_EQ(nullptr, chrome_client_impl_->CreateWindow(
                         frame, request, features,
                         kNavigationPolicyNewForegroundTab, kSandboxNone));
}

class FakeColorChooserClient
    : public GarbageCollectedFinalized<FakeColorChooserClient>,
      public ColorChooserClient {
 public:
  FakeColorChooserClient(Element* owner_element)
      : owner_element_(owner_element) {}
  ~FakeColorChooserClient() override {}

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(owner_element_);
    ColorChooserClient::Trace(visitor);
  }

  USING_GARBAGE_COLLECTED_MIXIN(FakeColorChooserClient)

  // ColorChooserClient
  void DidChooseColor(const Color& color) override {}
  void DidEndChooser() override {}
  Element& OwnerElement() const override { return *owner_element_; }
  IntRect ElementRectRelativeToViewport() const override { return IntRect(); }
  Color CurrentColor() override { return Color(); }
  bool ShouldShowSuggestions() const override { return false; }
  Vector<ColorSuggestion> Suggestions() const override {
    return Vector<ColorSuggestion>();
  }

 private:
  Member<Element> owner_element_;
};

class FakeDateTimeChooserClient
    : public GarbageCollectedFinalized<FakeDateTimeChooserClient>,
      public DateTimeChooserClient {
 public:
  FakeDateTimeChooserClient(Element* owner_element)
      : owner_element_(owner_element) {}
  ~FakeDateTimeChooserClient() override {}

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(owner_element_);
    DateTimeChooserClient::Trace(visitor);
  }

  USING_GARBAGE_COLLECTED_MIXIN(FakeDateTimeChooserClient)

  // DateTimeChooserClient
  Element& OwnerElement() const override { return *owner_element_; }
  void DidChooseValue(const String&) override {}
  void DidChooseValue(double) override {}
  void DidEndChooser() override {}

 private:
  Member<Element> owner_element_;
};

class PagePopupSuppressionTest : public ::testing::Test {
 public:
  PagePopupSuppressionTest() {}

  bool CanOpenColorChooser() {
    LocalFrame* frame = main_frame_->GetFrame();
    Color color;
    return !!chrome_client_impl_->OpenColorChooser(frame, color_chooser_client_,
                                                   color);
  }

  bool CanOpenDateTimeChooser() {
    DateTimeChooserParameters params;
    params.locale = DefaultLanguage();
    return !!chrome_client_impl_->OpenDateTimeChooser(date_time_chooser_client_,
                                                      params);
  }

  bool CanOpenPopupMenu() {
    LocalFrame* frame = main_frame_->GetFrame();
    return !!chrome_client_impl_->OpenPopupMenu(*frame, *select_);
  }

  Settings* GetSettings() {
    LocalFrame* frame = main_frame_->GetFrame();
    return frame->GetDocument()->GetSettings();
  }

 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize();
    main_frame_ = helper_.LocalMainFrame();
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
    LocalFrame* frame = helper_.LocalMainFrame()->GetFrame();
    color_chooser_client_ =
        new FakeColorChooserClient(frame->GetDocument()->documentElement());
    date_time_chooser_client_ =
        new FakeDateTimeChooserClient(frame->GetDocument()->documentElement());
    select_ = HTMLSelectElement::Create(*(frame->GetDocument()));
  }

 protected:
  FrameTestHelpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  Persistent<WebLocalFrameImpl> main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
  Persistent<FakeColorChooserClient> color_chooser_client_;
  Persistent<FakeDateTimeChooserClient> date_time_chooser_client_;
  Persistent<HTMLSelectElement> select_;
};

TEST_F(PagePopupSuppressionTest, SuppressColorChooser) {
  // By default, the popup should be shown.
  EXPECT_TRUE(CanOpenColorChooser());

  Settings* settings = GetSettings();
  settings->SetPagePopupsSuppressed(true);

  EXPECT_FALSE(CanOpenColorChooser());

  settings->SetPagePopupsSuppressed(false);
  EXPECT_TRUE(CanOpenColorChooser());
}

TEST_F(PagePopupSuppressionTest, SuppressDateTimeChooser) {
  // By default, the popup should be shown.
  EXPECT_TRUE(CanOpenDateTimeChooser());

  Settings* settings = GetSettings();
  settings->SetPagePopupsSuppressed(true);

  EXPECT_FALSE(CanOpenDateTimeChooser());

  settings->SetPagePopupsSuppressed(false);
  EXPECT_TRUE(CanOpenDateTimeChooser());
}

TEST_F(PagePopupSuppressionTest, SuppressPopupMenu) {
  // By default, the popup should be shown.
  EXPECT_TRUE(CanOpenPopupMenu());

  Settings* settings = GetSettings();
  settings->SetPagePopupsSuppressed(true);

  EXPECT_FALSE(CanOpenPopupMenu());

  settings->SetPagePopupsSuppressed(false);
  EXPECT_TRUE(CanOpenPopupMenu());
}

}  // namespace blink
