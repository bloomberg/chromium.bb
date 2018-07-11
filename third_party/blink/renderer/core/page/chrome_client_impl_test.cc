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

#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_selection.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/testing/stub_graphics_layer_client.h"

namespace blink {

namespace {

class TestWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  explicit TestWebViewClient(WebNavigationPolicy* target) : target_(target) {}
  ~TestWebViewClient() override = default;

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

class CreateWindowTest : public testing::Test {
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

TEST_F(CreateWindowTest, CreateWindowFromPausedPage) {
  ScopedPagePauser pauser;
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
  ~FakeColorChooserClient() override = default;

  void Trace(blink::Visitor* visitor) override {
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
  Vector<mojom::blink::ColorSuggestionPtr> Suggestions() const override {
    return Vector<mojom::blink::ColorSuggestionPtr>();
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
  ~FakeDateTimeChooserClient() override = default;

  void Trace(blink::Visitor* visitor) override {
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

// TODO(crbug.com/779126): A number of popups are not supported in immersive
// mode. The PagePopupSuppressionTests ensure that these unsupported popups
// do not appear in immersive mode.
class PagePopupSuppressionTest : public testing::Test {
 public:
  PagePopupSuppressionTest() = default;

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
  settings->SetImmersiveModeEnabled(true);

  EXPECT_FALSE(CanOpenColorChooser());

  settings->SetImmersiveModeEnabled(false);
  EXPECT_TRUE(CanOpenColorChooser());
}

TEST_F(PagePopupSuppressionTest, SuppressDateTimeChooser) {
  // By default, the popup should be shown.
  EXPECT_TRUE(CanOpenDateTimeChooser());

  Settings* settings = GetSettings();
  settings->SetImmersiveModeEnabled(true);

  EXPECT_FALSE(CanOpenDateTimeChooser());

  settings->SetImmersiveModeEnabled(false);
  EXPECT_TRUE(CanOpenDateTimeChooser());
}

TEST(ChromeClientImplTest, SelectionHandles) {
  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* web_view = helper.Initialize();
  ChromeClientImpl* chrome_client_impl =
      ToChromeClientImpl(&web_view->GetPage()->GetChromeClient());

  content::LayerTreeView* layer_tree_view = helper.GetLayerTreeView();
  LocalFrame* main_frame = helper.LocalMainFrame()->GetFrame();

  auto set_and_get_selection = [&](const CompositedSelection& selection) {
    // Sets the selection on the LayerTreeHost, then return what it set.
    chrome_client_impl->UpdateCompositedSelection(main_frame, selection);
    return layer_tree_view->layer_tree_host()->selection();
  };

  StubGraphicsLayerClient graphics_layer_client;
  std::unique_ptr<GraphicsLayer> graphics_layer =
      GraphicsLayer::Create(graphics_layer_client);

  CompositedSelection selection;
  cc::LayerSelection cc_selection;

  selection.start.layer = graphics_layer.get();
  selection.end.layer = graphics_layer.get();

  // An LTR range selection.
  selection.type = kRangeSelection;
  selection.start.is_text_direction_rtl = false;
  selection.end.is_text_direction_rtl = false;
  selection.start.edge_top_in_layer = FloatPoint(1.1f, 2.8f);
  selection.start.edge_bottom_in_layer = FloatPoint(2.9f, 3.2f);
  selection.end.edge_top_in_layer = FloatPoint(4.1f, 5.8f);
  selection.end.edge_bottom_in_layer = FloatPoint(5.9f, 6.2f);
  selection.start.hidden = false;
  selection.end.hidden = false;

  cc_selection = set_and_get_selection(selection);
  // LTR means the start is left and the end is right.
  EXPECT_EQ(cc_selection.start.type, gfx::SelectionBound::Type::LEFT);
  EXPECT_EQ(cc_selection.end.type, gfx::SelectionBound::Type::RIGHT);
  EXPECT_EQ(cc_selection.start.hidden, false);
  EXPECT_EQ(cc_selection.end.hidden, false);
  // Points are rounded.
  EXPECT_EQ(cc_selection.start.edge_top, gfx::Point(1, 3));
  EXPECT_EQ(cc_selection.start.edge_bottom, gfx::Point(3, 3));
  EXPECT_EQ(cc_selection.end.edge_top, gfx::Point(4, 6));
  EXPECT_EQ(cc_selection.end.edge_bottom, gfx::Point(6, 6));

  // An RTL range selection.
  selection.start.is_text_direction_rtl = true;
  selection.end.is_text_direction_rtl = true;

  cc_selection = set_and_get_selection(selection);
  // RTL means the start is right and the end is left.
  EXPECT_EQ(cc_selection.start.type, gfx::SelectionBound::Type::RIGHT);
  EXPECT_EQ(cc_selection.end.type, gfx::SelectionBound::Type::LEFT);
  EXPECT_EQ(cc_selection.start.hidden, false);
  EXPECT_EQ(cc_selection.end.hidden, false);
  EXPECT_EQ(cc_selection.start.edge_top, gfx::Point(1, 3));
  EXPECT_EQ(cc_selection.start.edge_bottom, gfx::Point(3, 3));
  EXPECT_EQ(cc_selection.end.edge_top, gfx::Point(4, 6));
  EXPECT_EQ(cc_selection.end.edge_bottom, gfx::Point(6, 6));

  // A hidden range selection.
  selection.start.hidden = true;
  selection.end.hidden = true;

  cc_selection = set_and_get_selection(selection);
  EXPECT_EQ(cc_selection.start.type, gfx::SelectionBound::Type::RIGHT);
  EXPECT_EQ(cc_selection.end.type, gfx::SelectionBound::Type::LEFT);
  // The hidden flag is propogated.
  EXPECT_EQ(cc_selection.start.hidden, true);
  EXPECT_EQ(cc_selection.end.hidden, true);
  EXPECT_EQ(cc_selection.start.edge_top, gfx::Point(1, 3));
  EXPECT_EQ(cc_selection.start.edge_bottom, gfx::Point(3, 3));
  EXPECT_EQ(cc_selection.end.edge_top, gfx::Point(4, 6));
  EXPECT_EQ(cc_selection.end.edge_bottom, gfx::Point(6, 6));

  // A caret selection.
  selection.type = kCaretSelection;

  cc_selection = set_and_get_selection(selection);
  // Caret selections have no left/right, only center.
  EXPECT_EQ(cc_selection.start.type, gfx::SelectionBound::Type::CENTER);
  EXPECT_EQ(cc_selection.end.type, gfx::SelectionBound::Type::CENTER);
  EXPECT_EQ(cc_selection.start.hidden, true);
  EXPECT_EQ(cc_selection.end.hidden, true);
  EXPECT_EQ(cc_selection.start.edge_top, gfx::Point(1, 3));
  EXPECT_EQ(cc_selection.start.edge_bottom, gfx::Point(3, 3));
  EXPECT_EQ(cc_selection.end.edge_top, gfx::Point(4, 6));
  EXPECT_EQ(cc_selection.end.edge_bottom, gfx::Point(6, 6));
}

}  // namespace blink
