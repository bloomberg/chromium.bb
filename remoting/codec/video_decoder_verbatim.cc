// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_verbatim.h"

#include "base/logging.h"
#include "remoting/base/util.h"

namespace remoting {

namespace {
// Both input and output data are assumed to be RGBA32.
const int kBytesPerPixel = 4;
}  // namespace

VideoDecoderVerbatim::VideoDecoderVerbatim() {}

VideoDecoderVerbatim::~VideoDecoderVerbatim() {}

bool VideoDecoderVerbatim::IsReadyForData() {
  return true;
}

void VideoDecoderVerbatim::Initialize(const webrtc::DesktopSize& screen_size) {
  updated_region_.Clear();
  screen_buffer_.reset();

  screen_size_ = screen_size;
  // Allocate the screen buffer, if necessary.
  if (!screen_size_.is_empty()) {
    screen_buffer_.reset(
        new uint8
            [screen_size_.width() * screen_size_.height() * kBytesPerPixel]);
  }
}

VideoDecoder::DecodeResult VideoDecoderVerbatim::DecodePacket(
    const VideoPacket* packet) {
  webrtc::DesktopRegion region;

  const char* in = packet->data().data();
  int stride = kBytesPerPixel * screen_size_.width();
  for (int i = 0; i < packet->dirty_rects_size(); ++i) {
    Rect proto_rect = packet->dirty_rects(i);
    webrtc::DesktopRect rect =
        webrtc::DesktopRect::MakeXYWH(proto_rect.x(), proto_rect.y(),
                                      proto_rect.width(), proto_rect.height());
    region.AddRect(rect);

    if (!DoesRectContain(webrtc::DesktopRect::MakeSize(screen_size_), rect)) {
      LOG(ERROR) << "Invalid packet received";
      return DECODE_ERROR;
    }

    int rect_row_size = kBytesPerPixel * rect.width();
    uint8_t* out = screen_buffer_.get() + rect.top() * stride +
                   rect.left() * kBytesPerPixel;
    for (int y = rect.top(); y < rect.top() + rect.height(); ++y) {
      if (in + rect_row_size > packet->data().data() + packet->data().size()) {
        LOG(ERROR) << "Invalid packet received";
        return DECODE_ERROR;
      }
      memcpy(out, in, rect_row_size);
      in += rect_row_size;
      out += stride;
    }
  }

  if (in != packet->data().data() + packet->data().size()) {
    LOG(ERROR) << "Invalid packet received";
    return DECODE_ERROR;
  }

  updated_region_.AddRegion(region);

  return DECODE_DONE;
}

VideoPacketFormat::Encoding VideoDecoderVerbatim::Encoding() {
  return VideoPacketFormat::ENCODING_VERBATIM;
}

void VideoDecoderVerbatim::Invalidate(const webrtc::DesktopSize& view_size,
                                      const webrtc::DesktopRegion& region) {
  updated_region_.AddRegion(region);
}

void VideoDecoderVerbatim::RenderFrame(const webrtc::DesktopSize& view_size,
                                       const webrtc::DesktopRect& clip_area,
                                       uint8* image_buffer,
                                       int image_stride,
                                       webrtc::DesktopRegion* output_region) {
  output_region->Clear();

  // TODO(alexeypa): scaling is not implemented.
  webrtc::DesktopRect clip_rect = webrtc::DesktopRect::MakeSize(screen_size_);
  clip_rect.IntersectWith(clip_area);
  if (clip_rect.is_empty())
    return;

  int screen_stride = screen_size_.width() * kBytesPerPixel;

  for (webrtc::DesktopRegion::Iterator i(updated_region_);
       !i.IsAtEnd(); i.Advance()) {
    webrtc::DesktopRect rect(i.rect());
    rect.IntersectWith(clip_rect);
    if (rect.is_empty())
      continue;

    CopyRGB32Rect(screen_buffer_.get(), screen_stride,
                  clip_rect,
                  image_buffer, image_stride,
                  clip_area,
                  rect);
    output_region->AddRect(rect);
  }

  updated_region_.Clear();
}

const webrtc::DesktopRegion* VideoDecoderVerbatim::GetImageShape() {
  return NULL;
}

}  // namespace remoting
