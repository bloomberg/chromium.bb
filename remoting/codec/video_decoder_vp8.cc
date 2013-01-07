// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_vp8.h"

#include <math.h>

#include "base/logging.h"
#include "media/base/media.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
}

namespace remoting {

VideoDecoderVp8::VideoDecoderVp8()
    : state_(kUninitialized),
      codec_(NULL),
      last_image_(NULL),
      screen_size_(SkISize::Make(0, 0)) {
}

VideoDecoderVp8::~VideoDecoderVp8() {
  if (codec_) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec_);
    CHECK(ret == VPX_CODEC_OK) << "Failed to destroy codec";
  }
  delete codec_;
}

void VideoDecoderVp8::Initialize(const SkISize& screen_size) {
  DCHECK(!screen_size.isEmpty());

  screen_size_ = screen_size;
  state_ = kReady;
}

VideoDecoder::DecodeResult VideoDecoderVp8::DecodePacket(
    const VideoPacket* packet) {
  DCHECK_EQ(kReady, state_);

  // Initialize the codec as needed.
  if (!codec_) {
    codec_ = new vpx_codec_ctx_t();

    // TODO(hclam): Scale the number of threads with number of cores of the
    // machine.
    vpx_codec_dec_cfg config;
    config.w = 0;
    config.h = 0;
    config.threads = 2;
    vpx_codec_err_t ret =
        vpx_codec_dec_init(
            codec_, vpx_codec_vp8_dx(), &config, 0);
    if (ret != VPX_CODEC_OK) {
      LOG(INFO) << "Cannot initialize codec.";
      delete codec_;
      codec_ = NULL;
      state_ = kError;
      return DECODE_ERROR;
    }
  }

  // Do the actual decoding.
  vpx_codec_err_t ret = vpx_codec_decode(
      codec_, reinterpret_cast<const uint8*>(packet->data().data()),
      packet->data().size(), NULL, 0);
  if (ret != VPX_CODEC_OK) {
    LOG(INFO) << "Decoding failed:" << vpx_codec_err_to_string(ret) << "\n"
              << "Details: " << vpx_codec_error(codec_) << "\n"
              << vpx_codec_error_detail(codec_);
    return DECODE_ERROR;
  }

  // Gets the decoded data.
  vpx_codec_iter_t iter = NULL;
  vpx_image_t* image = vpx_codec_get_frame(codec_, &iter);
  if (!image) {
    LOG(INFO) << "No video frame decoded";
    return DECODE_ERROR;
  }
  last_image_ = image;

  SkRegion region;
  for (int i = 0; i < packet->dirty_rects_size(); ++i) {
    Rect remoting_rect = packet->dirty_rects(i);
    SkIRect rect = SkIRect::MakeXYWH(remoting_rect.x(),
                                     remoting_rect.y(),
                                     remoting_rect.width(),
                                     remoting_rect.height());
    region.op(rect, SkRegion::kUnion_Op);
  }

  updated_region_.op(region, SkRegion::kUnion_Op);
  return DECODE_DONE;
}

bool VideoDecoderVp8::IsReadyForData() {
  return state_ == kReady;
}

VideoPacketFormat::Encoding VideoDecoderVp8::Encoding() {
  return VideoPacketFormat::ENCODING_VP8;
}

void VideoDecoderVp8::Invalidate(const SkISize& view_size,
                                 const SkRegion& region) {
  DCHECK_EQ(kReady, state_);
  DCHECK(!view_size.isEmpty());

  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    SkIRect rect = i.rect();
    rect = ScaleRect(rect, view_size, screen_size_);
    updated_region_.op(rect, SkRegion::kUnion_Op);
  }
}

void VideoDecoderVp8::RenderFrame(const SkISize& view_size,
                                  const SkIRect& clip_area,
                                  uint8* image_buffer,
                                  int image_stride,
                                  SkRegion* output_region) {
  DCHECK_EQ(kReady, state_);
  DCHECK(!view_size.isEmpty());

  // Early-return and do nothing if we haven't yet decoded any frames.
  if (!last_image_)
    return;

  SkIRect source_clip = SkIRect::MakeWH(last_image_->d_w, last_image_->d_h);

  // ScaleYUVToRGB32WithRect does not currently support up-scaling.  We won't
  // be asked to up-scale except during resizes or if page zoom is >100%, so
  // we work-around the limitation by using the slower ScaleYUVToRGB32.
  // TODO(wez): Remove this hack if/when ScaleYUVToRGB32WithRect can up-scale.
  if (!updated_region_.isEmpty() &&
      (source_clip.width() < view_size.width() ||
       source_clip.height() < view_size.height())) {
    // We're scaling only |clip_area| into the |image_buffer|, so we need to
    // work out which source rectangle that corresponds to.
    SkIRect source_rect = ScaleRect(clip_area, view_size, screen_size_);
    source_rect = SkIRect::MakeLTRB(RoundToTwosMultiple(source_rect.left()),
                                    RoundToTwosMultiple(source_rect.top()),
                                    source_rect.right(),
                                    source_rect.bottom());

    // If there were no changes within the clip source area then don't render.
    if (!updated_region_.intersects(source_rect))
      return;

    // Scale & convert the entire clip area.
    int y_offset = CalculateYOffset(source_rect.x(),
                                    source_rect.y(),
                                    last_image_->stride[0]);
    int uv_offset = CalculateUVOffset(source_rect.x(),
                                      source_rect.y(),
                                      last_image_->stride[1]);
    ScaleYUVToRGB32(last_image_->planes[0] + y_offset,
                    last_image_->planes[1] + uv_offset,
                    last_image_->planes[2] + uv_offset,
                    image_buffer,
                    source_rect.width(),
                    source_rect.height(),
                    clip_area.width(),
                    clip_area.height(),
                    last_image_->stride[0],
                    last_image_->stride[1],
                    image_stride,
                    media::YV12,
                    media::ROTATE_0,
                    media::FILTER_BILINEAR);

    output_region->op(clip_area, SkRegion::kUnion_Op);
    updated_region_.op(source_rect, SkRegion::kDifference_Op);
    return;
  }

  for (SkRegion::Iterator i(updated_region_); !i.done(); i.next()) {
    // Determine the scaled area affected by this rectangle changing.
    SkIRect rect = i.rect();
    if (!rect.intersect(source_clip))
      continue;
    rect = ScaleRect(rect, screen_size_, view_size);
    if (!rect.intersect(clip_area))
      continue;

    ConvertAndScaleYUVToRGB32Rect(last_image_->planes[0],
                                  last_image_->planes[1],
                                  last_image_->planes[2],
                                  last_image_->stride[0],
                                  last_image_->stride[1],
                                  screen_size_,
                                  source_clip,
                                  image_buffer,
                                  image_stride,
                                  view_size,
                                  clip_area,
                                  rect);

    output_region->op(rect, SkRegion::kUnion_Op);
  }

  updated_region_.op(ScaleRect(clip_area, view_size, screen_size_),
                     SkRegion::kDifference_Op);
}

}  // namespace remoting
