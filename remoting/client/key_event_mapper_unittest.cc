// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/key_event_mapper.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {

using protocol::InputStub;
using protocol::KeyEvent;
using protocol::MockInputStub;

// A hardcoded value used to verify |lock_states| is preserved.
static const uint32 kTestLockStates = protocol::KeyEvent::LOCK_STATES_CAPSLOCK;

MATCHER_P2(EqualsUsbEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32>(usb_keycode) &&
         arg.pressed() == pressed &&
         // |lock_states| is hardcoded to kTestLockStates in all key events.
         arg.lock_states() == kTestLockStates;
}

MATCHER_P2(EqualsUsbEventLockStates, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32>(usb_keycode) &&
         arg.pressed() == pressed &&
         arg.lock_states() == kTestLockStates;
}

static KeyEvent NewUsbEvent(uint32 usb_keycode,
                            bool pressed,
                            uint32 lock_states) {
  KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  event.set_lock_states(lock_states);

  return event;
}

static void PressAndReleaseUsb(InputStub* input_stub, uint32 usb_keycode) {
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, true, kTestLockStates));
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, false, kTestLockStates));
}

static void InjectTestSequence(InputStub* input_stub) {
  for (int i = 1; i <= 5; ++i)
    PressAndReleaseUsb(input_stub, i);
}

// Verify that keys are passed through the KeyEventMapper by default.
TEST(KeyEventMapperTest, NoMappingOrTrapping) {
  MockInputStub mock_stub;
  KeyEventMapper event_mapper(&mock_stub);

  {
    InSequence s;

    for (int i = 1; i <= 5; ++i) {
      EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(i, true)));
      EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(i, false)));
    }

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, false)));
  }

  InjectTestSequence(&event_mapper);
  PressAndReleaseUsb(&event_mapper, 3);
}

// Verify that USB keys are remapped at most once.
TEST(KeyEventMapperTest, RemapKeys) {
  MockInputStub mock_stub;
  KeyEventMapper event_mapper(&mock_stub);
  event_mapper.RemapKey(3, 4);
  event_mapper.RemapKey(4, 3);
  event_mapper.RemapKey(5, 3);

  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(4, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(4, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(3, false)));
  }

  InjectTestSequence(&event_mapper);
}

static void HandleTrappedKey(MockInputStub* stub, const KeyEvent& event) {
  stub->InjectKeyEvent(event);
}

// Verify that trapped and mapped USB keys are trapped but not remapped.
TEST(KeyEventMapperTest, TrapKeys) {
  MockInputStub mock_stub;
  MockInputStub trap_stub;
  KeyEventMapper event_mapper(&mock_stub);
  KeyEventMapper::KeyTrapCallback callback =
      base::Bind(&HandleTrappedKey, base::Unretained(&trap_stub));
  event_mapper.SetTrapCallback(callback);
  event_mapper.TrapKey(4, true);
  event_mapper.TrapKey(5, true);
  event_mapper.RemapKey(3, 4);
  event_mapper.RemapKey(4, 3);
  event_mapper.RemapKey(5, 3);

  {
    InSequence s;

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(1, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(2, false)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(4, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsUsbEvent(4, false)));

    EXPECT_CALL(trap_stub, InjectKeyEvent(EqualsUsbEvent(4, true)));
    EXPECT_CALL(trap_stub, InjectKeyEvent(EqualsUsbEvent(4, false)));
    EXPECT_CALL(trap_stub, InjectKeyEvent(EqualsUsbEvent(5, true)));
    EXPECT_CALL(trap_stub, InjectKeyEvent(EqualsUsbEvent(5, false)));
  }

  InjectTestSequence(&event_mapper);
}

}  // namespace remoting
