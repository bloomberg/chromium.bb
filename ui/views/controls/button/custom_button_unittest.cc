// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/custom_button.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/screen.h"
#include "ui/views/animation/ink_drop_delegate.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

namespace views {

namespace {

class TestCustomButton : public CustomButton, public ButtonListener {
 public:
  explicit TestCustomButton()
      : CustomButton(this) {
  }

  ~TestCustomButton() override {}

  void ButtonPressed(Button* sender, const ui::Event& event) override {
    pressed_ = true;
  }

  void OnClickCanceled(const ui::Event& event) override {
    canceled_ = true;
  }

  bool pressed() { return pressed_; }
  bool canceled() { return canceled_; }

  void Reset() {
    pressed_ = false;
    canceled_ = false;
  }

  // CustomButton methods:
  bool IsChildWidget() const override { return is_child_widget_; }
  bool FocusInChildWidget() const override { return focus_in_child_widget_; }

  void set_child_widget(bool b) { is_child_widget_ = b; }
  void set_focus_in_child_widget(bool b) { focus_in_child_widget_ = b; }

 private:
  bool pressed_ = false;
  bool canceled_ = false;
  bool is_child_widget_ = false;
  bool focus_in_child_widget_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCustomButton);
};

class TestWidget : public Widget {
 public:
  TestWidget() : Widget() {}

  // Widget method:
  bool IsActive() const override { return active_; }

  void set_active(bool active) { active_ = active; }

 private:
  bool active_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWidget);
};

// An InkDropDelegate that keeps track of ink drop visibility.
class TestInkDropDelegate : public InkDropDelegate {
 public:
  TestInkDropDelegate(InkDropHost* ink_drop_host,
                      bool* ink_shown,
                      bool* ink_hidden)
      : ink_drop_host_(ink_drop_host),
        ink_shown_(ink_shown),
        ink_hidden_(ink_hidden) {
    ink_drop_host_->AddInkDropLayer(nullptr);
  }
  ~TestInkDropDelegate() override {}

  // InkDropDelegate:
  void SetInkDropSize(int large_size,
                      int large_corner_radius,
                      int small_size,
                      int small_corner_radius) override {}
  void OnLayout() override {}
  void OnAction(InkDropState state) override {
    switch (state) {
      case InkDropState::ACTION_PENDING:
      case InkDropState::SLOW_ACTION_PENDING:
      case InkDropState::ACTIVATED:
        *ink_shown_ = true;
        break;
      case InkDropState::HIDDEN:
        *ink_hidden_ = true;
        break;
      case InkDropState::QUICK_ACTION:
      case InkDropState::SLOW_ACTION:
      case InkDropState::DEACTIVATED:
        break;
    }
  }

  void SetHovered(bool is_hovered) override {}

 private:
  InkDropHost* ink_drop_host_;
  bool* ink_shown_;
  bool* ink_hidden_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropDelegate);
};

// A test Button class that owns a TestInkDropDelegate.
class TestButtonWithInkDrop : public TestCustomButton {
 public:
  TestButtonWithInkDrop(bool* ink_shown, bool* ink_hidden)
      : TestCustomButton(),
        ink_drop_delegate_(
            new TestInkDropDelegate(this, ink_shown, ink_hidden)) {
    set_ink_drop_delegate(ink_drop_delegate_.get());
  }
  ~TestButtonWithInkDrop() override {}

  // views::InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override {}
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override {}
  gfx::Point CalculateInkDropCenter() const override { return gfx::Point(); }

 private:
  scoped_ptr<views::InkDropDelegate> ink_drop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestButtonWithInkDrop);
};

class CustomButtonTest : public ViewsTestBase {
 public:
  CustomButtonTest() {}
  ~CustomButtonTest() override {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the CustomButton can query the hover state
    // correctly.
    widget_.reset(new TestWidget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(params);
    widget_->Show();

    button_ = new TestCustomButton();
    widget_->SetContentsView(button_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void CreateButtonWithInkDrop() {
    delete button_;
    ink_shown_ = false;
    ink_hidden_ = false;
    button_ = new TestButtonWithInkDrop(&ink_shown_, &ink_hidden_);
    widget_->SetContentsView(button_);
  }

 protected:
  TestWidget* widget() { return widget_.get(); }
  TestCustomButton* button() { return button_; }
  bool ink_shown() const { return ink_shown_; }
  bool ink_hidden() const { return ink_hidden_; }

 private:
  scoped_ptr<TestWidget> widget_;
  TestCustomButton* button_;
  bool ink_shown_ = false;
  bool ink_hidden_ = false;

  DISALLOW_COPY_AND_ASSIGN(CustomButtonTest);
};

}  // namespace

// Tests that hover state changes correctly when visiblity/enableness changes.
TEST_F(CustomButtonTest, HoverStateOnVisibilityChange) {
  gfx::Point center(10, 10);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());

  button()->SetEnabled(false);
  EXPECT_EQ(CustomButton::STATE_DISABLED, button()->state());

  button()->SetEnabled(true);
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());

  button()->SetVisible(false);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

  button()->SetVisible(true);
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());

#if defined(USE_AURA)
  {
    // If another widget has capture, the button should ignore mouse position
    // and not enter hovered state.
    Widget second_widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(700, 700, 10, 10);
    second_widget.Init(params);
    second_widget.Show();
    second_widget.GetNativeWindow()->SetCapture();

    button()->SetEnabled(false);
    EXPECT_EQ(CustomButton::STATE_DISABLED, button()->state());

    button()->SetEnabled(true);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

    button()->SetVisible(false);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

    button()->SetVisible(true);
    EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
  }
#endif

// Disabling cursor events occurs for touch events and the Ash magnifier. There
// is no touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)
  aura::test::TestCursorClient cursor_client(
      widget()->GetNativeView()->GetRootWindow());

  // In Aura views, no new hover effects are invoked if mouse events
  // are disabled.
  cursor_client.DisableMouseEvents();

  button()->SetEnabled(false);
  EXPECT_EQ(CustomButton::STATE_DISABLED, button()->state());

  button()->SetEnabled(true);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

  button()->SetVisible(false);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

  button()->SetVisible(true);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
#endif  // !defined(OS_MACOSX) || defined(USE_AURA)
}

// Tests the different types of NotifyActions.
TEST_F(CustomButtonTest, NotifyAction) {
  gfx::Point center(10, 10);

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());
  EXPECT_FALSE(button()->pressed());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());
  EXPECT_TRUE(button()->pressed());

  // Set the notify action to its listener on mouse press.
  button()->Reset();
  button()->set_notify_action(CustomButton::NOTIFY_ON_PRESS);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());
  EXPECT_TRUE(button()->pressed());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(CustomButton::STATE_HOVERED, button()->state());
  EXPECT_FALSE(button()->pressed());
}

TEST_F(CustomButtonTest, HandleAccelerator) {
  // Child widgets shouldn't handle accelerators when they are not focused.
  EXPECT_FALSE(button()->IsChildWidget());
  EXPECT_FALSE(button()->FocusInChildWidget());
  EXPECT_FALSE(widget()->IsActive());
  button()->AcceleratorPressed(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  EXPECT_FALSE(button()->pressed());
  // Child without focus.
  button()->set_child_widget(true);
  button()->set_focus_in_child_widget(false);
  button()->AcceleratorPressed(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  EXPECT_FALSE(button()->pressed());
  button()->Reset();
  // Child with focus.
  button()->set_child_widget(true);
  button()->set_focus_in_child_widget(true);
  button()->AcceleratorPressed(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  EXPECT_TRUE(button()->pressed());
  button()->Reset();
  // Not a child, but active.
  button()->set_child_widget(false);
  button()->set_focus_in_child_widget(true);
  widget()->set_active(true);
  button()->AcceleratorPressed(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  EXPECT_TRUE(button()->pressed());
}

// Tests that OnClickCanceled gets called when NotifyClick is not expected
// anymore.
TEST_F(CustomButtonTest, NotifyActionNoClick) {
  gfx::Point center(10, 10);

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(button()->canceled());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_TRUE(button()->canceled());

  // Set the notify action to its listener on mouse press.
  button()->Reset();
  button()->set_notify_action(CustomButton::NOTIFY_ON_PRESS);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  // OnClickCanceled is only sent on mouse release.
  EXPECT_FALSE(button()->canceled());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(button()->canceled());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)

namespace {

void PerformGesture(CustomButton* button, ui::EventType event_type) {
  ui::GestureEventDetails gesture_details(event_type);
  base::TimeDelta time_stamp = base::TimeDelta::FromMicroseconds(0);
  ui::GestureEvent gesture_event(0, 0, 0, time_stamp, gesture_details);
  button->OnGestureEvent(&gesture_event);
}

}  // namespace

// Tests that gesture events correctly change the button state.
TEST_F(CustomButtonTest, GestureEventsSetState) {
  aura::test::TestCursorClient cursor_client(
      widget()->GetNativeView()->GetRootWindow());

  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_TAP_DOWN);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_EQ(CustomButton::STATE_PRESSED, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_TAP_CANCEL);
  EXPECT_EQ(CustomButton::STATE_NORMAL, button()->state());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

// Ensure subclasses of CustomButton are correctly recognized as CustomButton.
TEST_F(CustomButtonTest, AsCustomButton) {
  base::string16 text;

  LabelButton label_button(NULL, text);
  EXPECT_TRUE(CustomButton::AsCustomButton(&label_button));

  ImageButton image_button(NULL);
  EXPECT_TRUE(CustomButton::AsCustomButton(&image_button));

  Checkbox checkbox(text);
  EXPECT_TRUE(CustomButton::AsCustomButton(&checkbox));

  RadioButton radio_button(text, 0);
  EXPECT_TRUE(CustomButton::AsCustomButton(&radio_button));

  MenuButton menu_button(text, NULL, false);
  EXPECT_TRUE(CustomButton::AsCustomButton(&menu_button));

  Label label;
  EXPECT_FALSE(CustomButton::AsCustomButton(&label));

  Link link(text);
  EXPECT_FALSE(CustomButton::AsCustomButton(&link));

  Textfield textfield;
  EXPECT_FALSE(CustomButton::AsCustomButton(&textfield));
}

// Tests that pressing a button shows the ink drop and releasing the button
// does not hide the ink drop.
// Note: Ink drop is not hidden upon release because CustomButton descendants
// may enter a different ink drop state.
TEST_F(CustomButtonTest, ButtonClickTogglesInkDrop) {
  gfx::Point old_cursor = gfx::Screen::GetScreen()->GetCursorScreenPoint();
  CreateButtonWithInkDrop();

  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());
  generator.set_current_location(gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_TRUE(ink_shown());
  EXPECT_FALSE(ink_hidden());

  generator.ReleaseLeftButton();
  EXPECT_FALSE(ink_hidden());
}

// Tests that pressing a button shows and releasing capture hides ink drop.
TEST_F(CustomButtonTest, CaptureLossHidesInkDrop) {
  gfx::Point old_cursor = gfx::Screen::GetScreen()->GetCursorScreenPoint();
  CreateButtonWithInkDrop();

  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());
  generator.set_current_location(gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_TRUE(ink_shown());
  EXPECT_FALSE(ink_hidden());

  widget()->SetCapture(button());
  widget()->ReleaseCapture();
  EXPECT_TRUE(ink_hidden());
}

}  // namespace views
