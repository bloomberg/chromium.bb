// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_row_based.h"

#include "remoting/base/decompressor.h"
#include "remoting/base/decompressor_zlib.h"
#include "remoting/base/decompressor_verbatim.h"
#include "remoting/base/util.h"

namespace remoting {

namespace {
// Both input and output data are assumed to be RGBA32.
const int kBytesPerPixel = 4;
}

DecoderRowBased* DecoderRowBased::CreateZlibDecoder() {
  return new DecoderRowBased(new DecompressorZlib(),
                             VideoPacketFormat::ENCODING_ZLIB);
}

DecoderRowBased* DecoderRowBased::CreateVerbatimDecoder() {
  return new DecoderRowBased(new DecompressorVerbatim(),
                             VideoPacketFormat::ENCODING_VERBATIM);
}

DecoderRowBased::DecoderRowBased(Decompressor* decompressor,
                                 VideoPacketFormat::Encoding encoding)
    : state_(kUninitialized),
      decompressor_(decompressor),
      encoding_(encoding),
      row_pos_(0),
      row_y_(0),
      // TODO(hclam): We should use the information from the update stream
      // to determine whether we should reverse the rows or not.
      // But for simplicity we set to be always true.
      reverse_rows_(true) {
}

DecoderRowBased::~DecoderRowBased() {
}

void DecoderRowBased::Reset() {
  frame_ = NULL;
  decompressor_->Reset();
  state_ = kUninitialized;
}

bool DecoderRowBased::IsReadyForData() {
  return state_ == kReady || state_ == kProcessing || state_ == kDone;
}

void DecoderRowBased::Initialize(scoped_refptr<media::VideoFrame> frame) {
  // Make sure we are not currently initialized.
  CHECK_EQ(kUninitialized, state_);

  if (frame->format() != media::VideoFrame::RGB32) {
    LOG(WARNING) << "DecoderRowBased only supports RGB32.";
    state_ = kError;
    return;
  }

  frame_ = frame;
  state_ = kReady;
}

Decoder::DecodeResult DecoderRowBased::DecodePacket(
    const VideoPacket* packet) {
  UpdateStateForPacket(packet);

  if (state_ == kError) {
    return DECODE_ERROR;
  }

  const uint8* in = reinterpret_cast<const uint8*>(packet->data().data());
  const int in_size = packet->data().size();

  const int row_size = clip_.width() * kBytesPerPixel;
  int stride = frame_->stride(media::VideoFrame::kRGBPlane);
  uint8* rect_begin = frame_->data(media::VideoFrame::kRGBPlane);

  if (reverse_rows_) {
    // Advance the pointer to the last row.
    rect_begin += (frame_->height() - 1) * stride;

    // And then make the stride negative.
    stride = -stride;
  }

  uint8* out = rect_begin + stride * (clip_.y() + row_y_) +
      kBytesPerPixel * clip_.x();

  // Consume all the data in the message.
  bool decompress_again = true;
  int used = 0;
  while (decompress_again && used < in_size) {
    if (row_y_ >= clip_.height()) {
      state_ = kError;
      LOG(WARNING) << "Too much data is received for the given rectangle.";
      return DECODE_ERROR;
    }

    int written = 0;
    int consumed = 0;
    // TODO(ajwong): This assume source and dest stride are the same, which is
    // incorrect.
    decompress_again = decompressor_->Process(
        in + used, in_size - used, out + row_pos_, row_size - row_pos_,
        &consumed, &written);
    used += consumed;
    row_pos_ += written;

    // If this row is completely filled then move onto the next row.
    if (row_pos_ == row_size) {
      ++row_y_;
      row_pos_ = 0;
      out += stride;
    }
  }

  if (state_ == kDone && row_y_ < clip_.height()) {
    state_ = kError;
    LOG(WARNING) << "Received LAST_PACKET, but didn't get enough data.";
    return DECODE_ERROR;
  }

  return state_ == kDone ? DECODE_DONE : DECODE_IN_PROGRESS;
}

void DecoderRowBased::UpdateStateForPacket(const VideoPacket* packet) {
  if (state_ == kError) {
    return;
  }

  if (packet->flags() & VideoPacket::FIRST_PACKET) {
    if (state_ != kReady && state_ != kDone) {
      state_ = kError;
      LOG(WARNING) << "Received unexpected FIRST_PACKET.";
      return;
    }
    state_ = kProcessing;

    // Reset the buffer location status variables on the first packet.
    clip_.SetRect(packet->format().x(), packet->format().y(),
                  packet->format().width(), packet->format().height());
    row_pos_ = 0;
    row_y_ = 0;
  }

  if (state_ != kProcessing) {
    state_ = kError;
    LOG(WARNING) << "Received unexpected packet.";
    return;
  }

  if (packet->flags() & VideoPacket::LAST_PACKET) {
    if (state_ != kProcessing) {
      state_ = kError;
      LOG(WARNING) << "Received unexpected LAST_PACKET.";
      return;
    }
    state_ = kDone;
  }

  return;
}

void DecoderRowBased::GetUpdatedRects(UpdatedRects* rects) {
  rects->push_back(clip_);
}

VideoPacketFormat::Encoding DecoderRowBased::Encoding() {
  return encoding_;
}

}  // namespace remoting
