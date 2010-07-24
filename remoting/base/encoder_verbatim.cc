// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_verbatim.h"

#include "gfx/rect.h"
#include "media/base/data_buffer.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/protocol_util.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

using media::DataBuffer;

void EncoderVerbatim::Encode(scoped_refptr<CaptureData> capture_data,
                             bool key_frame,
                             DataAvailableCallback* data_available_callback) {
  int num_rects = capture_data->dirty_rects().size();
  for (int i = 0; i < num_rects; i++) {
    const gfx::Rect& dirty_rect = capture_data->dirty_rects()[i];
    HostMessage* msg = new HostMessage();
    UpdateStreamPacketMessage* packet = msg->mutable_update_stream_packet();

    if (EncodeRect(dirty_rect.x(), dirty_rect.y(), dirty_rect.width(),
                   dirty_rect.height(), capture_data, packet)) {
      // Prepare the end rect content.
      packet->mutable_end_rect();

      EncodingState state = EncodingInProgress;
      if (i == 0) {
        state |= EncodingStarting;
      }
      if (i == num_rects - 1) {
        state |= EncodingEnded;
      }
      data_available_callback->Run(msg, state);
    }
  }

  delete data_available_callback;
}

bool EncoderVerbatim::EncodeRect(
    int x, int y, int width, int height,
    const scoped_refptr<CaptureData>& capture_data,
    UpdateStreamPacketMessage* packet) {
  // Prepare the begin rect content.
  packet->mutable_begin_rect()->set_x(x);
  packet->mutable_begin_rect()->set_y(y);
  packet->mutable_begin_rect()->set_width(width);
  packet->mutable_begin_rect()->set_height(height);
  packet->mutable_begin_rect()->set_encoding(EncodingNone);
  packet->mutable_begin_rect()->set_pixel_format(capture_data->pixel_format());

  // Calculate the size of output.
  int bytes_per_pixel = GetBytesPerPixel(capture_data->pixel_format());
  int row_size = bytes_per_pixel * width;
  int output_size = 0;
  for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
    // TODO(hclam): Handle YUV since the height would be different.
    const uint8* in = capture_data->data_planes().data[i];
    if (!in) continue;
    output_size += row_size * height;
  }

  // Resize the output data buffer.
  packet->mutable_rect_data()->mutable_data()->resize(output_size);
  uint8* out = reinterpret_cast<uint8*>(
      &((*packet->mutable_rect_data()->mutable_data())[0]));

  for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
    // Skip over planes that don't have data.
    if (!capture_data->data_planes().data[i])
      continue;

    const uint8* in = capture_data->data_planes().data[i] +
                      y * capture_data->data_planes().strides[i] +
                      x * bytes_per_pixel;

    // TODO(hclam): Handle YUV since the height would be different.
    for (int j = 0; j < height; ++j) {
      DCHECK_LE(row_size, capture_data->data_planes().strides[i]);
      memcpy(out, in, width * bytes_per_pixel);
      in += capture_data->data_planes().strides[i];
      out += row_size;
    }
  }
  return true;
}

}  // namespace remoting
