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
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

static uint8_t* GetPacketOutputBuffer(VideoPacket* packet, size_t size) {
  packet->mutable_data()->resize(size);
  return reinterpret_cast<uint8_t*>(string_as_array(packet->mutable_data()));
}

VideoEncoderVerbatim::VideoEncoderVerbatim() {}
VideoEncoderVerbatim::~VideoEncoderVerbatim() {}

scoped_ptr<VideoPacket> VideoEncoderVerbatim::Encode(
    const webrtc::DesktopFrame& frame) {
  CHECK(frame.data());

  base::Time encode_start_time = base::Time::Now();

  // Create a VideoPacket with common fields (e.g. DPI, rects, shape) set.
  scoped_ptr<VideoPacket> packet(helper_.CreateVideoPacket(frame));
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VERBATIM);

  // Calculate output size.
  size_t output_size = 0;
  for (webrtc::DesktopRegion::Iterator iter(frame.updated_region());
       !iter.IsAtEnd(); iter.Advance()) {
    const webrtc::DesktopRect& rect = iter.rect();
    output_size += rect.width() * rect.height() *
        webrtc::DesktopFrame::kBytesPerPixel;
  }

  uint8_t* out = GetPacketOutputBuffer(packet.get(), output_size);
  const int in_stride = frame.stride();

  // Encode pixel data for all changed rectangles into the packet.
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
  }

  // Note the time taken to encode the pixel data.
  packet->set_encode_time_ms(
      (base::Time::Now() - encode_start_time).InMillisecondsRoundedUp());

  return packet.Pass();
}

}  // namespace remoting
