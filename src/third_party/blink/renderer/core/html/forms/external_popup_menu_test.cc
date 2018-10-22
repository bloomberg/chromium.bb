// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/external_popup_menu.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_external_popup_menu.h"
#include "third_party/blink/public/web/web_popup_menu_info.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_menu_list.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class ExternalPopupMenuDisplayNoneItemsTest : public PageTestBase {
 public:
  ExternalPopupMenuDisplayNoneItemsTest() = default;

 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    HTMLSelectElement* element = HTMLSelectElement::Create(GetDocument());
    // Set the 4th an 5th items to have "display: none" property
    element->SetInnerHTMLFromString(
        "<option><option><option><option style='display:none;'><option "
        "style='display:none;'><option><option>");
    GetDocument().body()->AppendChild(element, ASSERT_NO_EXCEPTION);
    owner_element_ = element;
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  }

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

class ExternalPopupMenuTest : public testing::Test {
 public:
  ExternalPopupMenuTest() : base_url_("http://www.test.com") {}

 protected:
  void SetUp() override {
    helper_.Initialize(&web_frame_client_);
    WebView()->SetUseExternalPopupMenus(true);
  }
  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedURLLoad(const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath("popup"),
        WebString::FromUTF8(file_name), WebString::FromUTF8("text/html"));
  }

  void LoadFrame(const std::string& file_name) {
    FrameTestHelpers::LoadFrame(MainFrame(), base_url_ + file_name);
    WebView()->Resize(WebSize(800, 600));
    WebView()->UpdateAllLifecyclePhases();
  }

  WebViewImpl* WebView() const { return helper_.GetWebView(); }
  const ExternalPopupMenuWebFrameClient& Client() const {
    return web_frame_client_;
  }
  WebLocalFrameImpl* MainFrame() const { return helper_.LocalMainFrame(); }

 private:
  std::string base_url_;
  ExternalPopupMenuWebFrameClient web_frame_client_;
  FrameTestHelpers::WebViewHelper helper_;
};

TEST_F(ExternalPopupMenuTest, PopupAccountsForVisualViewportTransform) {
  RegisterMockedURLLoad("select_mid_screen.html");
  LoadFrame("select_mid_screen.html");

  WebView()->Resize(WebSize(100, 100));
  WebView()->UpdateAllLifecyclePhases();

  HTMLSelectElement* select = ToHTMLSelectElement(
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

  HTMLSelectElement* select = ToHTMLSelectElement(
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

  HTMLSelectElement* select = ToHTMLSelectElement(
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

  HTMLSelectElement* select = ToHTMLSelectElement(
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
