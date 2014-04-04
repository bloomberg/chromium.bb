// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include "ui/events/event_targeter.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

typedef ViewsTestBase RootViewTest;

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for key events.
TEST_F(RootViewTest, ViewTargeterForKeyEvents) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  View* child = new View;
  View* grandchild = new View;

  widget.SetContentsView(content);
  content->AddChildView(child);
  child->AddChildView(grandchild);

  grandchild->SetFocusable(true);
  grandchild->RequestFocus();

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);

  // The focused view should be the initial target of the event.
  ui::EventTarget* current_target = targeter->FindTargetForEvent(root_view,
                                                                 &key_event);
  EXPECT_EQ(grandchild, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(grandchild, &key_event);
  EXPECT_EQ(child, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(child, &key_event);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &key_event);
  EXPECT_EQ(widget.GetRootView(), static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(widget.GetRootView(),
                                                &key_event);
  EXPECT_EQ(NULL, static_cast<View*>(current_target));
}

class DeleteOnKeyEventView : public View {
 public:
  explicit DeleteOnKeyEventView(bool* set_on_key) : set_on_key_(set_on_key) {}
  virtual ~DeleteOnKeyEventView() {}

  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE {
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

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0, false);
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&key_event);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(got_key_event);
}

// Used to determine whether or not a context menu is shown as a result of
// a keypress.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController()
      : show_context_menu_calls_(0),
        menu_source_view_(NULL),
        menu_source_type_(ui::MENU_SOURCE_NONE) {
  }
  virtual ~TestContextMenuController() {}

  int show_context_menu_calls() const { return show_context_menu_calls_; }
  View* menu_source_view() const { return menu_source_view_; }
  ui::MenuSourceType menu_source_type() const { return menu_source_type_; }

  void Reset() {
    show_context_menu_calls_ = 0;
    menu_source_view_ = NULL;
    menu_source_type_ = ui::MENU_SOURCE_NONE;
  }

  // ContextMenuController:
  virtual void ShowContextMenuForView(
      View* source,
      const gfx::Point& point,
      ui::MenuSourceType source_type) OVERRIDE {
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
  ui::KeyEvent nomenu_key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);
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
      ui::ET_KEY_PRESSED, ui::VKEY_F10, ui::EF_SHIFT_DOWN, false);
  details = root_view->OnEventFromSource(&menu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of VKEY_APPS.
  ui::KeyEvent menu_key_event2(ui::ET_KEY_PRESSED, ui::VKEY_APPS, 0, false);
  details = root_view->OnEventFromSource(&menu_key_event2);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();
}

}  // namespace test
}  // namespace views
