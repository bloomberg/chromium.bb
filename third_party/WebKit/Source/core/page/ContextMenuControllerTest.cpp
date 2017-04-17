// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ContextMenuController.h"

#include "core/clipboard/DataTransfer.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/ContextMenu.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class ContextMenuControllerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  Document& GetDocument() const { return page_holder_->GetDocument(); }

  void SetBodyInnerHTML(const String& html_content) {
    GetDocument().body()->setInnerHTML(html_content);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(ContextMenuControllerTest, TestCustomMenu) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  // Load the the test page.
  SetBodyInnerHTML(
      "<button id=\"button_id\" contextmenu=\"menu_id\" style=\"height: 100px; "
      "width: 100px;\">"
      "<menu type=\"context\" id=\"menu_id\">"
      "<menuitem label=\"Item1\" onclick='document.title = \"Title 1\";'>"
      "</menuitem>"
      "<menuitem label=\"Item2\" onclick='document.title = \"Title 2\";'>"
      "</menuitem>"
      "<menuitem label=\"Item3\" onclick='document.title = \"Title 3\";'>"
      "</menuitem>"
      "<menu label='Submenu'>"
      "<menuitem label=\"Item4\" onclick='document.title = \"Title 4\";'>"
      "</menuitem>"
      "<menuitem label=\"Item5\" onclick='document.title = \"Title 5\";'>"
      "</menuitem>"
      "<menuitem label=\"Item6\" onclick='document.title = \"Title 6\";'>"
      "</menuitem>"
      "</menu>"
      "<menuitem id=\"item7\" type=\"checkbox\" checked label=\"Item7\""
      "onclick='if "
      "(document.getElementById(\"item7\").hasAttribute(\"checked\"))"
      "document.title = \"Title 7 checked\"; else document.title = \"Title 7 "
      "not checked\";'>"
      "</menuitem>"
      "<menuitem id=\"item8\" type=\"radio\" radiogroup=\"group\" "
      "label=\"Item8\""
      "onclick='if "
      "(document.getElementById(\"item8\").hasAttribute(\"checked\"))"
      "document.title = \"Title 8 checked\"; else if "
      "(document.getElementById(\"item9\").hasAttribute(\"checked\"))"
      "document.title = \"Title 8 not checked and Title 9 checked\";'>"
      "</menuitem>"
      "<menuitem id=\"item9\" type=\"radio\" radiogroup=\"group\" checked "
      "label=\"Item9\""
      "onclick='if "
      "(document.getElementById(\"item9\").hasAttribute(\"checked\"))"
      "document.title = \"Title 9 checked\"; else document.title = \"Title 9 "
      "not checked\";'>"
      "</menuitem>"
      "<menuitem id=\"item10\" type=\"radio\" radiogroup=\"group\" "
      "label=\"Item10\""
      "onclick='if "
      "(document.getElementById(\"item10\").hasAttribute(\"checked\"))"
      "document.title = \"Title 10 checked\"; else if "
      "(document.getElementById(\"item8\").hasAttribute(\"checked\"))"
      "document.title = \"Title 10 not checked and Title 8 checked\";'>"
      "</menuitem>"
      "</menu>"
      "</button>");

  MouseEventInit mouse_initializer;
  mouse_initializer.setView(GetDocument().domWindow());
  mouse_initializer.setScreenX(50);
  mouse_initializer.setScreenY(50);
  mouse_initializer.setButton(1);

  // Create right button click event and pass it to context menu controller.
  Event* event =
      MouseEvent::Create(nullptr, EventTypeNames::click, mouse_initializer);
  GetDocument().GetElementById("button_id")->focus();
  event->SetTarget(GetDocument().GetElementById("button_id"));
  GetDocument().GetPage()->GetContextMenuController().HandleContextMenuEvent(
      event);

  // Item 1
  // Item 2
  // Item 3
  // Submenu > Item 4
  //           Item 5
  //           Item 6
  // *Item 7
  // Item 8
  // *Item 9
  // Item 10
  const Vector<ContextMenuItem>& items = GetDocument()
                                             .GetPage()
                                             ->GetContextMenuController()
                                             .GetContextMenu()
                                             ->Items();
  EXPECT_EQ(8u, items.size());
  EXPECT_EQ(kActionType, items[0].GetType());
  EXPECT_STREQ("Item1", items[0].Title().Utf8().Data());
  GetDocument().GetPage()->GetContextMenuController().ContextMenuItemSelected(
      &items[0]);
  EXPECT_STREQ("Title 1", GetDocument().title().Utf8().Data());
  EXPECT_EQ(kSubmenuType, items[3].GetType());
  EXPECT_STREQ("Submenu", items[3].Title().Utf8().Data());
  const Vector<ContextMenuItem>& sub_menu_items = items[3].SubMenuItems();
  EXPECT_EQ(3u, sub_menu_items.size());
  EXPECT_STREQ("Item6", sub_menu_items[2].Title().Utf8().Data());
  GetDocument().GetPage()->GetContextMenuController().ContextMenuItemSelected(
      &sub_menu_items[2]);
  EXPECT_STREQ("Title 6", GetDocument().title().Utf8().Data());
  GetDocument().GetPage()->GetContextMenuController().ContextMenuItemSelected(
      &items[4]);
  EXPECT_STREQ("Title 7 checked", GetDocument().title().Utf8().Data());
  GetDocument().GetPage()->GetContextMenuController().ContextMenuItemSelected(
      &items[4]);
  EXPECT_STREQ("Title 7 not checked", GetDocument().title().Utf8().Data());
  GetDocument().GetPage()->GetContextMenuController().ContextMenuItemSelected(
      &items[5]);
  EXPECT_STREQ("Title 8 not checked and Title 9 checked",
               GetDocument().title().Utf8().Data());
  GetDocument().GetPage()->GetContextMenuController().ContextMenuItemSelected(
      &items[7]);
  EXPECT_STREQ("Title 10 not checked and Title 8 checked",
               GetDocument().title().Utf8().Data());
}

}  // namespace blink
