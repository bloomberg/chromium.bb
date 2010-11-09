// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_zlib.h"

#include "base/logging.h"
#include "gfx/rect.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/compressor_zlib.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"

namespace remoting {

static const int kPacketSize = 1024 * 1024;

EncoderZlib::EncoderZlib() : packet_size_(kPacketSize) {
}

EncoderZlib::EncoderZlib(int packet_size) : packet_size_(packet_size) {
}

EncoderZlib::~EncoderZlib() {}

void EncoderZlib::Encode(scoped_refptr<CaptureData> capture_data,
                         bool key_frame,
                         DataAvailableCallback* data_available_callback) {
  CHECK(capture_data->pixel_format() == media::VideoFrame::RGB32)
      << "Zlib Encoder only works with RGB32. Got "
      << capture_data->pixel_format();
  capture_data_ = capture_data;
  callback_.reset(data_available_callback);

  CompressorZlib compressor;
  const InvalidRects& rects = capture_data->dirty_rects();
  int index = 0;
  for (InvalidRects::const_iterator r = rects.begin();
      r != rects.end(); ++r, ++index) {
    EncodeRect(&compressor, *r, index);
  }

  capture_data_ = NULL;
  callback_.reset();
}

void EncoderZlib::EncodeRect(CompressorZlib* compressor,
                             const gfx::Rect& rect, size_t rect_index) {
  CHECK(capture_data_->data_planes().data[0]);
  const int strides = capture_data_->data_planes().strides[0];
  const int bytes_per_pixel = GetBytesPerPixel(capture_data_->pixel_format());
  const int row_size = bytes_per_pixel * rect.width();

  VideoPacket* packet = new VideoPacket();
  PrepareUpdateStart(rect, packet);
  const uint8* in = capture_data_->data_planes().data[0] +
                    rect.y() * strides +
                    rect.x() * bytes_per_pixel;
  // TODO(hclam): Fill in the sequence number.
  uint8* out = GetOutputBuffer(packet, packet_size_);
  int filled = 0;
  int row_x = 0;
  int row_y = 0;
  bool compress_again = true;
  while (compress_again) {
    // Prepare a message for sending out.
    if (!packet) {
      packet = new VideoPacket();
      out = GetOutputBuffer(packet, packet_size_);
      filled = 0;
    }

    Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;
    if (row_y == rect.height() - 1) {
      if (rect_index == capture_data_->dirty_rects().size() - 1) {
        flush = Compressor::CompressorFinish;
      } else {
        flush = Compressor::CompressorSyncFlush;
      }
    }

    int consumed = 0;
    int written = 0;
    compress_again = compressor->Process(in + row_x, row_size - row_x,
                                         out + filled, packet_size_ - filled,
                                         flush, &consumed, &written);
    row_x += consumed;
    filled += written;

    // We have reached the end of stream.
    if (!compress_again) {
      packet->set_flags(packet->flags() | VideoPacket::LAST_PACKET);
    }

    // If we have filled the message or we have reached the end of stream.
    if (filled == packet_size_ || !compress_again) {
      packet->mutable_data()->resize(filled);
      SubmitMessage(packet, rect_index);
      packet = NULL;
    }

    // Reached the end of input row and we're not at the last row.
    if (row_x == row_size && row_y < rect.height() - 1) {
      row_x = 0;
      in += strides;
      ++row_y;
    }
  }
}

void EncoderZlib::PrepareUpdateStart(const gfx::Rect& rect,
                                     VideoPacket* packet) {
  packet->set_flags(packet->flags() | VideoPacket::FIRST_PACKET);
  VideoPacketFormat* format = packet->mutable_format();

  format->set_x(rect.x());
  format->set_y(rect.y());
  format->set_width(rect.width());
  format->set_height(rect.height());
  format->set_encoding(VideoPacketFormat::ENCODING_ZLIB);
}

uint8* EncoderZlib::GetOutputBuffer(VideoPacket* packet, size_t size) {
  packet->mutable_data()->resize(size);
  // TODO(ajwong): Is there a better way to do this at all???
  return const_cast<uint8*>(reinterpret_cast<const uint8*>(
      packet->mutable_data()->data()));
}

void EncoderZlib::SubmitMessage(VideoPacket* packet, size_t rect_index) {
  callback_->Run(packet);
}

}  // namespace remoting
