// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_verbatim.h"

#include "base/logging.h"
#include "gfx/rect.h"
#include "media/base/data_buffer.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/util.h"

namespace remoting {

// TODO(garykac): Move up into shared location. Share value with encoder_zlib.
// TODO(garykac): 10* is added to ensure that rects fit in a single packet.
//                Add support for splitting across packets and remove the 10*.
static const int kPacketSize = 10 * 1024 * 1024;

using media::DataBuffer;

EncoderVerbatim::EncoderVerbatim()
    : packet_size_(kPacketSize) {
}

EncoderVerbatim::EncoderVerbatim(int packet_size)
    : packet_size_(packet_size) {
}

void EncoderVerbatim::Encode(scoped_refptr<CaptureData> capture_data,
                             bool key_frame,
                             DataAvailableCallback* data_available_callback) {
  capture_data_ = capture_data;
  callback_.reset(data_available_callback);

  const InvalidRects& rects = capture_data->dirty_rects();
  int index = 0;
  for (InvalidRects::const_iterator r = rects.begin();
      r != rects.end(); ++r, ++index) {
    EncodeRect(*r, index);
  }

  capture_data_ = NULL;
  callback_.reset();
}

// TODO(garykac): This assumes that the rect fits into a single packet.
//                Fix this by modeling after code in encoder_zlib.cc
void EncoderVerbatim::EncodeRect(const gfx::Rect& rect, size_t rect_index) {
  CHECK(capture_data_->data_planes().data[0]);
  const int stride = capture_data_->data_planes().strides[0];
  const int bytes_per_pixel = GetBytesPerPixel(capture_data_->pixel_format());
  const int row_size = bytes_per_pixel * rect.width();

  ChromotingHostMessage* message = new ChromotingHostMessage();
  RectangleUpdatePacket* update = message->mutable_rectangle_update();
  PrepareUpdateStart(rect, update);

  const uint8* in = capture_data_->data_planes().data[0] +
                    rect.y() * stride +
                    rect.x() * bytes_per_pixel;
  // TODO(hclam): Fill in the sequence number.
  uint8* out = GetOutputBuffer(update, packet_size_);
  int total_bytes = 0;
  for (int y = 0; y < rect.height(); y++) {
    memcpy(out, in, row_size);
    out += row_size;
    in += stride;
    total_bytes += row_size;
  }

  // We have reached the end of stream.
  update->set_flags(update->flags() | RectangleUpdatePacket::LAST_PACKET);

  // If we have filled the message or we have reached the end of stream.
  message->mutable_rectangle_update()->mutable_encoded_rect()->
      resize(total_bytes);
  SubmitMessage(message, rect_index);
}

void EncoderVerbatim::PrepareUpdateStart(const gfx::Rect& rect,
                                         RectangleUpdatePacket* update) {

  update->set_flags(update->flags() | RectangleUpdatePacket::FIRST_PACKET);
  RectangleFormat* format = update->mutable_format();

  format->set_x(rect.x());
  format->set_y(rect.y());
  format->set_width(rect.width());
  format->set_height(rect.height());
  format->set_encoding(EncodingNone);
  format->set_pixel_format(capture_data_->pixel_format());
}

uint8* EncoderVerbatim::GetOutputBuffer(RectangleUpdatePacket* update,
                                        size_t size) {
  update->mutable_encoded_rect()->resize(size);
  // TODO(ajwong): Is there a better way to do this at all???
  return const_cast<uint8*>(reinterpret_cast<const uint8*>(
      update->mutable_encoded_rect()->data()));
}

void EncoderVerbatim::SubmitMessage(ChromotingHostMessage* message,
                                    size_t rect_index) {
  EncodingState state = EncodingInProgress;
  const RectangleUpdatePacket& update = message->rectangle_update();
  if (rect_index == 0 &&
      (update.flags() | RectangleUpdatePacket::FIRST_PACKET)) {
    state |= EncodingStarting;
  }
  if (rect_index == capture_data_->dirty_rects().size() - 1 &&
      (update.flags() | RectangleUpdatePacket::LAST_PACKET)) {
    state |= EncodingEnded;
  }
  callback_->Run(message, state);
}

}  // namespace remoting
