// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/normalizing_input_filter_cros.h"

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
using remoting::protocol::test::EqualsMouseButtonEvent;
using remoting::protocol::test::EqualsMouseMoveEvent;

namespace remoting {

namespace {

const unsigned int kUsbLeftOsKey      = 0x0700e3;
const unsigned int kUsbRightOsKey     = 0x0700e7;
const unsigned int kUsbLeftAltKey     = 0x0700e2;
const unsigned int kUsbRightAltKey    = 0x0700e6;

const unsigned int kUsbFunctionKey    = 0x07003a;  // F1
const unsigned int kUsbExtendedKey    = 0x070049;  // Insert
const unsigned int kUsbOtherKey       = 0x07002b;  // Tab

KeyEvent MakeKeyEvent(uint32 keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(keycode);
  event.set_pressed(pressed);
  event.set_lock_states(protocol::KeyEvent::LOCK_STATES_NUMLOCK);
  return event;
}

void PressAndReleaseKey(InputStub* input_stub, uint32 keycode) {
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, true));
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, false));
}

static MouseEvent MakeMouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

static MouseEvent MakeMouseButtonEvent(MouseEvent::MouseButton button,
                                       bool button_down) {
  MouseEvent event;
  event.set_button(button);
  event.set_button_down(button_down);
  return event;
}

}  // namespace

// Test OSKey press/release.
TEST(NormalizingInputFilterCrosTest, PressReleaseOsKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, false)));

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOsKey, true)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOsKey, false)));
  }

  // Inject press & release events for left & right OSKeys.
  PressAndReleaseKey(processor.get(), kUsbLeftOsKey);
  PressAndReleaseKey(processor.get(), kUsbRightOsKey);
}

// Test OSKey key repeat switches it to "modifying" mode.
TEST(NormalizingInputFilterCrosTest, OSKeyRepeats) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
  }

  // Inject a press and repeats for the left OSKey, but don't release it, and
  // verify that the repeats result in press events.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
}

// Test OSKey press followed by function key press and release results in
// just the function key events.
TEST(NormalizingInputFilterCrosTest, FunctionKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbFunctionKey, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbFunctionKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  PressAndReleaseKey(processor.get(), kUsbFunctionKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, false));
}

// Test OSKey press followed by extended key press and release results in
// just the function key events.
TEST(NormalizingInputFilterCrosTest, ExtendedKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbExtendedKey, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbExtendedKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  PressAndReleaseKey(processor.get(), kUsbExtendedKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, false));
}

// Test OSKey press followed by non-function, non-extended key press and release
// results in normal-looking sequence.
TEST(NormalizingInputFilterCrosTest, OtherKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  PressAndReleaseKey(processor.get(), kUsbOtherKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, false));
}

// Test OSKey press followed by extended key press, then normal key press
// results in OSKey switching to modifying mode for the normal key.
TEST(NormalizingInputFilterCrosTest, ExtendedThenOtherKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbExtendedKey, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbExtendedKey, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  PressAndReleaseKey(processor.get(), kUsbExtendedKey);
  PressAndReleaseKey(processor.get(), kUsbOtherKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, false));
}

// Test OSKey press followed by mouse event puts the OSKey into modifying mode.
TEST(NormalizingInputFilterCrosTest, MouseEvent) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseMoveEvent(0, 0)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOsKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, true));
  processor->InjectMouseEvent(MakeMouseMoveEvent(0, 0));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOsKey, false));
}

// Test left alt + right click is remapped to left alt + left click.
TEST(NormalizingInputFilterCrosTest, LeftAltClick) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftAltKey, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_LEFT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_LEFT, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftAltKey, false)));
  }

  // Hold the left alt key while left-clicking. ChromeOS will rewrite this as
  // Alt+RightClick
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftAltKey, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftAltKey, false));
}

// Test that right alt + right click is unchanged.
TEST(NormalizingInputFilterCrosTest, RightAltClick) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightAltKey, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_RIGHT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_RIGHT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbRightAltKey, false)));
  }

  // Hold the right alt key while left-clicking. ChromeOS will rewrite this as
  // Alt+RightClick
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightAltKey, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightAltKey, false));
}

}  // namespace remoting
