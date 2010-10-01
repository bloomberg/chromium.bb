// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_verbatim.h"

#include "remoting/base/protocol_util.h"

namespace remoting {

DecoderVerbatim::DecoderVerbatim()
    : state_(kWaitingForBeginRect),
      rect_x_(0),
      rect_y_(0),
      rect_width_(0),
      rect_height_(0),
      bytes_per_pixel_(0),
      updated_rects_(NULL),
      reverse_rows_(true) {
  encoding_ = EncodingNone;
}

DecoderVerbatim::~DecoderVerbatim() {}

bool DecoderVerbatim::BeginDecode(scoped_refptr<media::VideoFrame> frame,
                                  UpdatedRects* updated_rects,
                                  Task* partial_decode_done,
                                  Task* decode_done) {
  DCHECK(!partial_decode_done_.get());
  DCHECK(!decode_done_.get());
  DCHECK(!updated_rects_);
  DCHECK_EQ(kWaitingForBeginRect, state_);
  DCHECK(!started_);

  partial_decode_done_.reset(partial_decode_done);
  decode_done_.reset(decode_done);
  updated_rects_ = updated_rects;

  // TODO(hclam): Check if we can accept the color format of the video frame
  // and the codec.
  frame_ = frame;

  started_ = true;
  return true;
}

bool DecoderVerbatim::PartialDecode(ChromotingHostMessage* message) {
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

void DecoderVerbatim::EndDecode() {
  DCHECK_EQ(kWaitingForBeginRect, state_);
  DCHECK(started_);

  decode_done_->Run();

  partial_decode_done_.reset();
  decode_done_.reset();
  frame_ = NULL;
  updated_rects_ = NULL;
  started_ = false;
}

bool DecoderVerbatim::HandleBeginRect(ChromotingHostMessage* message) {
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
  return true;
}

bool DecoderVerbatim::HandleRectData(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForRectData, state_);
  DCHECK_EQ(0,
            message->update_stream_packet().rect_data().sequence_number());

  // Copy the data line by line.
  const int src_stride = bytes_per_pixel_ * rect_width_;
  const char* src =
      message->update_stream_packet().rect_data().data().data();

  // Make sure there's enough data in |message|.
  const int src_size =
      message->update_stream_packet().rect_data().data().length();
  if (src_size != src_stride * rect_height_)
    return false;

  // Make sure there's enough space for us to write to.
  if (frame_->height() < static_cast<size_t>(rect_height_) ||
      frame_->stride(media::VideoFrame::kRGBPlane) < src_stride) {
    return false;
  }

  int src_stride_dir = src_stride;
  if (reverse_rows_) {
    // Copy rows from bottom to top to flip the image vertically.
    src += (rect_height_ - 1) * src_stride;
    // Change the direction of the stride to work bottom to top.
    src_stride_dir *= -1;
  }
  const int dest_stride = frame_->stride(media::VideoFrame::kRGBPlane);
  uint8* dest = frame_->data(media::VideoFrame::kRGBPlane) +
      dest_stride * rect_y_ + bytes_per_pixel_ * rect_x_;
  for (int i = 0; i < rect_height_; ++i) {
    memcpy(dest, src, src_stride);
    dest += dest_stride;
    src += src_stride_dir;
  }

  updated_rects_->clear();
  updated_rects_->push_back(gfx::Rect(rect_x_, rect_y_,
                                      rect_width_, rect_height_));
  partial_decode_done_->Run();
  return true;
}

bool DecoderVerbatim::HandleEndRect(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForRectData, state_);
  state_ = kWaitingForBeginRect;
  return true;
}

}  // namespace remoting
