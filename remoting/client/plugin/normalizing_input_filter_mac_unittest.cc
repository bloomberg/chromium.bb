// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/normalizing_input_filter_mac.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using remoting::protocol::InputStub;
using remoting::protocol::KeyEvent;
using remoting::protocol::MockInputStub;
using remoting::protocol::MouseEvent;
using remoting::protocol::test::EqualsKeyEventWithNumLock;

namespace remoting {

namespace {

const unsigned int kUsbCapsLock   = 0x070039;
const unsigned int kUsbLeftShift  = 0x0700e1;
const unsigned int kUsbLeftOption = 0x0700e2;
const unsigned int kUsbLeftCmd    = 0x0700e3;
const unsigned int kUsbRightCmd   = 0x0700e7;

KeyEvent MakeKeyEvent(uint32 keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(keycode);
  event.set_pressed(pressed);
  event.set_lock_states(KeyEvent::LOCK_STATES_NUMLOCK);
  return event;
}

}  // namespace

// Test CapsLock press/release.
TEST(NormalizingInputFilterMacTest, CapsLock) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Verifies the generated CapsLock up/down events.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbCapsLock, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbCapsLock, false)));
  }

  // Injecting a CapsLock down event with NumLock on.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbCapsLock, true));
}

// Test without pressing command key.
TEST(NormalizingInputFilterMacTest, NoInjection) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
  }

  // C Down and C Up.
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent('C', false));
}

// Test pressing command key and other normal keys.
TEST(NormalizingInputFilterMacTest, CmdKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Left command key.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, false)));

    // Right command key.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, false)));

    // More than one keys after CMD.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('V', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('V', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, false)));
  }

  // Left command key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, false));

  // Right command key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, false));

  // More than one keys after CMD.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent('V', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, false));
}

// Test pressing command and special keys.
TEST(NormalizingInputFilterMacTest, SpecialKeys) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Command + Shift.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftShift, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftShift, false)));

    // Command + Option.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, true)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOption, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOption, false)));
  }

  // Command + Shift.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftShift, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftShift, false));

  // Command + Option.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOption, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOption, false));
}

// Test pressing multiple command keys.
TEST(NormalizingInputFilterMacTest, MultipleCmdKeys) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftCmd, false)));
  }

  // Test multiple CMD keys at the same time.
  // L CMD Down, C Down, R CMD Down, L CMD Up.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftCmd, false));
}

// Test press C key before command key.
TEST(NormalizingInputFilterMacTest, BeforeCmdKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightCmd, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
  }

  // Press C before command key.
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightCmd, false));
  processor->InjectKeyEvent(MakeKeyEvent('C', false));
}

}  // namespace remoting
