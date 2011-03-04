// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_row_based.h"

#include "base/logging.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/compressor_verbatim.h"
#include "remoting/base/compressor_zlib.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "ui/gfx/rect.h"

namespace remoting {

static const int kPacketSize = 1024 * 1024;

EncoderRowBased* EncoderRowBased::CreateZlibEncoder() {
  return new EncoderRowBased(new CompressorZlib(),
                             VideoPacketFormat::ENCODING_ZLIB);
}

EncoderRowBased* EncoderRowBased::CreateZlibEncoder(int packet_size) {
  return new EncoderRowBased(new CompressorZlib(),
                             VideoPacketFormat::ENCODING_ZLIB,
                             packet_size);
}

EncoderRowBased* EncoderRowBased::CreateVerbatimEncoder() {
  return new EncoderRowBased(new CompressorVerbatim(),
                             VideoPacketFormat::ENCODING_VERBATIM);
}

EncoderRowBased* EncoderRowBased::CreateVerbatimEncoder(int packet_size) {
  return new EncoderRowBased(new CompressorVerbatim(),
                             VideoPacketFormat::ENCODING_VERBATIM,
                             packet_size);
}

EncoderRowBased::EncoderRowBased(Compressor* compressor,
                                 VideoPacketFormat::Encoding encoding)
    : encoding_(encoding),
      compressor_(compressor),
      screen_width_(0),
      screen_height_(0),
      packet_size_(kPacketSize) {
}

EncoderRowBased::EncoderRowBased(Compressor* compressor,
                                 VideoPacketFormat::Encoding encoding,
                                 int packet_size)
    : encoding_(encoding),
      compressor_(compressor),
      screen_width_(0),
      screen_height_(0),
      packet_size_(packet_size) {
}

EncoderRowBased::~EncoderRowBased() {}

void EncoderRowBased::Encode(scoped_refptr<CaptureData> capture_data,
                             bool key_frame,
                             DataAvailableCallback* data_available_callback) {
  CHECK(capture_data->pixel_format() == media::VideoFrame::RGB32)
      << "RowBased Encoder only works with RGB32. Got "
      << capture_data->pixel_format();
  capture_data_ = capture_data;
  callback_.reset(data_available_callback);

  const InvalidRects& rects = capture_data->dirty_rects();
  for (InvalidRects::const_iterator r = rects.begin(); r != rects.end(); ++r) {
    EncodeRect(*r, r == --rects.end());
  }

  capture_data_ = NULL;
  callback_.reset();
}

void EncoderRowBased::EncodeRect(const gfx::Rect& rect, bool last) {
  CHECK(capture_data_->data_planes().data[0]);
  const int strides = capture_data_->data_planes().strides[0];
  const int bytes_per_pixel = GetBytesPerPixel(capture_data_->pixel_format());
  const int row_size = bytes_per_pixel * rect.width();

  compressor_->Reset();

  VideoPacket* packet = new VideoPacket();
  PrepareUpdateStart(rect, packet);
  const uint8* in = capture_data_->data_planes().data[0] +
      rect.y() * strides + rect.x() * bytes_per_pixel;
  // TODO(hclam): Fill in the sequence number.
  uint8* out = GetOutputBuffer(packet, packet_size_);
  int filled = 0;
  int row_pos = 0;  // Position in the current row in bytes.
  int row_y = 0;  // Current row.
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
      flush = Compressor::CompressorFinish;
    }

    int consumed = 0;
    int written = 0;
    compress_again = compressor_->Process(in + row_pos, row_size - row_pos,
                                          out + filled, packet_size_ - filled,
                                          flush, &consumed, &written);
    row_pos += consumed;
    filled += written;

    // We have reached the end of stream.
    if (!compress_again) {
      packet->set_flags(packet->flags() | VideoPacket::LAST_PACKET);
      if (last)
        packet->set_flags(packet->flags() | VideoPacket::LAST_PARTITION);
      DCHECK(row_pos == row_size);
      DCHECK(row_y == rect.height() - 1);
    }

    // If we have filled the message or we have reached the end of stream.
    if (filled == packet_size_ || !compress_again) {
      packet->mutable_data()->resize(filled);
      callback_->Run(packet);
      packet = NULL;
    }

    // Reached the end of input row and we're not at the last row.
    if (row_pos == row_size && row_y < rect.height() - 1) {
      row_pos = 0;
      in += strides;
      ++row_y;
    }
  }
}

void EncoderRowBased::PrepareUpdateStart(const gfx::Rect& rect,
                                         VideoPacket* packet) {
  packet->set_flags(packet->flags() | VideoPacket::FIRST_PACKET);

  VideoPacketFormat* format = packet->mutable_format();
  format->set_x(rect.x());
  format->set_y(rect.y());
  format->set_width(rect.width());
  format->set_height(rect.height());
  format->set_encoding(encoding_);
  if ((capture_data_->width() != screen_width_) ||
      (capture_data_->height() != screen_height_)) {
    screen_width_ = capture_data_->width();
    screen_height_ = capture_data_->height();
    format->set_screen_width(screen_width_);
    format->set_screen_height(screen_height_);
  }
}

uint8* EncoderRowBased::GetOutputBuffer(VideoPacket* packet, size_t size) {
  packet->mutable_data()->resize(size);
  // TODO(ajwong): Is there a better way to do this at all???
  return const_cast<uint8*>(reinterpret_cast<const uint8*>(
      packet->mutable_data()->data()));
}


}  // namespace remoting
