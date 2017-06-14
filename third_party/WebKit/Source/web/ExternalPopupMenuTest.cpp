// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ExternalPopupMenu.h"

#include <memory>
#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLSelectElement.h"
#include "core/layout/LayoutMenuList.h"
#include "core/page/Page.h"
#include "core/page/PopupMenu.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebExternalPopupMenu.h"
#include "public/web/WebPopupMenuInfo.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ExternalPopupMenuDisplayNoneItemsTest : public ::testing::Test {
 public:
  ExternalPopupMenuDisplayNoneItemsTest() {}

 protected:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    HTMLSelectElement* element =
        HTMLSelectElement::Create(dummy_page_holder_->GetDocument());
    // Set the 4th an 5th items to have "display: none" property
    element->setInnerHTML(
        "<option><option><option><option style='display:none;'><option "
        "style='display:none;'><option><option>");
    dummy_page_holder_->GetDocument().body()->AppendChild(element,
                                                          ASSERT_NO_EXCEPTION);
    owner_element_ = element;
    dummy_page_holder_->GetDocument()
        .UpdateStyleAndLayoutIgnorePendingStylesheets();
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<HTMLSelectElement> owner_element_;
};

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, PopupMenuInfoSizeTest) {
  WebPopupMenuInfo info;
  ExternalPopupMenu::GetPopupMenuInfo(info, *owner_element_);
  EXPECT_EQ(5U, info.items.size());
}

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, IndexMappingTest) {
  // 6th indexed item in popupmenu would be the 4th item in ExternalPopupMenu,
  // and vice-versa.
  EXPECT_EQ(
      4, ExternalPopupMenu::ToExternalPopupMenuItemIndex(6, *owner_element_));
  EXPECT_EQ(6, ExternalPopupMenu::ToPopupMenuItemIndex(4, *owner_element_));

  // Invalid index, methods should return -1.
  EXPECT_EQ(
      -1, ExternalPopupMenu::ToExternalPopupMenuItemIndex(8, *owner_element_));
  EXPECT_EQ(-1, ExternalPopupMenu::ToPopupMenuItemIndex(8, *owner_element_));
}

class ExternalPopupMenuWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  WebExternalPopupMenu* CreateExternalPopupMenu(
      const WebPopupMenuInfo&,
      WebExternalPopupMenuClient*) override {
    return &mock_web_external_popup_menu_;
  }
  WebRect ShownBounds() const {
    return mock_web_external_popup_menu_.ShownBounds();
  }

 private:
  class MockWebExternalPopupMenu : public WebExternalPopupMenu {
    void Show(const WebRect& bounds) override { shown_bounds_ = bounds; }
    void Close() override {}

   public:
    WebRect ShownBounds() const { return shown_bounds_; }

   private:
    WebRect shown_bounds_;
  };
  WebRect shown_bounds_;
  MockWebExternalPopupMenu mock_web_external_popup_menu_;
};

class ExternalPopupMenuTest : public ::testing::Test {
 public:
  ExternalPopupMenuTest() : base_url_("http://www.test.com") {}

 protected:
  void SetUp() override {
    helper_.Initialize(false, &web_frame_client_, &web_view_client_);
    WebView()->SetUseExternalPopupMenus(true);
  }
  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedURLLoad(const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::WebTestDataPath("popup"),
        WebString::FromUTF8(file_name), WebString::FromUTF8("text/html"));
  }

  void LoadFrame(const std::string& file_name) {
    FrameTestHelpers::LoadFrame(MainFrame(), base_url_ + file_name);
    WebView()->Resize(WebSize(800, 600));
    WebView()->UpdateAllLifecyclePhases();
  }

  WebViewBase* WebView() const { return helper_.WebView(); }
  const ExternalPopupMenuWebFrameClient& Client() const {
    return web_frame_client_;
  }
  WebLocalFrameBase* MainFrame() const {
    return helper_.WebView()->MainFrameImpl();
  }

 private:
  std::string base_url_;
  FrameTestHelpers::TestWebViewClient web_view_client_;
  ExternalPopupMenuWebFrameClient web_frame_client_;
  FrameTestHelpers::WebViewHelper helper_;
};

TEST_F(ExternalPopupMenuTest, PopupAccountsForVisualViewportTransform) {
  RegisterMockedURLLoad("select_mid_screen.html");
  LoadFrame("select_mid_screen.html");

  WebView()->Resize(WebSize(100, 100));
  WebView()->UpdateAllLifecyclePhases();

  HTMLSelectElement* select = toHTMLSelectElement(
      MainFrame()->GetFrame()->GetDocument()->getElementById("select"));
  LayoutMenuList* menu_list = ToLayoutMenuList(select->GetLayoutObject());
  ASSERT_TRUE(menu_list);

  VisualViewport& visual_viewport = WebView()->GetPage()->GetVisualViewport();

  IntRect rect_in_document = menu_list->AbsoluteBoundingBoxRect();

  constexpr int kScaleFactor = 2;
  ScrollOffset scroll_delta(20, 30);

  const int expected_x =
      (rect_in_document.X() - scroll_delta.Width()) * kScaleFactor;
  const int expected_y =
      (rect_in_document.Y() - scroll_delta.Height()) * kScaleFactor;

  WebView()->SetPageScaleFactor(kScaleFactor);
  visual_viewport.Move(scroll_delta);
  select->ShowPopup();

  EXPECT_EQ(expected_x, Client().ShownBounds().x);
  EXPECT_EQ(expected_y, Client().ShownBounds().y);
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndex) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  HTMLSelectElement* select = toHTMLSelectElement(
      MainFrame()->GetFrame()->GetDocument()->getElementById("select"));
  LayoutMenuList* menu_list = ToLayoutMenuList(select->GetLayoutObject());
  ASSERT_TRUE(menu_list);

  select->ShowPopup();
  ASSERT_TRUE(select->PopupIsVisible());

  WebExternalPopupMenuClient* client =
      static_cast<ExternalPopupMenu*>(select->Popup());
  client->DidAcceptIndex(2);
  EXPECT_FALSE(select->PopupIsVisible());
  ASSERT_STREQ("2", menu_list->GetText().Utf8().data());
  EXPECT_EQ(2, select->selectedIndex());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndices) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  HTMLSelectElement* select = toHTMLSelectElement(
      MainFrame()->GetFrame()->GetDocument()->getElementById("select"));
  LayoutMenuList* menu_list = ToLayoutMenuList(select->GetLayoutObject());
  ASSERT_TRUE(menu_list);

  select->ShowPopup();
  ASSERT_TRUE(select->PopupIsVisible());

  WebExternalPopupMenuClient* client =
      static_cast<ExternalPopupMenu*>(select->Popup());
  int indices[] = {2};
  WebVector<int> indices_vector(indices, 1);
  client->DidAcceptIndices(indices_vector);
  EXPECT_FALSE(select->PopupIsVisible());
  EXPECT_STREQ("2", menu_list->GetText().Utf8().data());
  EXPECT_EQ(2, select->selectedIndex());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndicesClearSelect) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  HTMLSelectElement* select = toHTMLSelectElement(
      MainFrame()->GetFrame()->GetDocument()->getElementById("select"));
  LayoutMenuList* menu_list = ToLayoutMenuList(select->GetLayoutObject());
  ASSERT_TRUE(menu_list);

  select->ShowPopup();
  ASSERT_TRUE(select->PopupIsVisible());

  WebExternalPopupMenuClient* client =
      static_cast<ExternalPopupMenu*>(select->Popup());
  WebVector<int> indices;
  client->DidAcceptIndices(indices);
  EXPECT_FALSE(select->PopupIsVisible());
  EXPECT_EQ(-1, select->selectedIndex());
}

}  // namespace blink
