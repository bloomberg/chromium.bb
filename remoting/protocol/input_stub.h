// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for a device that receives input events.
// This interface handles event messages defined in event.proto.

#ifndef REMOTING_PROTOCOL_INPUT_STUB_H_
#define REMOTING_PROTOCOL_INPUT_STUB_H_

class Task;

namespace remoting {

class KeyEvent;
class MouseEvent;

namespace protocol {

class InputStub {
 public:
  InputStub() {}
  virtual ~InputStub() {}

  virtual void InjectKeyEvent(const KeyEvent& event, Task* done) = 0;
  virtual void InjectMouseEvent(const MouseEvent& event, Task* done) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_INPUT_STUB_H_
