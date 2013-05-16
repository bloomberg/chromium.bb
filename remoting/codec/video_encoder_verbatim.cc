// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_verbatim.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

static const int kPacketSize = 1024 * 1024;

VideoEncoderVerbatim::VideoEncoderVerbatim()
    : max_packet_size_(kPacketSize) {
}

void VideoEncoderVerbatim::SetMaxPacketSize(int size) {
  max_packet_size_ = size;
}

VideoEncoderVerbatim::~VideoEncoderVerbatim() {
}

void VideoEncoderVerbatim::Encode(
    const webrtc::DesktopFrame* frame,
    const DataAvailableCallback& data_available_callback) {
  callback_ = data_available_callback;
  encode_start_time_ = base::Time::Now();

  webrtc::DesktopRegion::Iterator iter(frame->updated_region());
  while (!iter.IsAtEnd()) {
    const webrtc::DesktopRect& rect = iter.rect();
    iter.Advance();
    EncodeRect(frame, rect, iter.IsAtEnd());
  }

  callback_.Reset();
}

void VideoEncoderVerbatim::EncodeRect(const webrtc::DesktopFrame* frame,
                                      const webrtc::DesktopRect& rect,
                                      bool last) {
  CHECK(frame->data());
  const int stride = frame->stride();
  const int bytes_per_pixel = 4;
  const int row_size = bytes_per_pixel * rect.width();

  scoped_ptr<VideoPacket> packet(new VideoPacket());
  PrepareUpdateStart(frame, rect, packet.get());
  const uint8* in = frame->data() +
      rect.top() * stride + rect.left() * bytes_per_pixel;
  // TODO(hclam): Fill in the sequence number.
  uint8* out = GetOutputBuffer(packet.get(), max_packet_size_);
  int filled = 0;
  int row_pos = 0;  // Position in the current row in bytes.
  int row_y = 0;  // Current row.
  while (row_y < rect.height()) {
    // Prepare a message for sending out.
    if (!packet.get()) {
      packet.reset(new VideoPacket());
      out = GetOutputBuffer(packet.get(), max_packet_size_);
      filled = 0;
    }

    if (row_y < rect.height()) {
      int bytes_to_copy =
          std::min(row_size - row_pos, max_packet_size_ - filled);
      memcpy(out + filled, in + row_pos, bytes_to_copy);
      row_pos += bytes_to_copy;
      filled += bytes_to_copy;

      // Jump to the next row when we've reached the end of the current row.
      if (row_pos == row_size) {
        row_pos = 0;
        in += stride;
        ++row_y;
      }
    }

    if (row_y == rect.height()) {
      DCHECK_EQ(row_pos, 0);

      packet->mutable_data()->resize(filled);
      packet->set_flags(packet->flags() | VideoPacket::LAST_PACKET);

      packet->set_capture_time_ms(frame->capture_time_ms());
      packet->set_encode_time_ms(
          (base::Time::Now() - encode_start_time_).InMillisecondsRoundedUp());
      if (!frame->dpi().is_zero()) {
        packet->mutable_format()->set_x_dpi(frame->dpi().x());
        packet->mutable_format()->set_y_dpi(frame->dpi().y());
      }
      if (last)
        packet->set_flags(packet->flags() | VideoPacket::LAST_PARTITION);
    }

    // If we have filled the current packet, then send it.
    if (filled == max_packet_size_ || row_y == rect.height()) {
      packet->mutable_data()->resize(filled);
      callback_.Run(packet.Pass());
    }
  }
}

void VideoEncoderVerbatim::PrepareUpdateStart(const webrtc::DesktopFrame* frame,
                                              const webrtc::DesktopRect& rect,
                                              VideoPacket* packet) {
  packet->set_flags(packet->flags() | VideoPacket::FIRST_PACKET);

  VideoPacketFormat* format = packet->mutable_format();
  format->set_x(rect.left());
  format->set_y(rect.top());
  format->set_width(rect.width());
  format->set_height(rect.height());
  format->set_encoding(VideoPacketFormat::ENCODING_VERBATIM);
  if (frame->size().equals(screen_size_)) {
    screen_size_ = frame->size();
    format->set_screen_width(screen_size_.width());
    format->set_screen_height(screen_size_.height());
  }
}

uint8* VideoEncoderVerbatim::GetOutputBuffer(VideoPacket* packet, size_t size) {
  packet->mutable_data()->resize(size);
  return reinterpret_cast<uint8*>(string_as_array(packet->mutable_data()));
}

}  // namespace remoting
