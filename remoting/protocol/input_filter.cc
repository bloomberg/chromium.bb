// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_filter.h"

namespace remoting {
namespace protocol {

InputFilter::InputFilter() : input_stub_(NULL) {
}

InputFilter::InputFilter(InputStub* input_stub) : input_stub_(input_stub) {
}

InputFilter::~InputFilter() {
}

void InputFilter::InjectKeyEvent(const KeyEvent& event) {
  if (input_stub_ != NULL)
    input_stub_->InjectKeyEvent(event);
}

void InputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (input_stub_ != NULL)
    input_stub_->InjectMouseEvent(event);
}

}  // namespace protocol
}  // namespace remoting
