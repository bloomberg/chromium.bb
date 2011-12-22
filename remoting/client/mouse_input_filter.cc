// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/mouse_input_filter.h"

#include "remoting/proto/event.pb.h"

namespace remoting {

MouseInputFilter::MouseInputFilter(protocol::InputStub* input_stub)
    : input_stub_(input_stub) {
  input_size_.setEmpty();
  output_size_.setEmpty();
}

MouseInputFilter::~MouseInputFilter() {
}

void MouseInputFilter::InjectKeyEvent(const protocol::KeyEvent& event) {
  input_stub_->InjectKeyEvent(event);
}

void MouseInputFilter::InjectMouseEvent(const protocol::MouseEvent& event) {
  if (input_size_.isZero() || output_size_.isZero())
    return;

  protocol::MouseEvent out_event(event);
  if (out_event.has_x()) {
    int x = (out_event.x() * output_size_.width()) / input_size_.width();
    out_event.set_x(std::max(0, std::min(output_size_.width() - 1, x)));
  }
  if (out_event.has_y()) {
    int y = (out_event.y() * output_size_.height()) / input_size_.height();
    out_event.set_y(std::max(0, std::min(output_size_.height() - 1, y)));
  }

  input_stub_->InjectMouseEvent(out_event);
}

void MouseInputFilter::set_input_size(const SkISize& size) {
  input_size_ = size;
}

void MouseInputFilter::set_output_size(const SkISize& size) {
  output_size_ = size;
}

}  // namespace remoting
