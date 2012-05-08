// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/mac_key_event_processor.h"

#include <vector>

namespace remoting {

namespace {

// A list of known keycodes.
const int kShift = 16;
const int kControl = 17;
const int kOption = 18;
const int kCapsLock = 20;
const int kLeftCmd = 91;
const int kRightCmd = 93;

}  // namespace

MacKeyEventProcessor::MacKeyEventProcessor(protocol::InputStub* input_stub)
    : protocol::InputFilter(input_stub) {
}

MacKeyEventProcessor::~MacKeyEventProcessor() {
}

void MacKeyEventProcessor::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (event.pressed()) {
    key_pressed_map_[event.keycode()] = event;
  } else {
    key_pressed_map_.erase(event.keycode());
    if (event.keycode() == kLeftCmd || event.keycode() == kRightCmd)
      GenerateKeyupEvents();
  }

  InputFilter::InjectKeyEvent(event);
}

int MacKeyEventProcessor::NumberOfPressedKeys() const {
  return key_pressed_map_.size();
}

void MacKeyEventProcessor::GenerateKeyupEvents() {
  // A list of key codes to be erased from |key_pressed_map_|.
  typedef std::vector<int> KeycodeList;
  KeycodeList keycodes;

  for (KeyPressedMap::iterator i = key_pressed_map_.begin();
       i != key_pressed_map_.end(); ++i) {
    const int keycode = i->first;

    if (keycode == kCapsLock || keycode == kOption ||
        keycode == kControl || keycode == kShift ||
        keycode == kLeftCmd || keycode == kRightCmd) {
      continue;
    }

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
