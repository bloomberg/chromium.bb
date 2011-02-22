// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_vp8.h"

#include "media/base/media.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
}

namespace remoting {

DecoderVp8::DecoderVp8()
    : state_(kUninitialized),
      codec_(NULL) {
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
    vpx_codec_err_t ret =
        vpx_codec_dec_init(
            codec_, vpx_codec_vp8_dx(), NULL, 0);
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

  // Perform YUV conversion.
  uint8* data_start = frame_->data(media::VideoFrame::kRGBPlane);
  int stride = frame_->stride(media::VideoFrame::kRGBPlane);

  // Propogate updated rects.
  updated_rects_.clear();
  for (int i = 0; i < packet->dirty_rects_size(); ++i) {
    gfx::Rect r = gfx::Rect(packet->dirty_rects(i).x(),
                            packet->dirty_rects(i).y(),
                            packet->dirty_rects(i).width(),
                            packet->dirty_rects(i).height());

    // Perform color space conversion only on the updated rectangle.
    ConvertYUVToRGB32WithRect(image->planes[0],
                              image->planes[1],
                              image->planes[2],
                              data_start,
                              r.x(),
                              r.y(),
                              r.width(),
                              r.height(),
                              image->stride[0],
                              image->stride[1],
                              stride);
    updated_rects_.push_back(r);
  }
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

}  // namespace remoting
