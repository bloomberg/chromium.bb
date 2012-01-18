// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/key_event_tracker.h"

#include "remoting/proto/event.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {

MATCHER_P2(EqualsKeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

class MockInputStub : public protocol::InputStub {
 public:
  MockInputStub() {}

  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent&));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent&));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

static protocol::KeyEvent KeyEvent(int keycode, bool pressed) {
  protocol::KeyEvent event;
  event.set_keycode(keycode);
  event.set_pressed(pressed);
  return event;
}

static void PressAndRelease(protocol::InputStub* input_stub, int keycode) {
  input_stub->InjectKeyEvent(KeyEvent(keycode, true));
  input_stub->InjectKeyEvent(KeyEvent(keycode, false));
}

// Verify that keys that were pressed and released aren't re-released.
TEST(KeyEventTrackerTest, NothingToRelease) {
  MockInputStub mock_stub;
  protocol::KeyEventTracker key_tracker(&mock_stub);
  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, false)));
  }
  PressAndRelease(&key_tracker, 1);
  PressAndRelease(&key_tracker, 2);
}

// Verify that keys that were left pressed get released.
TEST(KeyEventTrackerTest, ReleaseAllKeys) {
  MockInputStub mock_stub;
  protocol::KeyEventTracker key_tracker(&mock_stub);
  ExpectationSet injects;

  {
    InSequence s;

    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(3, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(1, false)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, true)));
    injects += EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(2, false)));
  }

  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(3, false)))
      .After(injects);

  key_tracker.InjectKeyEvent(KeyEvent(3, true));
  PressAndRelease(&key_tracker, 1);
  PressAndRelease(&key_tracker, 2);
  key_tracker.ReleaseAllKeys();
}

}  // namespace remoting
