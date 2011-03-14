// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for a device that receives input events.
// This interface handles event messages defined in event.proto.

#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {


InputStub::InputStub() : authenticated_(false) {
}

InputStub::~InputStub() {
}

void InputStub::OnAuthenticated() {
  authenticated_ = true;
}

void InputStub::OnClosed() {
  authenticated_ = false;
}

bool InputStub::authenticated() {
  return authenticated_;
}

}  // namespace protocol
}  // namespace remoting
