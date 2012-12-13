// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/mac_key_event_processor.h"

#include <vector>

#include "base/logging.h"

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

MacKeyEventProcessor::MacKeyEventProcessor(protocol::InputStub* input_stub)
    : protocol::InputFilter(input_stub) {
}

MacKeyEventProcessor::~MacKeyEventProcessor() {
}

void MacKeyEventProcessor::InjectKeyEvent(const protocol::KeyEvent& event) {
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
    protocol::KeyEvent event;
    event.set_usb_keycode(kUsbCapsLock);

    event.set_pressed(true);
    InputFilter::InjectKeyEvent(event);
    event.set_pressed(false);
    InputFilter::InjectKeyEvent(event);

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

void MacKeyEventProcessor::GenerateKeyupEvents() {
  // A list of key codes to be erased from |key_pressed_map_|.
  typedef std::vector<int> KeycodeList;
  KeycodeList keycodes;

  for (KeyPressedMap::iterator i = key_pressed_map_.begin();
       i != key_pressed_map_.end(); ++i) {
    const int keycode = i->first;

    keycodes.push_back(keycode);
    protocol::KeyEvent event = i->second;
    event.set_pressed(false);
    InputFilter::InjectKeyEvent(event);
  }

  for (KeycodeList::iterator i = keycodes.begin(); i != keycodes.end(); ++i) {
    key_pressed_map_.erase(*i);
  }
}

}  // namespace remoting
