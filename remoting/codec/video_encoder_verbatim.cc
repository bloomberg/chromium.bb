// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_verbatim.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

VideoEncoderVerbatim::VideoEncoderVerbatim() {}
VideoEncoderVerbatim::~VideoEncoderVerbatim() {}

scoped_ptr<VideoPacket> VideoEncoderVerbatim::Encode(
    const webrtc::DesktopFrame& frame) {
  CHECK(frame.data());

  base::Time encode_start_time = base::Time::Now();
  scoped_ptr<VideoPacket> packet(new VideoPacket());

  VideoPacketFormat* format = packet->mutable_format();
  format->set_encoding(VideoPacketFormat::ENCODING_VERBATIM);
  if (!frame.size().equals(screen_size_)) {
    screen_size_ = frame.size();
    format->set_screen_width(screen_size_.width());
    format->set_screen_height(screen_size_.height());
  }

  // Calculate output size.
  size_t output_size = 0;
  for (webrtc::DesktopRegion::Iterator iter(frame.updated_region());
       !iter.IsAtEnd(); iter.Advance()) {
    const webrtc::DesktopRect& rect = iter.rect();
    output_size += rect.width() * rect.height() *
        webrtc::DesktopFrame::kBytesPerPixel;
  }

  uint8_t* out = GetOutputBuffer(packet.get(), output_size);
  const int in_stride = frame.stride();

  // Store all changed rectangles in the packet.
  for (webrtc::DesktopRegion::Iterator iter(frame.updated_region());
       !iter.IsAtEnd(); iter.Advance()) {
    const webrtc::DesktopRect& rect = iter.rect();
    const int row_size = webrtc::DesktopFrame::kBytesPerPixel * rect.width();
    const uint8_t* in = frame.data() + rect.top() * in_stride +
                        rect.left() * webrtc::DesktopFrame::kBytesPerPixel;
    for (int y = rect.top(); y < rect.top() + rect.height(); ++y) {
      memcpy(out, in, row_size);
      out += row_size;
      in += in_stride;
    }

    Rect* dirty_rect = packet->add_dirty_rects();
    dirty_rect->set_x(rect.left());
    dirty_rect->set_y(rect.top());
    dirty_rect->set_width(rect.width());
    dirty_rect->set_height(rect.height());
  }

  packet->set_capture_time_ms(frame.capture_time_ms());
  packet->set_encode_time_ms(
      (base::Time::Now() - encode_start_time).InMillisecondsRoundedUp());
  if (!frame.dpi().is_zero()) {
    packet->mutable_format()->set_x_dpi(frame.dpi().x());
    packet->mutable_format()->set_y_dpi(frame.dpi().y());
  }

  return packet.Pass();
}

uint8_t* VideoEncoderVerbatim::GetOutputBuffer(VideoPacket* packet,
                                               size_t size) {
  packet->mutable_data()->resize(size);
  return reinterpret_cast<uint8_t*>(string_as_array(packet->mutable_data()));
}

}  // namespace remoting
