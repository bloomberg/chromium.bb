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

MATCHER_P2(EqualsVkeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

MATCHER_P2(EqualsUsbEvent, usb_keycode, pressed, "") {
  return arg.usb_keycode() == static_cast<uint32>(usb_keycode) &&
         arg.pressed() == pressed;
}

static KeyEvent NewVkeyEvent(int keycode, bool pressed) {
  KeyEvent event;
  event.set_keycode(keycode);
  event.set_pressed(pressed);
  return event;
}

static void PressAndReleaseVkey(InputStub* input_stub, int keycode) {
  input_stub->InjectKeyEvent(NewVkeyEvent(keycode, true));
  input_stub->InjectKeyEvent(NewVkeyEvent(keycode, false));
}

static KeyEvent NewUsbEvent(uint32 usb_keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  return event;
}

static void PressAndReleaseUsb(InputStub* input_stub,
                               uint32 usb_keycode) {
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, true));
  input_stub->InjectKeyEvent(NewUsbEvent(usb_keycode, false));
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

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(3, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(3, false)));
  }

  InjectTestSequence(&event_mapper);
  PressAndReleaseVkey(&event_mapper, 3);
}

// Verify that USB keys are remapped at most once, and VKEYs are not mapped.
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

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(3, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(3, false)));
  }

  InjectTestSequence(&event_mapper);
  PressAndReleaseVkey(&event_mapper, 3);
}

static void HandleTrappedKey(MockInputStub* stub, uint32 keycode, bool down) {
  stub->InjectKeyEvent(NewUsbEvent(keycode, down));
}

// Verify that USB keys are trapped, not remapped, and VKEYs are not trapped.
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

    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(3, true)));
    EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsVkeyEvent(3, false)));
  }

  InjectTestSequence(&event_mapper);
  PressAndReleaseVkey(&event_mapper, 3);
}

}  // namespace remoting
