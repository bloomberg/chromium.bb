// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_vp8.h"

#include <math.h>

#include "base/logging.h"
#include "media/base/media.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/libvpx.h"
}

namespace remoting {

DecoderVp8::DecoderVp8()
    : state_(kUninitialized),
      codec_(NULL),
      last_image_(NULL),
      clip_rect_(SkIRect::MakeEmpty()),
      horizontal_scale_ratio_(1.0),
      vertical_scale_ratio_(1.0) {
}

DecoderVp8::~DecoderVp8() {
  if (codec_) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec_);
    CHECK(ret == VPX_CODEC_OK) << "Failed to destroy codec";
  }
  delete codec_;
}

void DecoderVp8::Initialize(scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(kUninitialized, state_);

  if (frame->format() != media::VideoFrame::RGB32) {
    LOG(INFO) << "DecoderVp8 only supports RGB32 as output";
    state_ = kError;
    return;
  }
  frame_ = frame;

  state_ = kReady;
}

Decoder::DecodeResult DecoderVp8::DecodePacket(const VideoPacket* packet) {
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

  RectVector rects;
  rects.reserve(packet->dirty_rects_size());
  for (int i = 0; i < packet->dirty_rects_size(); ++i) {
    Rect remoting_rect = packet->dirty_rects(i);
    rects.push_back(SkIRect::MakeXYWH(remoting_rect.x(),
                                      remoting_rect.y(),
                                      remoting_rect.width(),
                                      remoting_rect.height()));
  }

  if (!DoScaling())
    ConvertRects(rects, &updated_rects_);
  else
    ScaleAndConvertRects(rects, &updated_rects_);
  return DECODE_DONE;
}

void DecoderVp8::GetUpdatedRects(RectVector* rects) {
  rects->swap(updated_rects_);
}

void DecoderVp8::Reset() {
  frame_ = NULL;
  state_ = kUninitialized;
}

bool DecoderVp8::IsReadyForData() {
  return state_ == kReady;
}

VideoPacketFormat::Encoding DecoderVp8::Encoding() {
  return VideoPacketFormat::ENCODING_VP8;
}

void DecoderVp8::SetScaleRatios(double horizontal_ratio,
                                double vertical_ratio) {
  // TODO(hclam): Ratio greater than 1.0 is not supported. This is
  // because we need to reallocate the backing video frame and this
  // is not implemented yet.
  if (horizontal_ratio > 1.0 || horizontal_ratio <= 0.0 ||
      vertical_ratio > 1.0 || vertical_ratio <= 0.0) {
    return;
  }

  horizontal_scale_ratio_ = horizontal_ratio;
  vertical_scale_ratio_ = vertical_ratio;
}

void DecoderVp8::SetClipRect(const SkIRect& clip_rect) {
  clip_rect_ = clip_rect;
}

void DecoderVp8::RefreshRects(const RectVector& rects) {
  if (!DoScaling())
    ConvertRects(rects, &updated_rects_);
  else
    ScaleAndConvertRects(rects, &updated_rects_);
}

bool DecoderVp8::DoScaling() const {
  return horizontal_scale_ratio_ != 1.0 || vertical_scale_ratio_ != 1.0;
}

void DecoderVp8::ConvertRects(const RectVector& input_rects,
                              RectVector* output_rects) {
  if (!last_image_)
    return;

  // The conversion routine we use is optimized for even widths & heights.
  int image_width = RoundToTwosMultiple(last_image_->d_w);
  int image_height = RoundToTwosMultiple(last_image_->d_h);

  uint8* output_rgb_buf = frame_->data(media::VideoFrame::kRGBPlane);
  const int output_stride = frame_->stride(media::VideoFrame::kRGBPlane);

  output_rects->clear();

  // Clip based on both the output dimensions and Pepper clip rect.
  SkIRect clip_rect = clip_rect_;
  if (!clip_rect.intersect(SkIRect::MakeWH(image_width, image_height)))
    return;

  output_rects->reserve(input_rects.size());

  for (size_t i = 0; i < input_rects.size(); ++i) {
    // Align the rectangle to avoid artifacts in color space conversion.
    SkIRect dest_rect = AlignRect(input_rects[i]);

    // Clip to the image and Pepper clip region.
    if (!dest_rect.intersect(clip_rect))
      continue;

    ConvertYUVToRGB32WithRect(last_image_->planes[0],
                              last_image_->planes[1],
                              last_image_->planes[2],
                              output_rgb_buf,
                              dest_rect,
                              last_image_->stride[0],
                              last_image_->stride[1],
                              output_stride);

    output_rects->push_back(dest_rect);
  }
}

void DecoderVp8::ScaleAndConvertRects(const RectVector& input_rects,
                                      RectVector* output_rects) {
  if (!last_image_)
    return;

  int input_width = last_image_->d_w;
  int input_height = last_image_->d_h;

  uint8* output_rgb_buf = frame_->data(media::VideoFrame::kRGBPlane);
  const int output_stride = frame_->stride(media::VideoFrame::kRGBPlane);

  // TODO(wez): Resize |frame_| to our desired output dimensions when scaling.
  int output_width = ceil(input_width * horizontal_scale_ratio_);
  int output_height = ceil(input_height * vertical_scale_ratio_);

  output_rects->clear();

  // Clip based on both the output dimensions and Pepper clip rect.
  SkIRect clip_rect = clip_rect_;
  if (!clip_rect.intersect(SkIRect::MakeWH(output_width, output_height)))
    return;

  output_rects->reserve(input_rects.size());

  for (size_t i = 0; i < input_rects.size(); ++i) {
    // Determine the scaled area affected by this rectangle changing.
    SkIRect output_rect = ScaleRect(input_rects[i],
                                    horizontal_scale_ratio_,
                                    vertical_scale_ratio_);
    if (!output_rect.intersect(clip_rect))
      continue;

    // The scaler will not read outside the input dimensions.
    ScaleYUVToRGB32WithRect(last_image_->planes[0],
                            last_image_->planes[1],
                            last_image_->planes[2],
                            output_rgb_buf,
                            input_rects[i],
                            output_rect,
                            last_image_->stride[0],
                            last_image_->stride[1],
                            output_stride);
    output_rects->push_back(output_rect);
  }
}

}  // namespace remoting
