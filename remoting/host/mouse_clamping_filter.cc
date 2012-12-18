// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_clamping_filter.h"

#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

MouseClampingFilter::MouseClampingFilter(VideoFrameCapturer* capturer,
                                         protocol::InputStub* input_stub)
  : MouseInputFilter(input_stub), capturer_(capturer) {
}

MouseClampingFilter::~MouseClampingFilter() {
}

void MouseClampingFilter::InjectMouseEvent(const protocol::MouseEvent& event) {
  // Ensure that the MouseInputFilter is clamping to the current dimensions.
  set_output_size(capturer_->size_most_recent());
  set_input_size(capturer_->size_most_recent());
  MouseInputFilter::InjectMouseEvent(event);
}

}  // namespace remoting
