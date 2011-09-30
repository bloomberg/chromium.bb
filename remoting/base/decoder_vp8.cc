// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_vp8.h"

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

  std::vector<gfx::Rect> rects;
  for (int i = 0; i < packet->dirty_rects_size(); ++i) {
    gfx::Rect r = gfx::Rect(packet->dirty_rects(i).x(),
                            packet->dirty_rects(i).y(),
                            packet->dirty_rects(i).width(),
                            packet->dirty_rects(i).height());
    rects.push_back(r);
  }

  if (!DoScaling())
    ConvertRects(rects, &updated_rects_);
  else
    ScaleAndConvertRects(rects, &updated_rects_);
  return DECODE_DONE;
}

void DecoderVp8::GetUpdatedRects(UpdatedRects* rects) {
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

void DecoderVp8::SetClipRect(const gfx::Rect& clip_rect) {
  clip_rect_ = clip_rect;
}

void DecoderVp8::RefreshRects(const std::vector<gfx::Rect>& rects) {
  if (!DoScaling())
    ConvertRects(rects, &updated_rects_);
  else
    ScaleAndConvertRects(rects, &updated_rects_);
}

bool DecoderVp8::DoScaling() const {
  return horizontal_scale_ratio_ != 1.0 || vertical_scale_ratio_ != 1.0;
}

void DecoderVp8::ConvertRects(const UpdatedRects& rects,
                              UpdatedRects* output_rects) {
  if (!last_image_)
    return;

  uint8* data_start = frame_->data(media::VideoFrame::kRGBPlane);
  const int stride = frame_->stride(media::VideoFrame::kRGBPlane);

  output_rects->clear();
  for (size_t i = 0; i < rects.size(); ++i) {
    // Round down the image width and height.
    int image_width = RoundToTwosMultiple(last_image_->d_w);
    int image_height = RoundToTwosMultiple(last_image_->d_h);

    // Clip by the clipping rectangle first.
    gfx::Rect dest_rect = rects[i].Intersect(clip_rect_);

    // Then clip by the rounded down dimension of the image for safety.
    dest_rect = dest_rect.Intersect(
        gfx::Rect(0, 0, image_width, image_height));

    // Align the rectangle to avoid artifacts in color space conversion.
    dest_rect = AlignRect(dest_rect);

    if (dest_rect.IsEmpty())
      continue;

    ConvertYUVToRGB32WithRect(last_image_->planes[0],
                              last_image_->planes[1],
                              last_image_->planes[2],
                              data_start,
                              dest_rect,
                              last_image_->stride[0],
                              last_image_->stride[1],
                              stride);
    output_rects->push_back(dest_rect);
  }
}

void DecoderVp8::ScaleAndConvertRects(const UpdatedRects& rects,
                                      UpdatedRects* output_rects) {
  if (!last_image_)
    return;

  uint8* data_start = frame_->data(media::VideoFrame::kRGBPlane);
  const int stride = frame_->stride(media::VideoFrame::kRGBPlane);

  output_rects->clear();
  for (size_t i = 0; i < rects.size(); ++i) {
    // Round down the image width and height.
    int image_width = RoundToTwosMultiple(last_image_->d_w);
    int image_height = RoundToTwosMultiple(last_image_->d_h);

    // Clip by the rounded down dimension of the image for safety.
    gfx::Rect dest_rect =
        rects[i].Intersect(gfx::Rect(0, 0, image_width, image_height));

    // Align the rectangle to avoid artifacts in color space conversion.
    dest_rect = AlignRect(dest_rect);

    if (dest_rect.IsEmpty())
      continue;

    gfx::Rect scaled_rect = ScaleRect(dest_rect,
                                      horizontal_scale_ratio_,
                                      vertical_scale_ratio_);

    ScaleYUVToRGB32WithRect(last_image_->planes[0],
                            last_image_->planes[1],
                            last_image_->planes[2],
                            data_start,
                            dest_rect,
                            scaled_rect,
                            last_image_->stride[0],
                            last_image_->stride[1],
                            stride);
    output_rects->push_back(scaled_rect);
  }
}

}  // namespace remoting
