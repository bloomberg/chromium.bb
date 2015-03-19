// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include "ui/events/event_utils.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

typedef ViewsTestBase RootViewTest;

class DeleteOnKeyEventView : public View {
 public:
  explicit DeleteOnKeyEventView(bool* set_on_key) : set_on_key_(set_on_key) {}
  ~DeleteOnKeyEventView() override {}

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    *set_on_key_ = true;
    delete this;
    return true;
  }

 private:
  // Set to true in OnKeyPressed().
  bool* set_on_key_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnKeyEventView);
};

// Verifies deleting a View in OnKeyPressed() doesn't crash and that the
// target is marked as destroyed in the returned EventDispatchDetails.
TEST_F(RootViewTest, DeleteViewDuringKeyEventDispatch) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  bool got_key_event = false;

  View* content = new View;
  widget.SetContentsView(content);

  View* child = new DeleteOnKeyEventView(&got_key_event);
  content->AddChildView(child);

  // Give focus to |child| so that it will be the target of the key event.
  child->SetFocusable(true);
  child->RequestFocus();

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  ViewTargeter* view_targeter = new ViewTargeter(root_view);
  root_view->SetEventTargeter(make_scoped_ptr(view_targeter));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&key_event);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(got_key_event);
}

// Tracks whether a context menu is shown.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController()
      : show_context_menu_calls_(0),
        menu_source_view_(NULL),
        menu_source_type_(ui::MENU_SOURCE_NONE) {
  }
  ~TestContextMenuController() override {}

  int show_context_menu_calls() const { return show_context_menu_calls_; }
  View* menu_source_view() const { return menu_source_view_; }
  ui::MenuSourceType menu_source_type() const { return menu_source_type_; }

  void Reset() {
    show_context_menu_calls_ = 0;
    menu_source_view_ = NULL;
    menu_source_type_ = ui::MENU_SOURCE_NONE;
  }

  // ContextMenuController:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override {
    show_context_menu_calls_++;
    menu_source_view_ = source;
    menu_source_type_ = source_type;
  }

 private:
  int show_context_menu_calls_;
  View* menu_source_view_;
  ui::MenuSourceType menu_source_type_;

  DISALLOW_COPY_AND_ASSIGN(TestContextMenuController);
};

// Tests that context menus are shown for certain key events (Shift+F10
// and VKEY_APPS) by the pre-target handler installed on RootView.
TEST_F(RootViewTest, ContextMenuFromKeyEvent) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());

  TestContextMenuController controller;
  View* focused_view = new View;
  focused_view->set_context_menu_controller(&controller);
  widget.SetContentsView(focused_view);
  focused_view->SetFocusable(true);
  focused_view->RequestFocus();

  // No context menu should be shown for a keypress of 'A'.
  ui::KeyEvent nomenu_key_event('a', ui::VKEY_A, ui::EF_NONE);
  ui::EventDispatchDetails details =
      root_view->OnEventFromSource(&nomenu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  EXPECT_EQ(NULL, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_NONE, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of Shift+F10.
  ui::KeyEvent menu_key_event(
      ui::ET_KEY_PRESSED, ui::VKEY_F10, ui::EF_SHIFT_DOWN);
  details = root_view->OnEventFromSource(&menu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of VKEY_APPS.
  ui::KeyEvent menu_key_event2(ui::ET_KEY_PRESSED, ui::VKEY_APPS, ui::EF_NONE);
  details = root_view->OnEventFromSource(&menu_key_event2);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();
}

// View which handles all gesture events.
class GestureHandlingView : public View {
 public:
  GestureHandlingView() {
  }

  ~GestureHandlingView() override {}

  void OnGestureEvent(ui::GestureEvent* event) override { event->SetHandled(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureHandlingView);
};

// Tests that context menus are shown for long press by the post-target handler
// installed on the RootView only if the event is targetted at a view which can
// show a context menu.
TEST_F(RootViewTest, ContextMenuFromLongPress) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(100, 100);
  widget.Init(init_params);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());

  // Create a view capable of showing the context menu with two children one of
  // which handles all gesture events (e.g. a button).
  TestContextMenuController controller;
  View* parent_view = new View;
  parent_view->set_context_menu_controller(&controller);
  widget.SetContentsView(parent_view);

  View* gesture_handling_child_view = new GestureHandlingView;
  gesture_handling_child_view->SetBoundsRect(gfx::Rect(10, 10));
  parent_view->AddChildView(gesture_handling_child_view);

  View* other_child_view = new View;
  other_child_view->SetBoundsRect(gfx::Rect(20, 0, 10, 10));
  parent_view->AddChildView(other_child_view);

  // |parent_view| should not show a context menu as a result of a long press on
  // |gesture_handling_child_view|.
  ui::GestureEvent long_press1(
      5,
      5,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&long_press1);

  ui::GestureEvent end1(
      5, 5, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_END));
  details = root_view->OnEventFromSource(&end1);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should show a context menu as a result of a long press on
  // |other_child_view|.
  ui::GestureEvent long_press2(
      25,
      5,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  details = root_view->OnEventFromSource(&long_press2);

  ui::GestureEvent end2(
      25, 5, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_END));
  details = root_view->OnEventFromSource(&end2);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should show a context menu as a result of a long press on
  // itself.
  ui::GestureEvent long_press3(
      50,
      50,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  details = root_view->OnEventFromSource(&long_press3);

  ui::GestureEvent end3(
      25, 5, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_END));
  details = root_view->OnEventFromSource(&end3);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
}

// Tests that context menus are not shown for disabled views on a long press.
TEST_F(RootViewTest, ContextMenuFromLongPressOnDisabledView) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(100, 100);
  widget.Init(init_params);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());

  // Create a view capable of showing the context menu with two children one of
  // which handles all gesture events (e.g. a button). Also mark this view
  // as disabled.
  TestContextMenuController controller;
  View* parent_view = new View;
  parent_view->set_context_menu_controller(&controller);
  parent_view->SetEnabled(false);
  widget.SetContentsView(parent_view);

  View* gesture_handling_child_view = new GestureHandlingView;
  gesture_handling_child_view->SetBoundsRect(gfx::Rect(10, 10));
  parent_view->AddChildView(gesture_handling_child_view);

  View* other_child_view = new View;
  other_child_view->SetBoundsRect(gfx::Rect(20, 0, 10, 10));
  parent_view->AddChildView(other_child_view);

  // |parent_view| should not show a context menu as a result of a long press on
  // |gesture_handling_child_view|.
  ui::GestureEvent long_press1(
      5,
      5,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&long_press1);

  ui::GestureEvent end1(
      5, 5, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_END));
  details = root_view->OnEventFromSource(&end1);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should not show a context menu as a result of a long press on
  // |other_child_view|.
  ui::GestureEvent long_press2(
      25,
      5,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  details = root_view->OnEventFromSource(&long_press2);

  ui::GestureEvent end2(
      25, 5, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_END));
  details = root_view->OnEventFromSource(&end2);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should not show a context menu as a result of a long press on
  // itself.
  ui::GestureEvent long_press3(
      50,
      50,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  details = root_view->OnEventFromSource(&long_press3);

  ui::GestureEvent end3(
      25, 5, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_END));
  details = root_view->OnEventFromSource(&end3);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
}

// This view class provides functionality to delete itself in the context of
// mouse exit event and helps test that we don't crash when we return from
// the mouse exit handler.
class DeleteViewOnMouseExit : public View {
 public:
  explicit DeleteViewOnMouseExit(bool* got_mouse_exit)
      : got_mouse_exit_(got_mouse_exit) {
  }

  ~DeleteViewOnMouseExit() override {}

  void OnMouseExited(const ui::MouseEvent& event) override {
    *got_mouse_exit_ = true;
    delete this;
  }

 private:
  // Set to true in OnMouseExited().
  bool* got_mouse_exit_;

  DISALLOW_COPY_AND_ASSIGN(DeleteViewOnMouseExit);
};

// Verifies deleting a View in OnMouseExited() doesn't crash.
TEST_F(RootViewTest, DeleteViewOnMouseExitDispatch) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);
  widget.SetBounds(gfx::Rect(10, 10, 500, 500));

  View* content = new View;
  widget.SetContentsView(content);

  bool got_mouse_exit = false;
  View* child = new DeleteViewOnMouseExit(&got_mouse_exit);
  content->AddChildView(child);
  child->SetBounds(10, 10, 500, 500);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());

  // Generate a mouse move event which ensures that the mouse_moved_handler_
  // member is set in the RootView class.
  ui::MouseEvent moved_event(ui::ET_MOUSE_MOVED, gfx::Point(15, 15),
                             gfx::Point(100, 100), ui::EventTimeForNow(), 0,
                             0);
  root_view->OnMouseMoved(moved_event);
  EXPECT_FALSE(got_mouse_exit);

  // Generate a mouse exit event which in turn will delete the child view which
  // was the target of the mouse move event above. This should not crash when
  // the mouse exit handler returns from the child.
  ui::MouseEvent exit_event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseExited(exit_event);

  EXPECT_TRUE(got_mouse_exit);
  EXPECT_FALSE(content->has_children());
}

}  // namespace test
}  // namespace views
