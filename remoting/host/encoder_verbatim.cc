// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/encoder_verbatim.h"

#include "gfx/rect.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol_util.h"

namespace remoting {

using media::DataBuffer;

void EncoderVerbatim::Encode(scoped_refptr<Capturer::CaptureData> capture_data,
                             bool key_frame,
                             DataAvailableCallback* data_available_callback) {
  int num_rects = capture_data->dirty_rects().size();
  for (int i = 0; i < num_rects; i++) {
    scoped_refptr<DataBuffer> data;
    const gfx::Rect& dirty_rect = capture_data->dirty_rects()[i];
    scoped_ptr<UpdateStreamPacketHeader> header(new UpdateStreamPacketHeader);
    if (EncodeRect(dirty_rect,
                   capture_data,
                   header.get(),
                   &data)) {
      EncodingState state = EncodingInProgress;
      if (i == 0) {
        state |= EncodingStarting;
      }
      if (i == num_rects - 1) {
        state |= EncodingEnded;
      }
      data_available_callback->Run(header.release(),
                                   data,
                                   state);
    }
  }

  delete data_available_callback;
}

bool EncoderVerbatim::EncodeRect(
    const gfx::Rect& dirty,
    const scoped_refptr<Capturer::CaptureData>& capture_data,
    UpdateStreamPacketHeader* header,
    scoped_refptr<DataBuffer>* output_data) {
  int bytes_per_pixel = GetBytesPerPixel(capture_data->pixel_format());
  int row_size = bytes_per_pixel * dirty.width();

  // Calculate the size of output.
  int output_size = 0;
  for (int i = 0; i < Capturer::DataPlanes::kPlaneCount; ++i) {
    // TODO(hclam): Handle YUV since the height would be different.
    const uint8* in = capture_data->data_planes().data[i];
    if (!in) continue;
    output_size += row_size * dirty.height();
  }

  header->set_x(dirty.x());
  header->set_y(dirty.y());
  header->set_width(dirty.width());
  header->set_height(dirty.height());
  header->set_encoding(EncodingNone);
  header->set_pixel_format(capture_data->pixel_format());

  *output_data = new DataBuffer(new uint8[output_size], output_size);
  uint8* out = (*output_data)->GetWritableData();

  for (int i = 0; i < Capturer::DataPlanes::kPlaneCount; ++i) {
    const uint8* in = capture_data->data_planes().data[i];
    // Skip over planes that don't have data.
    if (!in) continue;

    // TODO(hclam): Handle YUV since the height would be different.
    for (int j = 0; j < dirty.height(); ++j) {
      DCHECK_LE(row_size, capture_data->data_planes().strides[i]);
      memcpy(out, in, row_size);
      in += capture_data->data_planes().strides[i];
      out += row_size;
    }
  }
  return true;
}

}  // namespace remoting
