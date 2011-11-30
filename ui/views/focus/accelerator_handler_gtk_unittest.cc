// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkkeysyms.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/rect.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class AcceleratorHandlerGtkTest
    : public testing::Test,
      public WidgetDelegate,
      public ui::AcceleratorTarget {
 public:
  AcceleratorHandlerGtkTest()
      : kMenuAccelerator(ui::VKEY_MENU, false, false, false),
        kHomepageAccelerator(ui::VKEY_HOME, false, false, true),
        content_view_(NULL) {
  }

  virtual void SetUp() {
    window_ = Widget::CreateWindowWithBounds(this, gfx::Rect(0, 0, 500, 500));
    window_->Show();
    FocusManager* focus_manager = window_->GetFocusManager();
    focus_manager->RegisterAccelerator(kMenuAccelerator, this);
    focus_manager->RegisterAccelerator(kHomepageAccelerator, this);
    menu_pressed_ = false;
    home_pressed_ = false;
  }

  virtual void TearDown() {
    window_->Close();

    // Flush the message loop to make application verifiers happy.
    message_loop_.RunAllPending();
  }

  GdkEventKey CreateKeyEvent(GdkEventType type, guint keyval, guint state) {
    GdkEventKey evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = type;
    evt.keyval = keyval;
    // The keyval won't be a "correct" hardware keycode for any real hardware,
    // but the code should never depend on exact hardware keycodes, just the
    // fact that the code for presses and releases of the same key match.
    evt.hardware_keycode = keyval;
    evt.state = state;
    GtkWidget* widget = GTK_WIDGET(window_->GetNativeWindow());
    evt.window = widget->window;
    return evt;
  }

  // AcceleratorTarget implementation.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) {
    if (accelerator == kMenuAccelerator)
      menu_pressed_ = true;
    else if (accelerator == kHomepageAccelerator)
      home_pressed_ = true;
    return true;
  }

  // WidgetDelegate Implementation.
  virtual View* GetContentsView() {
    if (!content_view_)
      content_view_ = new View();
    return content_view_;
  }
  virtual const views::Widget* GetWidget() const {
    return content_view_->GetWidget();
  }
  virtual views::Widget* GetWidget() {
    return content_view_->GetWidget();
  }

  virtual void InitContentView() {
  }

 protected:
  bool menu_pressed_;
  bool home_pressed_;

 private:
  ui::Accelerator kMenuAccelerator;
  ui::Accelerator kHomepageAccelerator;
  Widget* window_;
  View* content_view_;
  MessageLoopForUI message_loop_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratorHandlerGtkTest);
};

// Test that the homepage accelerator (Alt+Home) is activated on key down
// and that the menu accelerator (Alt) is never activated.
TEST_F(AcceleratorHandlerGtkTest, TestHomepageAccelerator) {
  AcceleratorHandler handler;
  GdkEventKey evt;

  ASSERT_FALSE(menu_pressed_);
  ASSERT_FALSE(home_pressed_);

  evt = CreateKeyEvent(GDK_KEY_PRESS, GDK_Alt_L, 0);
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_FALSE(menu_pressed_);
  ASSERT_FALSE(home_pressed_);

  evt = CreateKeyEvent(GDK_KEY_PRESS, GDK_Home, GDK_MOD1_MASK);
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_FALSE(menu_pressed_);
  ASSERT_TRUE(home_pressed_);

  evt = CreateKeyEvent(GDK_KEY_RELEASE, GDK_Home, GDK_MOD1_MASK);
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));
  evt = CreateKeyEvent(GDK_KEY_RELEASE, GDK_Alt_L, 0);
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_FALSE(menu_pressed_);
  ASSERT_TRUE(home_pressed_);
}

// Test that the menu accelerator is activated on key up and not key down.
TEST_F(AcceleratorHandlerGtkTest, TestMenuAccelerator) {
  AcceleratorHandler handler;
  GdkEventKey evt;

  ASSERT_FALSE(menu_pressed_);

  evt = CreateKeyEvent(GDK_KEY_PRESS, GDK_Alt_L, 0);
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_FALSE(menu_pressed_);

  evt = CreateKeyEvent(GDK_KEY_RELEASE, GDK_Alt_L, 0);
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_TRUE(menu_pressed_);
}

// Test that the menu accelerator isn't confused by the interaction of the
// Alt and Shift keys. Try the following sequence on Linux:
//   Press Alt
//   Press Shift
//   Release Alt
//   Release Shift
// The key codes for pressing Alt and releasing Alt are different! This
// caused a bug in a previous version of the code, which is now fixed by
// keeping track of hardware keycodes, which are consistent.
TEST_F(AcceleratorHandlerGtkTest, TestAltShiftInteraction) {
  AcceleratorHandler handler;
  GdkEventKey evt;

  ASSERT_FALSE(menu_pressed_);

  // Press Shift.
  evt = CreateKeyEvent(GDK_KEY_PRESS, GDK_Shift_L, 0);
  evt.hardware_keycode = 0x32;
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));
  // Press Alt - but GDK calls this Meta when Shift is also down.
  evt = CreateKeyEvent(GDK_KEY_PRESS, GDK_Meta_L, 0);
  evt.hardware_keycode = 0x40;
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  // Release Shift.
  evt = CreateKeyEvent(GDK_KEY_RELEASE, GDK_Shift_L, 0);
  evt.hardware_keycode = 0x32;
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));
  // Release Alt - with Shift not down, the keyval is now Alt, but
  // the hardware keycode is unchanged.
  evt = CreateKeyEvent(GDK_KEY_RELEASE, GDK_Alt_L, 0);
  evt.hardware_keycode = 0x40;
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_FALSE(menu_pressed_);

  // Press Alt by itself.
  evt = CreateKeyEvent(GDK_KEY_PRESS, GDK_Alt_L, 0);
  evt.hardware_keycode = 0x40;
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  // This line fails if we don't keep track of hardware keycodes.
  ASSERT_FALSE(menu_pressed_);

  // Release Alt - now this should trigger the menu shortcut.
  evt = CreateKeyEvent(GDK_KEY_RELEASE, GDK_Alt_L, 0);
  evt.hardware_keycode = 0x40;
  EXPECT_TRUE(handler.Dispatch(reinterpret_cast<GdkEvent*>(&evt)));

  ASSERT_TRUE(menu_pressed_);
}

}  // namespace views
