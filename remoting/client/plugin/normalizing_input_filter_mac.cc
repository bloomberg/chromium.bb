// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/normalizing_input_filter_mac.h"

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

NormalizingInputFilterMac::NormalizingInputFilterMac(
    protocol::InputStub* input_stub)
    : protocol::InputFilter(input_stub) {
}

NormalizingInputFilterMac::~NormalizingInputFilterMac() {}

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

}  // namespace remoting
