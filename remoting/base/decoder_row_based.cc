// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_row_based.h"

#include "remoting/base/decompressor.h"
#include "remoting/base/decompressor_zlib.h"
#include "remoting/base/decompressor_verbatim.h"
#include "remoting/base/util.h"

namespace remoting {

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
      bytes_per_src_pixel_(0),
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
  return state_ == kReady;
}

void DecoderRowBased::Initialize(scoped_refptr<media::VideoFrame> frame,
                                 const gfx::Rect& clip,
                                 int bytes_per_src_pixel) {
  // Make sure we are not currently initialized.
  CHECK_EQ(kUninitialized, state_);

  if (static_cast<PixelFormat>(frame->format()) != PIXEL_FORMAT_RGB32) {
    LOG(WARNING) << "DecoderRowBased only supports RGB32.";
    state_ = kError;
    return;
  }

  frame_ = frame;

  // Reset the buffer location status variables.
  clip_ = clip;
  row_pos_ = 0;
  row_y_ = 0;
  bytes_per_src_pixel_ = bytes_per_src_pixel;

  state_ = kReady;
}

void DecoderRowBased::DecodeBytes(const std::string& encoded_bytes) {
  DCHECK_EQ(kReady, state_);

  const uint8* in = reinterpret_cast<const uint8*>(encoded_bytes.data());
  const int in_size = encoded_bytes.size();
  const int row_size = clip_.width() * bytes_per_src_pixel_;
  int stride = frame_->stride(media::VideoFrame::kRGBPlane);
  uint8* rect_begin = frame_->data(media::VideoFrame::kRGBPlane);

  if (reverse_rows_) {
    // Advance the pointer to the last row.
    rect_begin += (frame_->height() - 1) * stride;

    // And then make the stride negative.
    stride = -stride;
  }

  // TODO(ajwong): This should be bytes_per_dst_pixel shouldn't this.?
  uint8* out = rect_begin +
      stride * (clip_.y() + row_y_) +
      bytes_per_src_pixel_ * clip_.x();

  // Consume all the data in the message.
  bool decompress_again = true;
  int used = 0;
  while (decompress_again && used < in_size) {
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
}

}  // namespace remoting
