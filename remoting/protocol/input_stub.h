// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for a device that receives input events.
// This interface handles event messages defined in event.proto.

#ifndef REMOTING_PROTOCOL_INPUT_STUB_H_
#define REMOTING_PROTOCOL_INPUT_STUB_H_

#include "base/basictypes.h"

class Task;

namespace remoting {
namespace protocol {

class KeyEvent;
class MouseEvent;

class InputStub {
 public:
  InputStub();
  virtual ~InputStub();

  virtual void InjectKeyEvent(const KeyEvent* event, Task* done) = 0;
  virtual void InjectMouseEvent(const MouseEvent* event, Task* done) = 0;

  // Called when the client has authenticated with the host to enable the
  // input event channel.
  // Before this is called, all input event will be ignored.
  void OnAuthenticated();

  // Has the client successfully authenticated with the host?
  // I.e., should we be processing input events?
  bool authenticated();

 private:
  // Initially false, this records whether the client has authenticated with
  // the host.
  bool authenticated_;

  DISALLOW_COPY_AND_ASSIGN(InputStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_INPUT_STUB_H_
