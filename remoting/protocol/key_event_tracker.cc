// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/key_event_tracker.h"

#include "base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting {
namespace protocol {

KeyEventTracker::KeyEventTracker(InputStub* input_stub)
    : input_stub_(input_stub) {
}

KeyEventTracker::~KeyEventTracker() {
  DCHECK(pressed_keys_.empty());
}

void KeyEventTracker::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(event.has_pressed());
  DCHECK(event.has_keycode());
  if (event.pressed()) {
    pressed_keys_.insert(event.keycode());
  } else {
    pressed_keys_.erase(event.keycode());
  }
  input_stub_->InjectKeyEvent(event);
}

void KeyEventTracker::InjectMouseEvent(const MouseEvent& event) {
  input_stub_->InjectMouseEvent(event);
}

void KeyEventTracker::ReleaseAllKeys() {
  std::set<int>::iterator i;
  for (i = pressed_keys_.begin(); i != pressed_keys_.end(); ++i) {
    KeyEvent event;
    event.set_keycode(*i);
    event.set_pressed(false);
    input_stub_->InjectKeyEvent(event);
  }
  pressed_keys_.clear();
}

}  // namespace protocol
}  // namespace remoting
