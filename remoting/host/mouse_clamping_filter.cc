// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_clamping_filter.h"

#include "remoting/proto/event.pb.h"
#include "remoting/proto/video.pb.h"

namespace remoting {

MouseClampingFilter::MouseClampingFilter(
    protocol::InputStub* input_stub)
    : input_filter_(input_stub),
      video_stub_(NULL) {
}

MouseClampingFilter::~MouseClampingFilter() {
}

void MouseClampingFilter::ProcessVideoPacket(
    scoped_ptr<VideoPacket> video_packet,
    const base::Closure& done) {
  // Configure the MouseInputFilter to clamp to the video dimensions.
  if (video_packet->format().has_screen_width() &&
      video_packet->format().has_screen_height()) {
    SkISize screen_size = SkISize::Make(video_packet->format().screen_width(),
                                        video_packet->format().screen_height());
    input_filter_.set_input_size(screen_size);
    input_filter_.set_output_size(screen_size);
  }

  video_stub_->ProcessVideoPacket(video_packet.Pass(), done);
}

}  // namespace remoting
