// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_event_tracker.h"

#include "remoting/proto/event.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {

static const protocol::MouseEvent::MouseButton BUTTON_LEFT =
    protocol::MouseEvent::BUTTON_LEFT;
static const protocol::MouseEvent::MouseButton BUTTON_RIGHT =
    protocol::MouseEvent::BUTTON_RIGHT;

MATCHER_P2(EqualsKeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

MATCHER_P4(EqualsMouseEvent, x, y, button, down, "") {
  return arg.x() == x && arg.y() == y && arg.button() == button &&
         arg.button_down() == down;
}

class MockInputStub : public protocol::InputStub {
 public:
  MockInputStub() {}

  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent&));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent&));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

static protocol::KeyEvent NewKeyEvent(int keycode, bool pressed) {
  protocol::KeyEvent event;
  event.set_keycode(keycode);
  event.set_pressed(pressed);
  return event;
}

static void PressAndReleaseKey(protocol::InputStub* input_stub, int keycode) {
  input_stub->InjectKeyEvent(NewKeyEvent(keycode, true));
  input_stub->InjectKeyEvent(NewKeyEvent(keycode, false));
}

static protocol::MouseEvent NewMouseEvent(int x, int y,
    protocol::MouseEvent::MouseButton button, bool down) {
  protocol::MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  event.set_button(button);
  event.set_button_down(down);
  return event;
}

// Verify that keys that were pressed and released aren't re-released.
TEST(InputEventTrackerTest, NothingToRelease) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);

  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, false)));

    EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, false)));
  }

  PressAndReleaseKey(&input_tracker, 1);
  PressAndReleaseKey(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, false));

  input_tracker.ReleaseAll();
}

// Verify that keys that were left pressed get released.
TEST(InputEventTrackerTest, ReleaseAllKeys) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, false)));

    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_RIGHT, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(0, 0, BUTTON_LEFT, true)));
    injects += EXPECT_CALL(mock_stub,
        InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_LEFT, false)));
  }

  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(3, false)))
      .After(injects);
  EXPECT_CALL(mock_stub,
              InjectMouseEvent(EqualsMouseEvent(1, 1, BUTTON_RIGHT, false)))
      .After(injects);

  input_tracker.InjectKeyEvent(NewKeyEvent(3, true));
  PressAndReleaseKey(&input_tracker, 1);
  PressAndReleaseKey(&input_tracker, 2);

  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_RIGHT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(0, 0, BUTTON_LEFT, true));
  input_tracker.InjectMouseEvent(NewMouseEvent(1, 1, BUTTON_LEFT, false));

  input_tracker.ReleaseAll();
}

}  // namespace remoting
