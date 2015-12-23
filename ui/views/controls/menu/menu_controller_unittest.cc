// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_delegate.h"
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

// Test implementation of MenuControllerDelegate that only reports the values
// called of OnMenuClosed.
class TestMenuControllerDelegate : public internal::MenuControllerDelegate {
 public:
  TestMenuControllerDelegate();
  ~TestMenuControllerDelegate() override {}

  int on_menu_closed_called() { return on_menu_closed_called_; }

  NotifyType on_menu_closed_notify_type() {
    return on_menu_closed_notify_type_;
  }

  MenuItemView* on_menu_closed_menu() { return on_menu_closed_menu_; }

  int on_menu_closed_mouse_event_flags() {
    return on_menu_closed_mouse_event_flags_;
  }

  // internal::MenuControllerDelegate:
  void OnMenuClosed(NotifyType type,
                    MenuItemView* menu,
                    int mouse_event_flags) override;
  void SiblingMenuCreated(MenuItemView* menu) override;

 private:
  // Number of times OnMenuClosed has been called.
  int on_menu_closed_called_;

  // The values passed on the last call of OnMenuClosed.
  NotifyType on_menu_closed_notify_type_;
  MenuItemView* on_menu_closed_menu_;
  int on_menu_closed_mouse_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuControllerDelegate);
};

TestMenuControllerDelegate::TestMenuControllerDelegate()
    : on_menu_closed_called_(0),
      on_menu_closed_notify_type_(NOTIFY_DELEGATE),
      on_menu_closed_menu_(nullptr),
      on_menu_closed_mouse_event_flags_(0) {}

void TestMenuControllerDelegate::OnMenuClosed(NotifyType type,
                                              MenuItemView* menu,
                                              int mouse_event_flags) {
  on_menu_closed_called_++;
  on_menu_closed_notify_type_ = type;
  on_menu_closed_menu_ = menu;
  on_menu_closed_mouse_event_flags_ = mouse_event_flags;
}

void TestMenuControllerDelegate::SiblingMenuCreated(MenuItemView* menu) {}

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
  TestMenuItemViewShown(MenuDelegate* delegate) : MenuItemView(delegate) {
    submenu_ = new SubmenuViewShown(this);
  }
  ~TestMenuItemViewShown() override {}

  void SetController(MenuController* controller) {
    set_controller(controller);
  }

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

  internal::MenuControllerDelegate* GetCurrentDelegate() {
    return menu_controller_->delegate_;
  }

  bool IsAsyncRun() { return menu_controller_->async_run_; }

  void SelectByChar(base::char16 character) {
    menu_controller_->SelectByChar(character);
  }

  void SetIsCombobox(bool is_combobox) {
    menu_controller_->set_is_combobox(is_combobox);
  }

  void RunMenu() {
    menu_controller_->RunMessageLoop(false);
  }

  void Accept(MenuItemView* item, int event_flags) {
    menu_controller_->Accept(item, event_flags);
  }

  Widget* owner() { return owner_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TestMenuItemViewShown* menu_item() { return menu_item_.get(); }
  TestMenuControllerDelegate* menu_controller_delegate() {
    return menu_controller_delegate_.get();
  }
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
    menu_delegate_.reset(new MenuDelegate);
    menu_item_.reset(new TestMenuItemViewShown(menu_delegate_.get()));
    menu_item_->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
    menu_item_->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
    menu_item_->AppendMenuItemWithLabel(3, base::ASCIIToUTF16("Three"));
    menu_item_->AppendMenuItemWithLabel(4, base::ASCIIToUTF16("Four"));
  }

  void SetupMenuController() {
    menu_controller_delegate_.reset(new TestMenuControllerDelegate);
    menu_controller_ =
        new MenuController(true, menu_controller_delegate_.get());
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
    menu_controller_->SetSelection(
        menu_item_.get(), MenuController::SELECTION_UPDATE_IMMEDIATELY);
    menu_item_->SetController(menu_controller_);
  }

  scoped_ptr<Widget> owner_;
  scoped_ptr<ui::test::EventGenerator> event_generator_;
  scoped_ptr<TestMenuItemViewShown> menu_item_;
  scoped_ptr<TestMenuControllerDelegate> menu_controller_delegate_;
  scoped_ptr<MenuDelegate> menu_delegate_;
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

// Tests that opening menu and calling SelectByChar works correctly.
TEST_F(MenuControllerTest, SelectByChar) {
  SetIsCombobox(true);

  // Handle null character should do nothing.
  SelectByChar(0);
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when Accept is called.
TEST_F(MenuControllerTest, AsynchronousAccept) {
  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);

  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  const int kEventFlags = 42;
  Accept(accepted, kEventFlags);

  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(accepted, delegate->on_menu_closed_menu());
  EXPECT_EQ(kEventFlags, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when CancelAll is called.
TEST_F(MenuControllerTest, AsynchronousCancelAll) {
  MenuController* controller = menu_controller();
  controller->SetAsyncRun(true);

  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  controller->CancelAll();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

// Tests that an asynchrnous menu nested within a synchronous menu restores the
// previous MenuControllerDelegate and synchronous settings.
TEST_F(MenuControllerTest, AsynchronousNestedDelegate) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  scoped_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  ASSERT_FALSE(IsAsyncRun());
  controller->AddNestedDelegate(nested_delegate.get());
  controller->SetAsyncRun(true);

  EXPECT_TRUE(IsAsyncRun());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  int mouse_event_flags = 0;
  MenuItemView* run_result =
      controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                      MENU_ANCHOR_TOPLEFT, false, false, &mouse_event_flags);
  EXPECT_EQ(run_result, nullptr);

  controller->CancelAll();
  EXPECT_FALSE(IsAsyncRun());
  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(0, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::EXIT_ALL, controller->exit_type());
}

}  // namespace test
}  // namespace views
