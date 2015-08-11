// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event_handler.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/test/platform_event_source_test_api.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/wm/public/dispatcher_client.h"
#endif

#if defined(OS_WIN)
#include "base/message_loop/message_pump_dispatcher.h"
#elif defined(USE_X11)
#include <X11/Xlib.h>
#undef Bool
#undef None
#include "ui/events/test/events_test_utils_x11.h"
#elif defined(USE_OZONE)
#include "ui/events/event.h"
#elif defined(OS_MACOSX)
#include "ui/events/test/event_generator.h"
#endif

namespace views {
namespace test {

namespace {

class TestMenuItemView : public MenuItemView {
 public:
  TestMenuItemView() : MenuItemView(nullptr) {}
  ~TestMenuItemView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemView);
};

class SubmenuViewShown : public SubmenuView {
 public:
  SubmenuViewShown(MenuItemView* parent) : SubmenuView(parent) {}
  ~SubmenuViewShown() override {}
  bool IsShowing() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubmenuViewShown);
};

#if defined(USE_AURA)
class TestDispatcherClient : public aura::client::DispatcherClient {
 public:
  TestDispatcherClient() : dispatcher_(nullptr) {}
  ~TestDispatcherClient() override {}

  base::MessagePumpDispatcher* dispatcher() {
    return dispatcher_;
  }

  // aura::client::DispatcherClient:
  void PrepareNestedLoopClosures(base::MessagePumpDispatcher* dispatcher,
                                 base::Closure* run_closure,
                                 base::Closure* quit_closure) override {
    scoped_ptr<base::RunLoop> run_loop(new base::RunLoop());
    *quit_closure = run_loop->QuitClosure();
    *run_closure = base::Bind(&TestDispatcherClient::RunNestedDispatcher,
                              base::Unretained(this),
                              base::Passed(&run_loop),
                              dispatcher);
  }

 private:
  void RunNestedDispatcher(scoped_ptr<base::RunLoop> run_loop,
                           base::MessagePumpDispatcher* dispatcher) {
    base::AutoReset<base::MessagePumpDispatcher*> reset_dispatcher(&dispatcher_,
                                                                   dispatcher);
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    run_loop->Run();
  }

  base::MessagePumpDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(TestDispatcherClient);
};
#endif  // USE_AURA

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
  MenuControllerTest()
      : controller_(nullptr),
        event_source_(ui::PlatformEventSource::GetInstance()) {}
  ~MenuControllerTest() override { ResetMenuController(); }

  // Dispatches |count| number of items, each in a separate iteration of the
  // message-loop, by posting a task.
  void Step3_DispatchEvents(int count) {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    controller_->exit_type_ = MenuController::EXIT_ALL;

    DispatchEvent();
    if (count) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MenuControllerTest::Step3_DispatchEvents,
                     base::Unretained(this),
                     count - 1));
    } else {
      EXPECT_TRUE(run_loop_->running());
      run_loop_->Quit();
    }
  }

  // Runs a nested message-loop that does not involve the menu itself.
  void Step2_RunNestedLoop() {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::Step3_DispatchEvents,
                   base::Unretained(this),
                   3));
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void Step1_RunMenu() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::Step2_RunNestedLoop,
                   base::Unretained(this)));
    scoped_ptr<Widget> owner(CreateOwnerWidget());
    RunMenu(owner.get());
  }

  scoped_ptr<Widget> CreateOwnerWidget() {
    scoped_ptr<Widget> widget(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(params);
    widget->Show();

#if defined(USE_AURA)
    aura::client::SetDispatcherClient(
        widget->GetNativeWindow()->GetRootWindow(), &dispatcher_client_);
#endif
    return widget.Pass();
  }

  const MenuItemView* pending_state_item() const {
    return controller_->pending_state_.item;
  }

  void SetPendingStateItem(MenuItemView* item) {
    controller_->pending_state_.item = item;
    controller_->pending_state_.submenu_open = true;
  }

  void ResetSelection() {
    controller_->SetSelection(nullptr,
                              MenuController::SELECTION_EXIT |
                              MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  void IncrementSelection() {
    controller_->IncrementSelection(MenuController::INCREMENT_SELECTION_DOWN);
  }

  void DecrementSelection() {
    controller_->IncrementSelection(MenuController::INCREMENT_SELECTION_UP);
  }

  MenuItemView* FindInitialSelectableMenuItemDown(MenuItemView* parent) {
    return controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindInitialSelectableMenuItemUp(MenuItemView* parent) {
    return controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_UP);
  }

  MenuItemView* FindNextSelectableMenuItem(MenuItemView* parent,
                                           int index) {
    return controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindPreviousSelectableMenuItem(MenuItemView* parent,
                                               int index) {
    return controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_UP);
  }

  void SetupMenu(views::Widget* owner, views::MenuItemView* item) {
    ResetMenuController();
    controller_ = new MenuController(nullptr, true, nullptr);
    controller_->owner_ = owner;
    controller_->showing_ = true;
    controller_->SetSelection(item,
                              MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  void RunMenu(views::Widget* owner) {
    scoped_ptr<TestMenuItemView> menu_item(new TestMenuItemView);
    SetupMenu(owner, menu_item.get());
    controller_->RunMessageLoop(false);
  }

#if defined(USE_X11)
  void DispatchEscapeAndExpect(MenuController::ExitType exit_type) {
    ui::ScopedXI2Event key_event;
    key_event.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
    event_source_.Dispatch(key_event);
    EXPECT_EQ(exit_type, controller_->exit_type());
    controller_->exit_type_ = MenuController::EXIT_ALL;
    DispatchEvent();
  }

  void DispatchTouch(int evtype, int id) {
    ui::ScopedXI2Event touch_event;
    std::vector<ui::Valuator> valuators;
    touch_event.InitTouchEvent(1, evtype, id, gfx::Point(10, 10), valuators);
    event_source_.Dispatch(touch_event);
    DispatchEvent();
  }
#endif

  void DispatchEvent() {
#if defined(USE_X11)
    XEvent xevent;
    memset(&xevent, 0, sizeof(xevent));
    event_source_.Dispatch(&xevent);
#elif defined(OS_WIN)
    MSG msg;
    memset(&msg, 0, sizeof(MSG));
    dispatcher_client_.dispatcher()->Dispatch(msg);
#elif defined(USE_OZONE)
    ui::KeyEvent event(' ', ui::VKEY_SPACE, ui::EF_NONE);
    event_source_.Dispatch(&event);
#elif defined(OS_MACOSX) && !defined(USE_AURA)
    // Since this is not an interactive test, on Mac there will be no key
    // window. Any system event will just get ignored, so use the EventGenerator
    // to generate a dummy event. Without Aura, these will be native events.
    gfx::NativeWindow window = controller_->owner()->GetNativeWindow();
    ui::test::EventGenerator generator(window, window);
    // Send "up", since this will not activate a menu item. But note that the
    // test has already set exit_type_ = EXIT_ALL just before the first call
    // to this function.
    generator.PressKey(ui::VKEY_UP, 0);
#else
#error Unsupported platform
#endif
  }

 private:
  void ResetMenuController() {
    if (controller_) {
      // These properties are faked by RunMenu for the purposes of testing and
      // need to be undone before we call the destructor.
      controller_->owner_ = nullptr;
      controller_->showing_ = false;
      delete controller_;
      controller_ = nullptr;
    }
  }

  // A weak pointer to the MenuController owned by this class.
  MenuController* controller_;
  scoped_ptr<base::RunLoop> run_loop_;
  ui::test::PlatformEventSourceTestAPI event_source_;
#if defined(USE_AURA)
  TestDispatcherClient dispatcher_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTest);
};

TEST_F(MenuControllerTest, Basic) {
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(
      base::MessageLoop::current());
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::Step1_RunMenu, base::Unretained(this)));
}

#if defined(OS_LINUX) && defined(USE_X11)
// Tests that an event targeter which blocks events will be honored by the menu
// event dispatcher.
TEST_F(MenuControllerTest, EventTargeter) {
  {
    // Verify that the menu handles the escape key under normal circumstances.
    scoped_ptr<Widget> owner(CreateOwnerWidget());
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::DispatchEscapeAndExpect,
                   base::Unretained(this),
                   MenuController::EXIT_OUTERMOST));
    RunMenu(owner.get());
  }

  {
    // With the NULL targeter instantiated and assigned we expect the menu to
    // not handle the key event.
    scoped_ptr<Widget> owner(CreateOwnerWidget());
    aura::ScopedWindowTargeter scoped_targeter(
        owner->GetNativeWindow()->GetRootWindow(),
        scoped_ptr<ui::EventTargeter>(new ui::NullEventTargeter));
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MenuControllerTest::DispatchEscapeAndExpect,
                   base::Unretained(this),
                   MenuController::EXIT_NONE));
    RunMenu(owner.get());
  }
}
#endif

#if defined(USE_X11)

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
};

// Tests that touch event ids are released correctly. See crbug.com/439051 for
// details. When the ids aren't managed correctly, we get stuck down touches.
TEST_F(MenuControllerTest, TouchIdsReleasedCorrectly) {
  scoped_ptr<Widget> owner(CreateOwnerWidget());
  TestEventHandler test_event_handler;
  owner->GetNativeWindow()->GetRootWindow()->AddPreTargetHandler(
      &test_event_handler);

  std::vector<int> devices;
  devices.push_back(1);
  ui::SetUpTouchDevicesForTest(devices);

  DispatchTouch(XI_TouchBegin, 0);
  DispatchTouch(XI_TouchBegin, 1);
  DispatchTouch(XI_TouchEnd, 0);

  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&MenuControllerTest::DispatchTouch,
                                      base::Unretained(this), XI_TouchEnd, 1));

  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MenuControllerTest::DispatchEscapeAndExpect,
                 base::Unretained(this), MenuController::EXIT_OUTERMOST));

  RunMenu(owner.get());
  EXPECT_EQ(0, test_event_handler.outstanding_touches());

  owner->GetNativeWindow()->GetRootWindow()->RemovePreTargetHandler(
      &test_event_handler);
}
#endif // defined(USE_X11)

// Tests that initial selected menu items are correct when items are enabled or
// disabled.
TEST_F(MenuControllerTest, InitialSelectedItem) {
  scoped_ptr<Widget> owner(CreateOwnerWidget());
  scoped_ptr<TestMenuItemView> menu_item(new TestMenuItemView);
  menu_item->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
  menu_item->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
  menu_item->AppendMenuItemWithLabel(3, base::ASCIIToUTF16("Three"));
  SetupMenu(owner.get(), menu_item.get());

  // Leave items "Two" and "Three" enabled.
  menu_item->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  // The first selectable item should be item "Two".
  MenuItemView* first_selectable =
      FindInitialSelectableMenuItemDown(menu_item.get());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  // The last selectable item should be item "Three".
  MenuItemView* last_selectable =
      FindInitialSelectableMenuItemUp(menu_item.get());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(3, last_selectable->GetCommand());

  // Leave items "One" and "Two" enabled.
  menu_item->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item.get());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "Two".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item.get());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // Leave only a single item "One" enabled.
  menu_item->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item.get());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "One".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item.get());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(1, last_selectable->GetCommand());

  // Leave only a single item "Three" enabled.
  menu_item->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(true);
  // The first selectable item should be item "Three".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item.get());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(3, first_selectable->GetCommand());
  // The last selectable item should be item "Three".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item.get());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(3, last_selectable->GetCommand());

  // Leave only a single item ("Two") selected. It should be the first and the
  // last selectable item.
  menu_item->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  first_selectable = FindInitialSelectableMenuItemDown(menu_item.get());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  last_selectable = FindInitialSelectableMenuItemUp(menu_item.get());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // There should be no next or previous selectable item since there is only a
  // single enabled item in the menu.
  EXPECT_EQ(nullptr, FindNextSelectableMenuItem(menu_item.get(), 1));
  EXPECT_EQ(nullptr, FindPreviousSelectableMenuItem(menu_item.get(), 1));

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Down' and 'Up' iterates over enabled
// items.
TEST_F(MenuControllerTest, NextSelectedItem) {
  scoped_ptr<Widget> owner(CreateOwnerWidget());
  scoped_ptr<TestMenuItemView> menu_item(new TestMenuItemView);
  menu_item->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
  menu_item->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
  menu_item->AppendMenuItemWithLabel(3, base::ASCIIToUTF16("Three"));
  menu_item->AppendMenuItemWithLabel(4, base::ASCIIToUTF16("Four"));
  // Disabling the item "Three" gets it skipped when using keyboard to navigate.
  menu_item->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);

  SetupMenu(owner.get(), menu_item.get());

  // Fake initial hot selection.
  SetPendingStateItem(menu_item->GetSubmenu()->GetMenuItemAt(0));
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
  scoped_ptr<Widget> owner(CreateOwnerWidget());
  scoped_ptr<TestMenuItemViewShown> menu_item(new TestMenuItemViewShown);
  menu_item->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
  menu_item->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
  menu_item->AppendMenuItemWithLabel(3, base::ASCIIToUTF16("Three"));
  menu_item->AppendMenuItemWithLabel(4, base::ASCIIToUTF16("Four"));
  // Disabling the item "Four" gets it skipped when using keyboard to navigate.
  menu_item->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);

  SetupMenu(owner.get(), menu_item.get());

  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item.get());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Move up and select a previous (in our case the last enabled) item.
  DecrementSelection();
  EXPECT_EQ(3, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

}  // namespace test
}  // namespace views
