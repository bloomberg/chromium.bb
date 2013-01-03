// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_

#include "base/compiler_specific.h"
#include "remoting/protocol/input_stub.h"

namespace pp {
class InputEvent;
}  // namespace pp

namespace remoting {

namespace protocol {
class InputStub;
} // namespace protocol

class PepperInputHandler {
 public:
  explicit PepperInputHandler(protocol::InputStub* input_stub);
  virtual ~PepperInputHandler();

  bool HandleInputEvent(const pp::InputEvent& event);

 private:
  protocol::InputStub* input_stub_;

  // Accumulated sub-pixel deltas from wheel events.
  float wheel_delta_x_;
  float wheel_delta_y_;

  DISALLOW_COPY_AND_ASSIGN(PepperInputHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
