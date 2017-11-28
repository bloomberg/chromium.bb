// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/input_filter.h"

namespace remoting {
namespace protocol {

InputFilter::InputFilter() : input_stub_(nullptr), enabled_(true) {
}

InputFilter::InputFilter(InputStub* input_stub)
  : input_stub_(input_stub), enabled_(true) {
}

InputFilter::~InputFilter() = default;

void InputFilter::InjectKeyEvent(const KeyEvent& event) {
  if (enabled_ && input_stub_ != nullptr)
    input_stub_->InjectKeyEvent(event);
}

void InputFilter::InjectTextEvent(const TextEvent& event) {
  if (enabled_ && input_stub_ != nullptr)
    input_stub_->InjectTextEvent(event);
}

void InputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (enabled_ && input_stub_ != nullptr)
    input_stub_->InjectMouseEvent(event);
}

void InputFilter::InjectTouchEvent(const TouchEvent& event) {
  if (enabled_ && input_stub_ != nullptr)
    input_stub_->InjectTouchEvent(event);
}

}  // namespace protocol
}  // namespace remoting
