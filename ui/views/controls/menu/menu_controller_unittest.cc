// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#endif

#if defined(USE_X11)
#include <X11/Xlib.h>
#undef Bool
#undef None
#include "ui/events/test/events_test_utils_x11.h"
#endif

namespace views {
namespace test {

namespace {

class SubmenuViewShown : public SubmenuView {
 public:
  SubmenuViewShown(MenuItemView* parent) : SubmenuView(parent) {}
  ~SubmenuViewShown() override {}
  bool IsShowing() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubmenuViewShown);
};

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() : outstanding_touches_(0) {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    switch(event->type()) {
      case ui::ET_TOUCH_PRESSED:
        outstanding_touches_++;
        break;
      case ui::ET_TOUCH_RELEASED:
      case ui::ET_TOUCH_CANCELLED:
        outstanding_touches_--;
        break;
      default:
        break;
    }
  }

  int outstanding_touches() const { return outstanding_touches_; }

 private:
  int outstanding_touches_;
  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

}  // namespace

class TestMenuItemViewShown : public MenuItemView {
 public:
  TestMenuItemViewShown() : MenuItemView(nullptr) {
    submenu_ = new SubmenuViewShown(this);
  }
  ~TestMenuItemViewShown() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemViewShown);
};

class MenuControllerTest : public ViewsTestBase {
 public:
  MenuControllerTest() : menu_controller_(nullptr) {
  }

  ~MenuControllerTest() override {}

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    Init();
  }

  void TearDown() override {
    owner_->CloseNow();

    menu_controller_->showing_ = false;
    menu_controller_->owner_ = nullptr;
    delete menu_controller_;
    menu_controller_ = nullptr;

    ViewsTestBase::TearDown();
  }

  void ReleaseTouchId(int id) {
    event_generator_->ReleaseTouchId(id);
  }

  void PressKey(ui::KeyboardCode key_code) {
    event_generator_->PressKey(key_code, 0);
  }

#if defined(OS_LINUX) && defined(USE_X11)
  void TestEventTargeter() {
    {
      // With the |ui::NullEventTargeter| instantiated and assigned we expect
      // the menu to not handle the key event.
      aura::ScopedWindowTargeter scoped_targeter(
          owner()->GetNativeWindow()->GetRootWindow(),
          scoped_ptr<ui::EventTargeter>(new ui::NullEventTargeter));
      event_generator_->PressKey(ui::VKEY_ESCAPE, 0);
      EXPECT_EQ(MenuController::EXIT_NONE, menu_exit_type());
    }
    // Now that the targeter has been destroyed, expect to exit the menu
    // normally when hitting escape.
    event_generator_->PressKey(ui::VKEY_ESCAPE, 0);
    EXPECT_EQ(MenuController::EXIT_OUTERMOST, menu_exit_type());
  }
#endif  // defined(OS_LINUX) && defined(USE_X11)

 protected:
  void SetPendingStateItem(MenuItemView* item) {
    menu_controller_->pending_state_.item = item;
    menu_controller_->pending_state_.submenu_open = true;
  }

  void ResetSelection() {
    menu_controller_->SetSelection(
        nullptr,
        MenuController::SELECTION_EXIT |
        MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  void IncrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_DOWN);
  }

  void DecrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_UP);
  }

  MenuItemView* FindInitialSelectableMenuItemDown(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindInitialSelectableMenuItemUp(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_UP);
  }

  MenuItemView* FindNextSelectableMenuItem(MenuItemView* parent,
                                           int index) {

    return menu_controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindPreviousSelectableMenuItem(MenuItemView* parent,
                                               int index) {
    return menu_controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_UP);
  }

  void RunMenu() {
    menu_controller_->RunMessageLoop(false);
  }

  Widget* owner() { return owner_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TestMenuItemViewShown* menu_item() { return menu_item_.get(); }
  MenuController* menu_controller() { return menu_controller_; }
  const MenuItemView* pending_state_item() const {
      return menu_controller_->pending_state_.item;
  }
  MenuController::ExitType menu_exit_type() const {
    return menu_controller_->exit_type_;
  }

 private:
  void Init() {
    owner_.reset(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    owner_->Init(params);
    event_generator_.reset(
        new ui::test::EventGenerator(GetContext(), owner_->GetNativeWindow()));
    owner_->Show();

    SetupMenuItem();

    SetupMenuController();
  }

  void SetupMenuItem() {
    menu_item_.reset(new TestMenuItemViewShown);
    menu_item_->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
    menu_item_->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
    menu_item_->AppendMenuItemWithLabel(3, base::ASCIIToUTF16("Three"));
    menu_item_->AppendMenuItemWithLabel(4, base::ASCIIToUTF16("Four"));
  }

  void SetupMenuController() {
    menu_controller_= new MenuController(nullptr, true, nullptr);
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
    menu_controller_->SetSelection(
        menu_item_.get(), MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  scoped_ptr<Widget> owner_;
  scoped_ptr<ui::test::EventGenerator> event_generator_;
  scoped_ptr<TestMenuItemViewShown> menu_item_;
  MenuController* menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTest);
};

#if defined(OS_LINUX) && defined(USE_X11)
// Tests that an event targeter which blocks events will be honored by the menu
// event dispatcher.
TEST_F(MenuControllerTest, EventTargeter) {
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::TestEventTargeter,
                 base::Unretained(this)));
  RunMenu();
}
#endif  // defined(OS_LINUX) && defined(USE_X11)

#if defined(USE_X11)
// Tests that touch event ids are released correctly. See crbug.com/439051 for
// details. When the ids aren't managed correctly, we get stuck down touches.
TEST_F(MenuControllerTest, TouchIdsReleasedCorrectly) {
  TestEventHandler test_event_handler;
  owner()->GetNativeWindow()->GetRootWindow()->AddPreTargetHandler(
      &test_event_handler);

  std::vector<int> devices;
  devices.push_back(1);
  ui::SetUpTouchDevicesForTest(devices);

  event_generator()->PressTouchId(0);
  event_generator()->PressTouchId(1);
  event_generator()->ReleaseTouchId(0);

  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::ReleaseTouchId,
                 base::Unretained(this),
                 1));

  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::PressKey,
                 base::Unretained(this),
                 ui::VKEY_ESCAPE));

  RunMenu();

  EXPECT_EQ(MenuController::EXIT_OUTERMOST, menu_exit_type());
  EXPECT_EQ(0, test_event_handler.outstanding_touches());

  owner()->GetNativeWindow()->GetRootWindow()->RemovePreTargetHandler(
      &test_event_handler);
}
#endif  // defined(USE_X11)

// Tests that initial selected menu items are correct when items are enabled or
// disabled.
TEST_F(MenuControllerTest, InitialSelectedItem) {
  // Leave items "Two", "Three", and "Four" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  // The first selectable item should be item "Two".
  MenuItemView* first_selectable =
      FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  // The last selectable item should be item "Four".
  MenuItemView* last_selectable =
      FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(4, last_selectable->GetCommand());

  // Leave items "One" and "Two" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "Two".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // Leave only a single item "One" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "One".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(1, last_selectable->GetCommand());

  // Leave only a single item "Three" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "Three".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(3, first_selectable->GetCommand());
  // The last selectable item should be item "Three".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(3, last_selectable->GetCommand());

  // Leave only a single item ("Two") selected. It should be the first and the
  // last selectable item.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // There should be no next or previous selectable item since there is only a
  // single enabled item in the menu.
  EXPECT_EQ(nullptr, FindNextSelectableMenuItem(menu_item(), 1));
  EXPECT_EQ(nullptr, FindPreviousSelectableMenuItem(menu_item(), 1));

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Down' and 'Up' iterates over enabled
// items.
TEST_F(MenuControllerTest, NextSelectedItem) {
  // Disabling the item "Three" gets it skipped when using keyboard to navigate.
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);

  // Fake initial hot selection.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Move down in the menu.
  // Select next item.
  IncrementSelection();
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  // Skip disabled item.
  IncrementSelection();
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Wrap around.
  IncrementSelection();
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Move up in the menu.
  // Wrap around.
  DecrementSelection();
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Skip disabled item.
  DecrementSelection();
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  // Select previous item.
  DecrementSelection();
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Up' selects the last enabled menu item.
TEST_F(MenuControllerTest, PreviousSelectedItem) {
  // Disabling the item "Four" gets it skipped when using keyboard to navigate.
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);

  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Move up and select a previous (in our case the last enabled) item.
  DecrementSelection();
  EXPECT_EQ(3, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

}  // namespace test
}  // namespace views
