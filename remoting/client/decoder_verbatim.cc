// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/decoder_verbatim.h"

namespace remoting {

bool DecoderVerbatim::BeginDecode(scoped_refptr<media::VideoFrame> frame) {
  // TODO(hclam): Check if we can accept the codec.
  frame_ = frame;
  return true;
}

bool DecoderVerbatim::PartialDecode(chromotocol_pb::HostMessage* message) {
  scoped_ptr<chromotocol_pb::HostMessage> msg_deleter(message);

  // TODO(hclam): Support YUV.
  if (static_cast<int>(message->update_stream_packet().header().pixel_format())
      != static_cast<int>(frame_->format())) {
    return false;
  }
  int width = message->update_stream_packet().header().width();
  int height = message->update_stream_packet().header().height();
  int x = message->update_stream_packet().header().x();
  int y = message->update_stream_packet().header().y();
  chromotocol_pb::PixelFormat pixel_format =
      message->update_stream_packet().header().pixel_format();
  int bytes_per_pixel = 0;

  // TODO(hclam): Extract the following to an util function.
  if (pixel_format == chromotocol_pb::PixelFormatRgb24) {
    bytes_per_pixel = 3;
  } else if (pixel_format == chromotocol_pb::PixelFormatRgb565) {
    bytes_per_pixel = 2;
  } else if (pixel_format == chromotocol_pb::PixelFormatRgb32) {
    bytes_per_pixel = 4;
  } else if (pixel_format != chromotocol_pb::PixelFormatAscii) {
    bytes_per_pixel = 1;
  } else {
    NOTREACHED() << "Pixel format not supported";
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

  UpdatedRects rects;
  rects.push_back(gfx::Rect(x, y, width, height));
  partial_decode_done()->Run(frame_, rects);
  return true;
}

void DecoderVerbatim::EndDecode() {
  decode_done()->Run(frame_);
  frame_ = NULL;
}

}  // namespace remoting
