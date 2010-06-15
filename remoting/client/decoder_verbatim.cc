// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/decoder_verbatim.h"

namespace remoting {

DecoderVerbatim::DecoderVerbatim()
    : updated_rects_(NULL) {
}

bool DecoderVerbatim::BeginDecode(scoped_refptr<media::VideoFrame> frame,
                                  UpdatedRects* updated_rects,
                                  Task* partial_decode_done,
                                  Task* decode_done) {
  DCHECK(!partial_decode_done_.get());
  DCHECK(!decode_done_.get());
  DCHECK(!updated_rects_);

  partial_decode_done_.reset(partial_decode_done);
  decode_done_.reset(decode_done);
  updated_rects_ = updated_rects;

  // TODO(hclam): Check if we can accept the color format of the video frame
  // and the codec.
  frame_ = frame;
  return true;
}

bool DecoderVerbatim::PartialDecode(HostMessage* message) {
  scoped_ptr<HostMessage> msg_deleter(message);

  int width = message->update_stream_packet().header().width();
  int height = message->update_stream_packet().header().height();
  int x = message->update_stream_packet().header().x();
  int y = message->update_stream_packet().header().y();
  PixelFormat pixel_format =
      message->update_stream_packet().header().pixel_format();
  int bytes_per_pixel = 0;

  // TODO(hclam): Extract the following to an util function.
  if (pixel_format == PixelFormatRgb24) {
    bytes_per_pixel = 3;
  } else if (pixel_format == PixelFormatRgb565) {
    bytes_per_pixel = 2;
  } else if (pixel_format == PixelFormatRgb32) {
    bytes_per_pixel = 4;
  } else if (pixel_format == PixelFormatAscii) {
    bytes_per_pixel = 1;
  } else {
    NOTREACHED() << "Pixel format not supported";
  }

  if (static_cast<PixelFormat>(frame_->format()) != pixel_format) {
    NOTREACHED() << "Pixel format of message doesn't match the video frame. "
                    "Expected vs received = "
                 << frame_->format() << " vs " << pixel_format
                 << " Color space conversion required.";
  }

  // Copy the data line by line.
  const int src_stride = bytes_per_pixel * width;
  const char* src = message->update_stream_packet().data().c_str();
  const int dest_stride = frame_->stride(media::VideoFrame::kRGBPlane);
  uint8* dest = frame_->data(media::VideoFrame::kRGBPlane) +
      dest_stride * y + bytes_per_pixel * x;
  for (int i = 0; i < height; ++i) {
    memcpy(dest, src, src_stride);
    dest += dest_stride;
    src += src_stride;
  }

  updated_rects_->clear();
  updated_rects_->push_back(gfx::Rect(x, y, width, height));
  partial_decode_done_->Run();
  return true;
}

void DecoderVerbatim::EndDecode() {
  decode_done_->Run();

  partial_decode_done_.reset();
  decode_done_.reset();
  frame_ = NULL;
  updated_rects_ = NULL;
}

}  // namespace remoting
