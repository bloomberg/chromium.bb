// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_verbatim.h"

#include "base/logging.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"

namespace remoting {

static const int kPacketSize = 1024 * 1024;

VideoEncoderVerbatim::VideoEncoderVerbatim()
    : screen_size_(SkISize::Make(0,0)),
      max_packet_size_(kPacketSize) {
}

void VideoEncoderVerbatim::SetMaxPacketSize(int size) {
  max_packet_size_ = size;
}

VideoEncoderVerbatim::~VideoEncoderVerbatim() {
}

void VideoEncoderVerbatim::Encode(
    scoped_refptr<media::ScreenCaptureData> capture_data,
    bool key_frame,
    const DataAvailableCallback& data_available_callback) {
  capture_data_ = capture_data;
  callback_ = data_available_callback;
  encode_start_time_ = base::Time::Now();

  const SkRegion& region = capture_data->dirty_region();
  SkRegion::Iterator iter(region);
  while (!iter.done()) {
    SkIRect rect = iter.rect();
    iter.next();
    EncodeRect(rect, iter.done());
  }

  capture_data_ = NULL;
  callback_.Reset();
}

void VideoEncoderVerbatim::EncodeRect(const SkIRect& rect, bool last) {
  CHECK(capture_data_->data());
  const int stride = capture_data_->stride();
  const int bytes_per_pixel = 4;
  const int row_size = bytes_per_pixel * rect.width();

  scoped_ptr<VideoPacket> packet(new VideoPacket());
  PrepareUpdateStart(rect, packet.get());
  const uint8* in = capture_data_->data() +
      rect.fTop * stride + rect.fLeft * bytes_per_pixel;
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
      packet->set_capture_time_ms(capture_data_->capture_time_ms());
      packet->set_encode_time_ms(
          (base::Time::Now() - encode_start_time_).InMillisecondsRoundedUp());
      packet->set_client_sequence_number(
          capture_data_->client_sequence_number());
      SkIPoint dpi(capture_data_->dpi());
      if (dpi.x())
        packet->mutable_format()->set_x_dpi(dpi.x());
      if (dpi.y())
        packet->mutable_format()->set_y_dpi(dpi.y());
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

void VideoEncoderVerbatim::PrepareUpdateStart(const SkIRect& rect,
                                              VideoPacket* packet) {
  packet->set_flags(packet->flags() | VideoPacket::FIRST_PACKET);

  VideoPacketFormat* format = packet->mutable_format();
  format->set_x(rect.fLeft);
  format->set_y(rect.fTop);
  format->set_width(rect.width());
  format->set_height(rect.height());
  format->set_encoding(VideoPacketFormat::ENCODING_VERBATIM);
  if (capture_data_->size() != screen_size_) {
    screen_size_ = capture_data_->size();
    format->set_screen_width(screen_size_.width());
    format->set_screen_height(screen_size_.height());
  }
}

uint8* VideoEncoderVerbatim::GetOutputBuffer(VideoPacket* packet, size_t size) {
  packet->mutable_data()->resize(size);
  // TODO(ajwong): Is there a better way to do this at all???
  return const_cast<uint8*>(reinterpret_cast<const uint8*>(
      packet->mutable_data()->data()));
}

}  // namespace remoting
