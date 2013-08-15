// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NormalizingInputFilterMac is designed to solve the problem of missing keyup
// events on Mac.
//
// PROBLEM
//
// On Mac if user presses CMD and then C key there is no keyup event generated
// for C when user releases the C key before the CMD key.
// The cause is that CMD + C triggers a system action and Chrome injects only a
// keydown event for the C key. Safari shares the same behavior.
//
// SOLUTION
//
// When a keyup event for CMD key happens we will check all prior keydown
// events received and inject corresponding keyup events artificially, with
// the exception of:
//
// SHIFT, CONTROL, OPTION, LEFT CMD, RIGHT CMD and CAPS LOCK
//
// because they are reported by Chrome correctly.
//
// There are a couple cases that this solution doesn't work perfectly, one
// of them leads to duplicated keyup events.
//
// User performs this sequence of actions:
//
// CMD DOWN, C DOWN, CMD UP, C UP
//
// In this case the algorithm will generate:
//
// CMD DOWN, C DOWN, C UP, CMD UP, C UP
//
// Because we artificially generate keyup events the C UP event is duplicated
// as user releases the key after CMD key. This would not be a problem as the
// receiver end will drop this duplicated keyup event.

#include "remoting/client/plugin/normalizing_input_filter.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

namespace {

const unsigned int kUsbCapsLock     = 0x070039;
const unsigned int kUsbLeftControl  = 0x0700e0;
const unsigned int kUsbLeftShift    = 0x0700e1;
const unsigned int kUsbLeftOption   = 0x0700e2;
const unsigned int kUsbLeftCmd      = 0x0700e3;
const unsigned int kUsbRightControl = 0x0700e4;
const unsigned int kUsbRightShift   = 0x0700e5;
const unsigned int kUsbRightOption  = 0x0700e6;
const unsigned int kUsbRightCmd     = 0x0700e7;
const unsigned int kUsbTab          = 0x07002b;

}  // namespace

class NormalizingInputFilterMac : public protocol::InputFilter {
 public:
  explicit NormalizingInputFilterMac(protocol::InputStub* input_stub);
  virtual ~NormalizingInputFilterMac() {}

  // InputFilter overrides.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;

 private:
  // Generate keyup events for any keys pressed with CMD.
  void GenerateKeyupEvents();

  // A map that stores pressed keycodes and the corresponding key event.
  typedef std::map<int, protocol::KeyEvent> KeyPressedMap;
  KeyPressedMap key_pressed_map_;

  DISALLOW_COPY_AND_ASSIGN(NormalizingInputFilterMac);
};

NormalizingInputFilterMac::NormalizingInputFilterMac(
    protocol::InputStub* input_stub)
    : protocol::InputFilter(input_stub) {
}

void NormalizingInputFilterMac::InjectKeyEvent(const protocol::KeyEvent& event)
{
  DCHECK(event.has_usb_keycode());

  bool is_special_key = event.usb_keycode() == kUsbLeftControl ||
      event.usb_keycode() == kUsbLeftShift ||
      event.usb_keycode() == kUsbLeftOption ||
      event.usb_keycode() == kUsbRightControl ||
      event.usb_keycode() == kUsbRightShift ||
      event.usb_keycode() == kUsbRightOption ||
      event.usb_keycode() == kUsbTab;

  bool is_cmd_key = event.usb_keycode() == kUsbLeftCmd ||
      event.usb_keycode() == kUsbRightCmd;

  if (event.usb_keycode() == kUsbCapsLock) {
    // Mac OS X generates keydown/keyup on lock-state transitions, rather than
    // when the key is pressed & released, so fake keydown/keyup on each event.
    protocol::KeyEvent newEvent(event);

    newEvent.set_pressed(true);
    InputFilter::InjectKeyEvent(newEvent);
    newEvent.set_pressed(false);
    InputFilter::InjectKeyEvent(newEvent);

    return;
  } else if (!is_cmd_key && !is_special_key) {
    // Track keydown/keyup events for non-modifiers, so we can release them if
    // necessary (see below).
    if (event.pressed()) {
      key_pressed_map_[event.usb_keycode()] = event;
    } else {
      key_pressed_map_.erase(event.usb_keycode());
    }
  }

  if (is_cmd_key && !event.pressed()) {
    // Mac OS X will not generate release events for keys pressed while Cmd is
    // pressed, so release all pressed keys when Cmd is released.
    GenerateKeyupEvents();
  }

  InputFilter::InjectKeyEvent(event);
}

void NormalizingInputFilterMac::GenerateKeyupEvents() {
  for (KeyPressedMap::iterator i = key_pressed_map_.begin();
       i != key_pressed_map_.end(); ++i) {
    // The generated key up event will have the same key code and lock states
    // as the original key down event.
    protocol::KeyEvent event = i->second;
    event.set_pressed(false);
    InputFilter::InjectKeyEvent(event);
  }

  // Clearing the map now that we have released all the pressed keys.
  key_pressed_map_.clear();
}

scoped_ptr<protocol::InputFilter> CreateNormalizingInputFilter(
    protocol::InputStub* input_stub) {
  return scoped_ptr<protocol::InputFilter>(
      new NormalizingInputFilterMac(input_stub));
}

}  // namespace remoting
