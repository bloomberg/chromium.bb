// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_zlib.h"

#include "remoting/base/decompressor_zlib.h"
#include "remoting/base/protocol_util.h"

namespace remoting {

DecoderZlib::DecoderZlib()
    : state_(kWaitingForBeginRect),
      rect_x_(0),
      rect_y_(0),
      rect_width_(0),
      rect_height_(0),
      bytes_per_pixel_(0),
      updated_rects_(NULL),
      row_pos_(0),
      row_y_(0),
      // TODO(hclam): We should use the information from the update stream
      // to determine whether we should reverse the rows or not.
      // But for simplicity we set to be always true.
      reverse_rows_(true) {
  encoding_ = EncodingZlib;
}

DecoderZlib::~DecoderZlib() {
}

bool DecoderZlib::BeginDecode(scoped_refptr<media::VideoFrame> frame,
                              UpdatedRects* updated_rects,
                              Task* partial_decode_done,
                              Task* decode_done) {
  DCHECK(!partial_decode_done_.get());
  DCHECK(!decode_done_.get());
  DCHECK(!updated_rects_);
  DCHECK_EQ(kWaitingForBeginRect, state_);
  DCHECK(!started_);

  if (static_cast<PixelFormat>(frame->format()) != PixelFormatRgb32) {
    LOG(INFO) << "DecoderZlib only supports RGB32.";
    return false;
  }

  partial_decode_done_.reset(partial_decode_done);
  decode_done_.reset(decode_done);
  updated_rects_ = updated_rects;
  frame_ = frame;

  // Create the decompressor.
  decompressor_.reset(new DecompressorZlib());

  started_ = true;
  return true;
}

bool DecoderZlib::PartialDecode(ChromotingHostMessage* message) {
  scoped_ptr<ChromotingHostMessage> msg_deleter(message);
  DCHECK(message->has_update_stream_packet());
  DCHECK(started_);

  bool ret = true;
  if (message->update_stream_packet().has_begin_rect())
    ret = HandleBeginRect(message);
  if (ret && message->update_stream_packet().has_rect_data())
    ret = HandleRectData(message);
  if (ret && message->update_stream_packet().has_end_rect())
    ret = HandleEndRect(message);
  return ret;
}

void DecoderZlib::EndDecode() {
  DCHECK_EQ(kWaitingForBeginRect, state_);
  DCHECK(started_);

  decode_done_->Run();

  partial_decode_done_.reset();
  decode_done_.reset();
  updated_rects_ = NULL;
  frame_ = NULL;
  decompressor_.reset();
  started_ = false;
}

bool DecoderZlib::HandleBeginRect(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForBeginRect, state_);
  state_ = kWaitingForRectData;

  rect_width_ = message->update_stream_packet().begin_rect().width();
  rect_height_ = message->update_stream_packet().begin_rect().height();
  rect_x_ = message->update_stream_packet().begin_rect().x();
  rect_y_ = message->update_stream_packet().begin_rect().y();

  PixelFormat pixel_format =
      message->update_stream_packet().begin_rect().pixel_format();

  if (static_cast<PixelFormat>(frame_->format()) != pixel_format) {
    NOTREACHED() << "Pixel format of message doesn't match the video frame. "
                    "Expected vs received = "
                 << frame_->format() << " vs " << pixel_format
                 << " Color space conversion required.";
    return false;
  }

  bytes_per_pixel_ = GetBytesPerPixel(pixel_format);
  row_pos_ = 0;
  row_y_ = 0;
  return true;
}

bool DecoderZlib::HandleRectData(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForRectData, state_);
  DCHECK_EQ(0,
            message->update_stream_packet().rect_data().sequence_number());

  const uint8* in =
      (const uint8*)message->update_stream_packet().rect_data().data().data();
  const int in_size =
      message->update_stream_packet().rect_data().data().size();
  const int row_size = rect_width_ * bytes_per_pixel_;
  int stride = frame_->stride(media::VideoFrame::kRGBPlane);
  uint8* rect_begin = frame_->data(media::VideoFrame::kRGBPlane);
  if (reverse_rows_) {
    // Advance the pointer to the last row.
    rect_begin += (frame_->height() - 1) * stride;

    // And then make the stride negative.
    stride = -stride;
  }

  uint8* out = rect_begin + stride * (rect_y_ + row_y_) +
      bytes_per_pixel_ * rect_x_;

  // Consume all the data in the message.
  bool decompress_again = true;
  int used = 0;
  while (decompress_again && used < in_size) {
    int written = 0;
    int consumed = 0;
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
  return true;
}

bool DecoderZlib::HandleEndRect(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForRectData, state_);
  state_ = kWaitingForBeginRect;

  updated_rects_->clear();
  updated_rects_->push_back(gfx::Rect(rect_x_, rect_y_,
                                      rect_width_, rect_height_));
  partial_decode_done_->Run();
  return true;
}

}  // namespace remoting
