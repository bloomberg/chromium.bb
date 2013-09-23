// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_verbatim.h"

#include "base/logging.h"
#include "remoting/base/util.h"

namespace remoting {

VideoDecoderVerbatim::VideoDecoderVerbatim()
    : screen_size_(SkISize::Make(0, 0)) {}

VideoDecoderVerbatim::~VideoDecoderVerbatim() {}

void VideoDecoderVerbatim::Initialize(const SkISize& screen_size) {
  updated_region_.setEmpty();
  screen_buffer_.reset();

  screen_size_ = screen_size;
  // Allocate the screen buffer, if necessary.
  if (!screen_size_.isEmpty()) {
    screen_buffer_.reset(
        new uint8[screen_size_.width() * screen_size_.height() *
                  kBytesPerPixel]);
  }
}

bool VideoDecoderVerbatim::DecodePacket(const VideoPacket& packet) {
  SkRegion region;

  const char* in = packet.data().data();
  int stride = kBytesPerPixel * screen_size_.width();
  for (int i = 0; i < packet.dirty_rects_size(); ++i) {
    Rect proto_rect = packet.dirty_rects(i);
    SkIRect rect = SkIRect::MakeXYWH(proto_rect.x(),
                                     proto_rect.y(),
                                     proto_rect.width(),
                                     proto_rect.height());
    region.op(rect, SkRegion::kUnion_Op);

    if (!SkIRect::MakeSize(screen_size_).contains(rect)) {
      LOG(ERROR) << "Invalid packet received";
      return false;
    }

    int rect_row_size = kBytesPerPixel * rect.width();
    uint8_t* out = screen_buffer_.get() + rect.y() * stride +
                   rect.x() * kBytesPerPixel;
    for (int y = rect.y(); y < rect.y() + rect.height(); ++y) {
      if (in + rect_row_size > packet.data().data() + packet.data().size()) {
        LOG(ERROR) << "Invalid packet received";
        return false;
      }
      memcpy(out, in, rect_row_size);
      in += rect_row_size;
      out += stride;
    }
  }

  if (in != packet.data().data() + packet.data().size()) {
    LOG(ERROR) << "Invalid packet received";
    return false;
  }

  updated_region_.op(region, SkRegion::kUnion_Op);

  return true;
}

void VideoDecoderVerbatim::Invalidate(const SkISize& view_size,
                                      const SkRegion& region) {
  updated_region_.op(region, SkRegion::kUnion_Op);
}

void VideoDecoderVerbatim::RenderFrame(const SkISize& view_size,
                                       const SkIRect& clip_area,
                                       uint8* image_buffer,
                                       int image_stride,
                                       SkRegion* output_region) {
  output_region->setEmpty();

  // TODO(alexeypa): scaling is not implemented.
  SkIRect clip_rect = SkIRect::MakeSize(screen_size_);
  if (!clip_rect.intersect(clip_area))
    return;

  int screen_stride = screen_size_.width() * kBytesPerPixel;

  for (SkRegion::Iterator i(updated_region_); !i.done(); i.next()) {
    SkIRect rect(i.rect());
    if (!rect.intersect(clip_rect))
      continue;

    CopyRGB32Rect(screen_buffer_.get(), screen_stride,
                  clip_rect,
                  image_buffer, image_stride,
                  clip_area,
                  rect);
    output_region->op(rect, SkRegion::kUnion_Op);
  }

  updated_region_.setEmpty();
}

const SkRegion* VideoDecoderVerbatim::GetImageShape() {
  return NULL;
}

}  // namespace remoting
