// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/input_event_dispatcher.h"

#include <fuchsia/ui/input/cpp/fidl.h>
#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/fuchsia/input_event_dispatcher_delegate.h"
#include "ui/events/keycodes/dom/dom_key.h"

using fuchsia::ui::input::InputEvent;
using FuchsiaKeyboardEvent = fuchsia::ui::input::KeyboardEvent;
using FuchsiaPointerEvent = fuchsia::ui::input::PointerEvent;

namespace ui {
namespace {

class InputEventDispatcherTest : public testing::Test,
                                 public InputEventDispatcherDelegate {
 public:
  InputEventDispatcherTest() : dispatcher_(this) {}
  ~InputEventDispatcherTest() override = default;

  void DispatchEvent(Event* event) override {
    DCHECK(!captured_event_);
    captured_event_ = Event::Clone(*event);
  }

 protected:
  InputEventDispatcher dispatcher_;
  std::unique_ptr<ui::Event> captured_event_;

  void ResetCapturedEvent() {
    DCHECK(captured_event_);
    captured_event_.reset();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputEventDispatcherTest);
};

TEST_F(InputEventDispatcherTest, MouseEventButtons) {
  // Left mouse button.
  FuchsiaPointerEvent pointer_event;
  pointer_event.x = 1;
  pointer_event.y = 1;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::DOWN;
  pointer_event.buttons = fuchsia::ui::input::kMouseButtonPrimary;
  pointer_event.type = fuchsia::ui::input::PointerEventType::MOUSE;
  InputEvent event;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_TRUE(captured_event_->AsMouseEvent()->IsLeftMouseButton());
  ResetCapturedEvent();

  // Right mouse button.
  pointer_event.buttons = fuchsia::ui::input::kMouseButtonSecondary;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_TRUE(captured_event_->AsMouseEvent()->IsRightMouseButton());
  ResetCapturedEvent();

  // Middle mouse button.
  pointer_event.buttons = fuchsia::ui::input::kMouseButtonTertiary;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_TRUE(captured_event_->AsMouseEvent()->IsMiddleMouseButton());
  ResetCapturedEvent();

  // Left mouse drag.
  pointer_event.buttons = fuchsia::ui::input::kMouseButtonPrimary;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::MOVE;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_MOUSE_DRAGGED, captured_event_->type());
  ResetCapturedEvent();

  // Left mouse up.
  pointer_event.buttons = fuchsia::ui::input::kMouseButtonPrimary;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::UP;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_MOUSE_RELEASED, captured_event_->type());
}

TEST_F(InputEventDispatcherTest, MouseMove) {
  FuchsiaPointerEvent pointer_event;
  pointer_event.x = 1.5;
  pointer_event.y = 2.5;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::MOVE;
  pointer_event.type = fuchsia::ui::input::PointerEventType::MOUSE;
  InputEvent event;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(1.5, captured_event_->AsMouseEvent()->x());
  EXPECT_EQ(2.5, captured_event_->AsMouseEvent()->y());
}

TEST_F(InputEventDispatcherTest, TouchLocation) {
  FuchsiaPointerEvent pointer_event;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::DOWN;
  pointer_event.type = fuchsia::ui::input::PointerEventType::TOUCH;
  pointer_event.x = 1.5;
  pointer_event.y = 2.5;
  InputEvent event;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(1.5, captured_event_->AsTouchEvent()->x());
  EXPECT_EQ(2.5, captured_event_->AsTouchEvent()->y());
}

TEST_F(InputEventDispatcherTest, TouchPointerIds) {
  FuchsiaPointerEvent pointer_event;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::DOWN;
  pointer_event.type = fuchsia::ui::input::PointerEventType::TOUCH;
  pointer_event.pointer_id = 1;
  InputEvent event;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_PRESSED, captured_event_->type());
  EXPECT_EQ(1, captured_event_->AsTouchEvent()->pointer_details().id);
  ResetCapturedEvent();

  pointer_event.pointer_id = 2;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_PRESSED, captured_event_->type());
  EXPECT_EQ(2, captured_event_->AsTouchEvent()->pointer_details().id);
}

TEST_F(InputEventDispatcherTest, TouchPhases) {
  FuchsiaPointerEvent pointer_event;
  pointer_event.x = 1;
  pointer_event.y = 1;
  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::DOWN;
  pointer_event.type = fuchsia::ui::input::PointerEventType::TOUCH;
  InputEvent event;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_PRESSED, captured_event_->type());
  ResetCapturedEvent();

  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::HOVER;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_PRESSED, captured_event_->type());
  EXPECT_TRUE(captured_event_->AsTouchEvent()->hovering());
  ResetCapturedEvent();

  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::MOVE;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_MOVED, captured_event_->type());
  ResetCapturedEvent();

  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::CANCEL;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_CANCELLED, captured_event_->type());
  ResetCapturedEvent();

  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::UP;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_TOUCH_RELEASED, captured_event_->type());
  ResetCapturedEvent();

  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::ADD;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(nullptr, captured_event_.get());

  pointer_event.phase = fuchsia::ui::input::PointerEventPhase::REMOVE;
  event.set_pointer(pointer_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(nullptr, captured_event_.get());
}

TEST_F(InputEventDispatcherTest, KeyPhases) {
  FuchsiaKeyboardEvent key_event;
  key_event.hid_usage = 4;  // Corresponds to pressing "a" on a US keyboard.
  key_event.phase = fuchsia::ui::input::KeyboardEventPhase::PRESSED;

  InputEvent event;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_PRESSED, captured_event_->type());
  ResetCapturedEvent();

  key_event.phase = fuchsia::ui::input::KeyboardEventPhase::REPEAT;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_PRESSED, captured_event_->type());
  ResetCapturedEvent();

  key_event.phase = fuchsia::ui::input::KeyboardEventPhase::RELEASED;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_RELEASED, captured_event_->type());
  ResetCapturedEvent();

  key_event.phase = fuchsia::ui::input::KeyboardEventPhase::CANCELLED;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_RELEASED, captured_event_->type());
}

TEST_F(InputEventDispatcherTest, KeyModifiers) {
  FuchsiaKeyboardEvent key_event;
  key_event.hid_usage = 4;  // Corresponds to pressing "a" on a US keyboard.
  key_event.phase = fuchsia::ui::input::KeyboardEventPhase::PRESSED;

  InputEvent event;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_PRESSED, captured_event_->type());
  EXPECT_EQ('a',
            DomKey(captured_event_->AsKeyEvent()->GetDomKey()).ToCharacter());
  EXPECT_EQ('a', captured_event_->AsKeyEvent()->GetCharacter());
  ResetCapturedEvent();

  key_event.modifiers = fuchsia::ui::input::kModifierShift;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_PRESSED, captured_event_->type());
  EXPECT_EQ('A',
            DomKey(captured_event_->AsKeyEvent()->GetDomKey()).ToCharacter());
  EXPECT_EQ('A', captured_event_->AsKeyEvent()->GetCharacter());
  EXPECT_TRUE(captured_event_->IsShiftDown());
  EXPECT_FALSE(captured_event_->IsControlDown());
  EXPECT_FALSE(captured_event_->IsAltDown());
  ResetCapturedEvent();

  key_event.modifiers =
      fuchsia::ui::input::kModifierShift | fuchsia::ui::input::kModifierControl;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_PRESSED, captured_event_->type());
  EXPECT_TRUE(captured_event_->IsShiftDown());
  EXPECT_TRUE(captured_event_->IsControlDown());
  EXPECT_FALSE(captured_event_->IsAltDown());
  ResetCapturedEvent();

  key_event.modifiers = fuchsia::ui::input::kModifierShift |
                        fuchsia::ui::input::kModifierControl |
                        fuchsia::ui::input::kModifierAlt;
  event.set_keyboard(key_event);
  dispatcher_.ProcessEvent(event);
  EXPECT_EQ(ET_KEY_PRESSED, captured_event_->type());
  EXPECT_EQ('A',
            DomKey(captured_event_->AsKeyEvent()->GetDomKey()).ToCharacter());
  EXPECT_TRUE(captured_event_->IsShiftDown());
  EXPECT_TRUE(captured_event_->IsControlDown());
  EXPECT_TRUE(captured_event_->IsAltDown());
}

}  // namespace
}  // namespace ui
