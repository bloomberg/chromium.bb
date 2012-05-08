// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MacKeyEventProcessor is designed to solve the problem of missing keyup
// events on Mac.
//
// PROBLEM
//
// On Mac if user presses CMD and then C key there is no keyup event generated
// for C when user releases the C key before the CMD key.
// The cause is that CMD + C triggers a system action and Chrome injects only a
// keydown event for the C key. Safari shares the same behavior.
//
// This is a list of sample edge cases:
//
// CMD DOWN, C DOWN, C UP, CMD UP
// CMD DOWN, SHIFT DOWN, C DOWN, C UP, CMD UP, SHIFT UP
// CMD DOWN, CAPS LOCK DOWN, CMD DOWN, CAPS LOCK DOWN
// L CMD DOWN, C DOWN, R CMD DOWN, L CMD UP, C UP, R CMD UP
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
// as user releases the key after CMD key.

#ifndef REMOTING_CLIENT_PLUGIN_MAC_KEY_EVENT_PROCESSOR_H_
#define REMOTING_CLIENT_PLUGIN_MAC_KEY_EVENT_PROCESSOR_H_

#include <map>

#include "base/basictypes.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/input_filter.h"

namespace remoting {

namespace protocol {
class InputStub;
}  // namespace protocol

class MacKeyEventProcessor : public protocol::InputFilter {
 public:
  explicit MacKeyEventProcessor(protocol::InputStub* input_stub);
  virtual ~MacKeyEventProcessor();

  // InputFilter overrides.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;

  // Return the number of keys pressed. This method is used by unit test to
  // test correctness of this class.
  int NumberOfPressedKeys() const;

 private:
  // Iterate the current pressed keys and generate keyup events.
  void GenerateKeyupEvents();

  // A map that stores pressed keycodes and the corresponding key event.
  typedef std::map<int, protocol::KeyEvent> KeyPressedMap;
  KeyPressedMap key_pressed_map_;

  DISALLOW_COPY_AND_ASSIGN(MacKeyEventProcessor);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_MAC_KEY_EVENT_PROCESSOR_H_
