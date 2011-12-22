// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_KEY_EVENT_TRACKER_H_
#define REMOTING_PROTOCOL_KEY_EVENT_TRACKER_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

// Filtering InputStub which tracks key press and release events before passing
// them on to |input_stub|, and can dispatch release events to |input_stub| for
// all currently-pressed keys when necessary.
class KeyEventTracker : public InputStub {
 public:
  KeyEventTracker(protocol::InputStub* input_stub);
  virtual ~KeyEventTracker();

  // Dispatch release events for all currently-pressed keys to the InputStub.
  void ReleaseAllKeys();

  // InputStub interface.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

 private:
  protocol::InputStub* input_stub_;

  std::set<int> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventTracker);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_KEY_EVENT_TRACKER_H_
