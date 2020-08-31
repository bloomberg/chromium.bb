// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/animation/test/test_ink_drop_host.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/button_observer.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

namespace views {

using test::InkDropHostViewTestApi;
using test::TestInkDrop;

namespace {

// No-op test double of a ContextMenuController.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController() = default;
  ~TestContextMenuController() override = default;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContextMenuController);
};

class TestButton : public Button, public ButtonListener {
 public:
  explicit TestButton(bool has_ink_drop_action_on_click) : Button(this) {
    set_has_ink_drop_action_on_click(has_ink_drop_action_on_click);
  }

  ~TestButton() override = default;

  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override {
    if (custom_key_click_action_ == KeyClickAction::kNone)
      return Button::GetKeyClickActionForEvent(event);
    return custom_key_click_action_;
  }

  void ButtonPressed(Button* sender, const ui::Event& event) override {
    pressed_ = true;

    if (!on_button_pressed_handler_.is_null())
      on_button_pressed_handler_.Run();
  }

  void OnClickCanceled(const ui::Event& event) override { canceled_ = true; }

  // Button:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override {
    ++ink_drop_layer_add_count_;
    Button::AddInkDropLayer(ink_drop_layer);
  }
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override {
    ++ink_drop_layer_remove_count_;
    Button::RemoveInkDropLayer(ink_drop_layer);
  }

  bool pressed() { return pressed_; }
  bool canceled() { return canceled_; }
  int ink_drop_layer_add_count() { return ink_drop_layer_add_count_; }
  int ink_drop_layer_remove_count() { return ink_drop_layer_remove_count_; }

  void set_custom_key_click_action(KeyClickAction custom_key_click_action) {
    custom_key_click_action_ = custom_key_click_action;
  }

  void set_on_button_pressed_handler(const base::RepeatingClosure& callback) {
    on_button_pressed_handler_ = callback;
  }

  void Reset() {
    pressed_ = false;
    canceled_ = false;
  }

  // Raised visibility of OnFocus() to public
  void OnFocus() override { Button::OnFocus(); }

 private:
  bool pressed_ = false;
  bool canceled_ = false;

  int ink_drop_layer_add_count_ = 0;
  int ink_drop_layer_remove_count_ = 0;

  KeyClickAction custom_key_click_action_ = KeyClickAction::kNone;

  // If available, will be triggered when the button is pressed.
  base::RepeatingClosure on_button_pressed_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestButton);
};

class TestButtonObserver : public ButtonObserver {
 public:
  TestButtonObserver() = default;
  ~TestButtonObserver() override = default;

  void OnHighlightChanged(views::Button* observed_button,
                          bool highlighted) override {
    observed_button_ = observed_button;
    highlighted_ = highlighted;
  }

  void OnStateChanged(views::Button* observed_button,
                      views::Button::ButtonState old_state) override {
    observed_button_ = observed_button;
    state_changed_ = true;
  }

  void Reset() {
    observed_button_ = nullptr;
    highlighted_ = false;
    state_changed_ = false;
  }

  views::Button* observed_button() { return observed_button_; }
  bool highlighted() const { return highlighted_; }
  bool state_changed() const { return state_changed_; }

 private:
  views::Button* observed_button_ = nullptr;
  bool highlighted_ = false;
  bool state_changed_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestButtonObserver);
};

TestInkDrop* AddTestInkDrop(TestButton* button) {
  auto owned_ink_drop = std::make_unique<TestInkDrop>();
  TestInkDrop* ink_drop = owned_ink_drop.get();
  InkDropHostViewTestApi(button).SetInkDrop(std::move(owned_ink_drop));
  return ink_drop;
}

// TODO(tluk): remove when the appropriate ownership APIs have been added for
// Widget's SetContentsView().
template <typename T>
T* AddContentsView(Widget* widget, std::unique_ptr<T> view) {
  T* view_ptr = view.get();
  widget->SetContentsView(view.release());
  return view_ptr;
}

}  // namespace

class ButtonTest : public ViewsTestBase {
 public:
  ButtonTest() = default;
  ~ButtonTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the Button can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    button_ = AddContentsView(widget(), std::make_unique<TestButton>(false));

    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget()));
    event_generator_->set_assume_window_at_origin(false);
  }

  void TearDown() override {
    if (button_observer_)
      button_->RemoveButtonObserver(button_observer_.get());

    button_observer_.reset();
    widget_.reset();

    ViewsTestBase::TearDown();
  }

  TestInkDrop* CreateButtonWithInkDrop(bool has_ink_drop_action_on_click) {
    button_ = AddContentsView(
        widget(), std::make_unique<TestButton>(has_ink_drop_action_on_click));
    widget_->SetContentsView(button_);
    return AddTestInkDrop(button_);
  }

  void CreateButtonWithRealInkDrop() {
    button_ = AddContentsView(widget(), std::make_unique<TestButton>(false));
    InkDropHostViewTestApi(button_).SetInkDrop(
        std::make_unique<InkDropImpl>(button_, button_->size()));
    widget_->SetContentsView(button_);
  }

  void CreateButtonWithObserver() {
    button_ = AddContentsView(widget(), std::make_unique<TestButton>(false));
    button_observer_ = std::make_unique<TestButtonObserver>();
    button_->AddButtonObserver(button_observer_.get());
    widget_->SetContentsView(button_);
  }

 protected:
  Widget* widget() { return widget_.get(); }
  TestButton* button() { return button_; }
  TestButtonObserver* button_observer() { return button_observer_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  void SetDraggedView(View* dragged_view) {
    widget_->dragged_view_ = dragged_view;
  }

 private:
  std::unique_ptr<Widget> widget_;
  TestButton* button_;
  std::unique_ptr<TestButtonObserver> button_observer_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(ButtonTest);
};

// Tests that hover state changes correctly when visiblity/enableness changes.
TEST_F(ButtonTest, HoverStateOnVisibilityChange) {
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  button()->SetEnabled(false);
  EXPECT_EQ(Button::STATE_DISABLED, button()->state());

  button()->SetEnabled(true);
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  button()->SetVisible(false);
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());

  button()->SetVisible(true);
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

#if defined(USE_AURA)
  {
    // If another widget has capture, the button should ignore mouse position
    // and not enter hovered state.
    Widget second_widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(700, 700, 10, 10);
    second_widget.Init(std::move(params));
    second_widget.Show();
    second_widget.GetNativeWindow()->SetCapture();

    button()->SetEnabled(false);
    EXPECT_EQ(Button::STATE_DISABLED, button()->state());

    button()->SetEnabled(true);
    EXPECT_EQ(Button::STATE_NORMAL, button()->state());

    button()->SetVisible(false);
    EXPECT_EQ(Button::STATE_NORMAL, button()->state());

    button()->SetVisible(true);
    EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  }
#endif

// Disabling cursor events occurs for touch events and the Ash magnifier. There
// is no touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)
  aura::test::TestCursorClient cursor_client(GetRootWindow(widget()));

  // In Aura views, no new hover effects are invoked if mouse events
  // are disabled.
  cursor_client.DisableMouseEvents();

  button()->SetEnabled(false);
  EXPECT_EQ(Button::STATE_DISABLED, button()->state());

  button()->SetEnabled(true);
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());

  button()->SetVisible(false);
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());

  button()->SetVisible(true);
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
#endif  // !defined(OS_MACOSX) || defined(USE_AURA)
}

// Tests that the hover state is preserved during a view hierarchy update of a
// button's child View.
TEST_F(ButtonTest, HoverStatePreservedOnDescendantViewHierarchyChange) {
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(Button::STATE_HOVERED, button()->state());
  Label* child = new Label(base::string16());
  button()->AddChildView(child);
  delete child;
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());
}

// Tests the different types of NotifyActions.
TEST_F(ButtonTest, NotifyAction) {
  gfx::Point center(10, 10);

  // By default the button should notify its listener on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());
  EXPECT_FALSE(button()->pressed());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());
  EXPECT_TRUE(button()->pressed());

  // Set the notify action to its listener on mouse press.
  button()->Reset();
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnPress);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());
  EXPECT_TRUE(button()->pressed());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());
  EXPECT_FALSE(button()->pressed());
}

// Tests that OnClickCanceled gets called when NotifyClick is not expected
// anymore.
TEST_F(ButtonTest, NotifyActionNoClick) {
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
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnPress);
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

void PerformGesture(Button* button, ui::EventType event_type) {
  ui::GestureEventDetails gesture_details(event_type);
  ui::GestureEvent gesture_event(0, 0, 0, base::TimeTicks(), gesture_details);
  button->OnGestureEvent(&gesture_event);
}

}  // namespace

// Tests that gesture events correctly change the button state.
TEST_F(ButtonTest, GestureEventsSetState) {
  aura::test::TestCursorClient cursor_client(GetRootWindow(widget()));

  EXPECT_EQ(Button::STATE_NORMAL, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_TAP_DOWN);
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  PerformGesture(button(), ui::ET_GESTURE_TAP_CANCEL);
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
}

// Tests that if the button was disabled in its button press handler, gesture
// events will not revert the disabled state back to normal.
// https://crbug.com/1084241.
TEST_F(ButtonTest, GestureEventsRespectDisabledState) {
  button()->set_on_button_pressed_handler(base::BindRepeating(
      [](TestButton* button) { button->SetEnabled(false); }, button()));

  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  event_generator()->GestureTapAt(button()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(Button::STATE_DISABLED, button()->state());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

// Ensure subclasses of Button are correctly recognized as Button.
TEST_F(ButtonTest, AsButton) {
  base::string16 text;

  LabelButton label_button(nullptr, text);
  EXPECT_TRUE(Button::AsButton(&label_button));

  ImageButton image_button(nullptr);
  EXPECT_TRUE(Button::AsButton(&image_button));

  Checkbox checkbox(text);
  EXPECT_TRUE(Button::AsButton(&checkbox));

  RadioButton radio_button(text, 0);
  EXPECT_TRUE(Button::AsButton(&radio_button));

  MenuButton menu_button(text, nullptr);
  EXPECT_TRUE(Button::AsButton(&menu_button));

  ToggleButton toggle_button(nullptr);
  EXPECT_TRUE(Button::AsButton(&toggle_button));

  Label label;
  EXPECT_FALSE(Button::AsButton(&label));

  Link link(text);
  EXPECT_FALSE(Button::AsButton(&link));

  Textfield textfield;
  EXPECT_FALSE(Button::AsButton(&textfield));
}

// Tests that pressing a button shows the ink drop and releasing the button
// does not hide the ink drop.
// Note: Ink drop is not hidden upon release because Button descendants
// may enter a different ink drop state.
TEST_F(ButtonTest, ButtonClickTogglesInkDrop) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

// Tests that pressing a button shows and releasing capture hides ink drop.
// Releasing capture should also reset PRESSED button state to NORMAL.
TEST_F(ButtonTest, CaptureLossHidesInkDrop) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  EXPECT_EQ(Button::ButtonState::STATE_PRESSED, button()->state());
  SetDraggedView(button());
  widget()->SetCapture(button());
  widget()->ReleaseCapture();
  SetDraggedView(nullptr);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  EXPECT_EQ(Button::ButtonState::STATE_NORMAL, button()->state());
}

TEST_F(ButtonTest, HideInkDropWhenShowingContextMenu) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  TestContextMenuController context_menu_controller;
  button()->set_context_menu_controller(&context_menu_controller);
  button()->set_hide_ink_drop_when_showing_context_menu(true);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_FALSE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, DontHideInkDropWhenShowingContextMenu) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  TestContextMenuController context_menu_controller;
  button()->set_context_menu_controller(&context_menu_controller);
  button()->set_hide_ink_drop_when_showing_context_menu(false);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, HideInkDropOnBlur) {
  gfx::Point center(10, 10);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  button()->OnFocus();

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnBlur();
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(button()->pressed());
}

TEST_F(ButtonTest, HideInkDropHighlightOnDisable) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(ink_drop->is_hovered());
  button()->SetEnabled(false);
  EXPECT_FALSE(ink_drop->is_hovered());
  button()->SetEnabled(true);
  EXPECT_TRUE(ink_drop->is_hovered());
}

TEST_F(ButtonTest, InkDropAfterTryingToShowContextMenu) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  button()->set_context_menu_controller(nullptr);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, HideInkDropHighlightWhenRemoved) {
  View* contents_view = AddContentsView(widget(), std::make_unique<View>());

  TestButton* button =
      contents_view->AddChildView(std::make_unique<TestButton>(false));
  button->SetBounds(0, 0, 200, 200);
  TestInkDrop* ink_drop = AddTestInkDrop(button);

  // Make sure that the button ink drop is hidden after the button gets removed.
  event_generator()->MoveMouseTo(button->GetBoundsInScreen().origin());
  event_generator()->MoveMouseBy(2, 2);
  EXPECT_TRUE(ink_drop->is_hovered());
  // Set ink-drop state to ACTIVATED to make sure that removing the container
  // sets it back to HIDDEN.
  ink_drop->AnimateToState(InkDropState::ACTIVATED);
  auto owned_button = contents_view->RemoveChildViewT(button);
  button = nullptr;

  EXPECT_FALSE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  // Make sure hiding the ink drop happens even if the button is indirectly
  // being removed.
  View* parent_view = contents_view->AddChildView(std::make_unique<View>());
  parent_view->SetBounds(0, 0, 400, 400);
  button = parent_view->AddChildView(std::move(owned_button));

  // Trigger hovering and then remove from the indirect parent. This should
  // propagate down to Button which should remove the highlight effect.
  EXPECT_FALSE(ink_drop->is_hovered());
  event_generator()->MoveMouseBy(8, 8);
  EXPECT_TRUE(ink_drop->is_hovered());
  // Set ink-drop state to ACTIVATED to make sure that removing the container
  // sets it back to HIDDEN.
  ink_drop->AnimateToState(InkDropState::ACTIVATED);
  auto owned_parent = contents_view->RemoveChildViewT(parent_view);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  EXPECT_FALSE(ink_drop->is_hovered());
}

// Tests that when button is set to notify on release, dragging mouse out and
// back transitions ink drop states correctly.
TEST_F(ButtonTest, InkDropShowHideOnMouseDraggedNotifyOnRelease) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnRelease);

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_FALSE(button()->pressed());
}

// Tests that when button is set to notify on press, dragging mouse out and back
// does not change the ink drop state.
TEST_F(ButtonTest, InkDropShowHideOnMouseDraggedNotifyOnPress) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(true);
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnPress);

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());
  EXPECT_TRUE(button()->pressed());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, InkDropStaysHiddenWhileDragging) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  SetDraggedView(button());
  widget()->SetCapture(button());
  widget()->ReleaseCapture();

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, oob, oob, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  SetDraggedView(nullptr);
}

// VisibilityTestButton tests to see if an ink drop or a layer has been added to
// the button at any point during the visibility state changes of its Widget.
class VisibilityTestButton : public TestButton {
 public:
  VisibilityTestButton() : TestButton(false) {}
  ~VisibilityTestButton() override {
    if (layer())
      ADD_FAILURE();
  }

  // TestButton:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override {
    ADD_FAILURE();
    TestButton::AddInkDropLayer(ink_drop_layer);
  }
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override {
    ADD_FAILURE();
    TestButton::RemoveInkDropLayer(ink_drop_layer);
  }
};

// Test that hiding or closing a Widget doesn't attempt to add a layer due to
// changed visibility states.
TEST_F(ButtonTest, NoLayerAddedForWidgetVisibilityChanges) {
  VisibilityTestButton* button =
      AddContentsView(widget(), std::make_unique<VisibilityTestButton>());

  // Ensure no layers are created during construction.
  EXPECT_TRUE(button->GetVisible());
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created when hiding the widget.
  widget()->Hide();
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created when the widget is reshown.
  widget()->Show();
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created during the closing of the Widget.
  widget()->Close();  // Start an asynchronous close.
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created following the Widget's destruction.
  base::RunLoop().RunUntilIdle();  // Complete the Close().
}

// Verify that the Space key clicks the button on key-press on Mac, and
// key-release on other platforms.
TEST_F(ButtonTest, ActionOnSpace) {
  // Give focus to the button.
  button()->SetFocusForPlatform();
  button()->RequestFocus();
  EXPECT_TRUE(button()->HasFocus());

  ui::KeyEvent space_press(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_TRUE(button()->OnKeyPressed(space_press));

#if defined(OS_MACOSX)
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  EXPECT_TRUE(button()->pressed());
#else
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());
  EXPECT_FALSE(button()->pressed());
#endif

  ui::KeyEvent space_release(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE);

#if defined(OS_MACOSX)
  EXPECT_FALSE(button()->OnKeyReleased(space_release));
#else
  EXPECT_TRUE(button()->OnKeyReleased(space_release));
#endif

  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  EXPECT_TRUE(button()->pressed());
}

// Verify that the Return key clicks the button on key-press on all platforms
// except Mac. On Mac, the Return key performs the default action associated
// with a dialog, even if a button has focus.
TEST_F(ButtonTest, ActionOnReturn) {
  // Give focus to the button.
  button()->SetFocusForPlatform();
  button()->RequestFocus();
  EXPECT_TRUE(button()->HasFocus());

  ui::KeyEvent return_press(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);

#if defined(OS_MACOSX)
  EXPECT_FALSE(button()->OnKeyPressed(return_press));
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  EXPECT_FALSE(button()->pressed());
#else
  EXPECT_TRUE(button()->OnKeyPressed(return_press));
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  EXPECT_TRUE(button()->pressed());
#endif

  ui::KeyEvent return_release(ui::ET_KEY_RELEASED, ui::VKEY_RETURN,
                              ui::EF_NONE);
  EXPECT_FALSE(button()->OnKeyReleased(return_release));
}

// Verify that a subclass may customize the action for a key pressed event.
TEST_F(ButtonTest, CustomActionOnKeyPressedEvent) {
  // Give focus to the button.
  button()->SetFocusForPlatform();
  button()->RequestFocus();
  EXPECT_TRUE(button()->HasFocus());

  // Set the button to handle any key pressed event as kOnKeyPress.
  button()->set_custom_key_click_action(Button::KeyClickAction::kOnKeyPress);

  ui::KeyEvent control_press(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(button()->OnKeyPressed(control_press));
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  EXPECT_TRUE(button()->pressed());

  ui::KeyEvent control_release(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                               ui::EF_NONE);
  EXPECT_FALSE(button()->OnKeyReleased(control_release));
}

// Verifies that ButtonObserver is notified when the button activition highlight
// state is changed. Also verifies the |observed_button| and |highlighted|
// passed to observer are correct.
TEST_F(ButtonTest, ChangingHighlightStateNotifiesListener) {
  CreateButtonWithObserver();
  EXPECT_FALSE(button_observer()->highlighted());

  button()->SetHighlighted(/*bubble_visible=*/true);
  EXPECT_EQ(button_observer()->observed_button(), button());
  EXPECT_TRUE(button_observer()->highlighted());

  button()->SetHighlighted(/*bubble_visible=*/false);
  EXPECT_EQ(button_observer()->observed_button(), button());
  EXPECT_FALSE(button_observer()->highlighted());
}

// Verifies that ButtonObserver is notified when the button state is changed,
// and that the |observed_button| is passed to observer correctly.
TEST_F(ButtonTest, ClickingButtonNotifiesObserverOfStateChanges) {
  CreateButtonWithObserver();

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(button_observer()->observed_button(), button());
  EXPECT_TRUE(button_observer()->state_changed());

  button_observer()->Reset();
  EXPECT_EQ(button_observer()->observed_button(), nullptr);
  EXPECT_FALSE(button_observer()->state_changed());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_observer()->observed_button(), button());
  EXPECT_TRUE(button_observer()->state_changed());
}

// Verifies the ButtonObserver is notified whenever Button::SetState() is
// called directly.
TEST_F(ButtonTest, SetStateNotifiesObserver) {
  CreateButtonWithObserver();

  button()->SetState(Button::ButtonState::STATE_HOVERED);
  EXPECT_EQ(button_observer()->observed_button(), button());
  EXPECT_TRUE(button_observer()->state_changed());

  button_observer()->Reset();
  EXPECT_EQ(button_observer()->observed_button(), nullptr);
  EXPECT_FALSE(button_observer()->state_changed());

  button()->SetState(Button::ButtonState::STATE_NORMAL);
  EXPECT_EQ(button_observer()->observed_button(), button());
  EXPECT_TRUE(button_observer()->state_changed());
}

}  // namespace views
