// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_event_tracker.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {
namespace protocol {

namespace {

static const MouseEvent::MouseButton BUTTON_LEFT = MouseEvent::BUTTON_LEFT;
static const MouseEvent::MouseButton BUTTON_RIGHT = MouseEvent::BUTTON_RIGHT;

// A hardcoded value used to verify |lock_states| is preserved.
static const uint32 kTestLockStates = protocol::KeyEvent::LOCK_STATES_CAPSLOCK;

// Verify the usb key code and the "pressed" state.
// Also verify that the event doesn't have |lock_states| set.
MATCHER_P2(EqualsUsbEventWithoutLockStates, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32>(usb_keycode) &&
         arg.pressed() == pressed &&
         !arg.has_lock_states();
}

// Verify the usb key code, the "pressed" state, and the lock states.
MATCHER_P2(EqualsUsbEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32>(usb_keycode) &&
          arg.pressed() == pressed &&
          arg.lock_states() == kTestLockStates;
}

MATCHER_P4(EqualsMouseEvent, x, y, button, down, "") {
  return arg.x() == x && arg.y() == y && arg.button() == button &&
         arg.button_down() == down;
}

static KeyEvent NewUsbEvent(uint32 usb_keycode,
                            bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  // Create all key events with the hardcoded |lock_state| in this test.
  event.set_lock_states(kTestLockStates);
  return event;
}

static void PressAndReleaseUsb(InputStub* input_stub,
                               uint32 usb_keycode) {
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, true));
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, false));
}

static MouseEvent NewMouseEvent(int x, int y,
    MouseEvent::MouseButton button, bool down) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  event.set_button(button);
  event.set_button_down(down);
  return event;
}

}

// Verify that keys that were pressed and released aren't re-released.
TEST(InputEventTrackerTest, NothingToRelease) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);

  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));

    EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, false)));
  }

  PressAndReleaseUsb(&input_tracker, 1);
  PressAndReleaseUsb(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, false));

  input_tracker.ReleaseAll();
}

// Verify that keys that were left pressed get released.
TEST(InputEventTrackerTest, ReleaseAllKeys) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));

    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_RIGHT, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_LEFT, false)));
  }

  // The key should be released but |lock_states| should not be set.
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsUsbEventWithoutLockStates(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_RIGHT, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseUsb(&input_tracker, 1);
  PressAndReleaseUsb(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_RIGHT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(1, 1, BUTTON_LEFT, false));

  EXPECT_FALSE(input_tracker.IsKeyPressed(1));
  EXPECT_FALSE(input_tracker.IsKeyPressed(2));
  EXPECT_TRUE(input_tracker.IsKeyPressed(3));
  EXPECT_EQ(1, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// Verify that we track both USB-based key events correctly.
TEST(InputEventTrackerTest, TrackUsbKeyEvents) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(6, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(7, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(5, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(5, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));
  }

  // The key should be auto released with no |lock_states|.
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsUsbEventWithoutLockStates(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsUsbEventWithoutLockStates(6, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsUsbEventWithoutLockStates(7, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsUsbEventWithoutLockStates(5, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(6, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(7, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(5, true));
  input_tracker.InjectKeyEvent(NewUsbEvent(5, true));
  PressAndReleaseUsb(&input_tracker, 2);

  EXPECT_FALSE(input_tracker.IsKeyPressed(1));
  EXPECT_FALSE(input_tracker.IsKeyPressed(2));
  EXPECT_TRUE(input_tracker.IsKeyPressed(3));
  EXPECT_TRUE(input_tracker.IsKeyPressed(5));
  EXPECT_TRUE(input_tracker.IsKeyPressed(6));
  EXPECT_TRUE(input_tracker.IsKeyPressed(7));
  EXPECT_EQ(4, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

// Verify that invalid events get passed through but not tracked.
TEST(InputEventTrackerTest, InvalidEventsNotTracked) {
  MockInputStub mock_stub;
  InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(_)).Times(2);
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));
  }

  EXPECT_CALL(mock_stub,
              InjectKeyEvent(EqualsUsbEventWithoutLockStates(3, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewUsbEvent(3, true));
  PressAndReleaseUsb(&input_tracker, 1);

  KeyEvent invalid_event1;
  invalid_event1.set_pressed(true);
  input_tracker.InjectKeyEvent(invalid_event1);

  KeyEvent invalid_event2;
  invalid_event2.set_usb_keycode(6);
  input_tracker.InjectKeyEvent(invalid_event2);

  PressAndReleaseUsb(&input_tracker, 2);

  EXPECT_FALSE(input_tracker.IsKeyPressed(1));
  EXPECT_FALSE(input_tracker.IsKeyPressed(2));
  EXPECT_TRUE(input_tracker.IsKeyPressed(3));
  EXPECT_EQ(1, input_tracker.PressedKeyCount());

  input_tracker.ReleaseAll();
}

}  // namespace protocol
}  // namespace remoting
