// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/encoder_verbatim.h"

#include "gfx/rect.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

using media::DataBuffer;

void EncoderVerbatim::Encode(const DirtyRects& dirty_rects,
                             const uint8* const* input_data,
                             const int* strides,
                             bool key_frame,
                             DataAvailableCallback* data_available_callback) {
  int num_rects = dirty_rects.size();
  for (int i = 0; i < num_rects; i++) {
    scoped_refptr<DataBuffer> data;
    gfx::Rect dirty_rect = dirty_rects[i];
    PixelFormat pixel_format;
    UpdateStreamEncoding encoding;
    scoped_ptr<UpdateStreamPacketHeader> header(new UpdateStreamPacketHeader);
    if (EncodeRect(dirty_rect,
                   input_data,
                   strides,
                   header.get(),
                   &data)) {
      EncodingState state = EncodingInProgress;
      if (i == 0) {
        state |= EncodingStarting;
      } else if (i == num_rects - 1) {
        state |= EncodingEnded;
      }
      data_available_callback->Run(header.release(),
                                   data,
                                   state);
    }
  }

  delete data_available_callback;
}

void EncoderVerbatim::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
}

void EncoderVerbatim::SetPixelFormat(PixelFormat pixel_format) {
  // These are sorted so that the most common formats are checked first.
  // TODO(hclam): Extract this into a util function.
  if (pixel_format == PixelFormatRgb24) {
    bytes_per_pixel_ = 3;
  } else if (pixel_format == PixelFormatRgb565) {
    bytes_per_pixel_ = 2;
  } else if (pixel_format == PixelFormatRgb32) {
    bytes_per_pixel_ = 4;
  } else if (pixel_format != PixelFormatAscii) {
    bytes_per_pixel_ = 1;
  } else {
    NOTREACHED() << "Pixel format not supported";
  }
  pixel_format_ = pixel_format;
}

bool EncoderVerbatim::EncodeRect(const gfx::Rect& dirty,
                                 const uint8* const* input_data,
                                 const int* strides,
                                 UpdateStreamPacketHeader *header,
                                 scoped_refptr<DataBuffer>* output_data) {
  const int kPlanes = 3;

  // Calculate the size of output.
  int output_size = 0;
  for (int i = 0; i < kPlanes; ++i) {
    // TODO(hclam): Handle YUV since the height would be different.
    output_size += strides[i] * height_;
  }

  header->set_x(dirty.x());
  header->set_y(dirty.y());
  header->set_width(dirty.width());
  header->set_height(dirty.height());
  header->set_encoding(EncodingNone);
  header->set_pixel_format(pixel_format_);

  *output_data = new DataBuffer(new uint8[output_size], output_size);
  uint8* out = (*output_data)->GetWritableData();

  for (int i = 0; i < kPlanes; ++i) {
    const uint8* in = input_data[i];
    // Skip over planes that don't have data.
    if (!in)
      continue;

    // TODO(hclam): Handle YUV since the height would be different.
    for (int j = 0; j < height_; ++j) {
      int row_size = width_ * bytes_per_pixel_;
      DCHECK_LE(row_size, strides[i]);
      memcpy(out, in, row_size);
      in += strides[i];
      out += row_size;
    }
  }
  return true;
}

}  // namespace remoting
