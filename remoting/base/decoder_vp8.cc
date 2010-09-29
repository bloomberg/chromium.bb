// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_vp8.h"

#include "media/base/media.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/protocol_util.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/include/vpx/vpx_codec.h"
#include "third_party/libvpx/include/vpx/vpx_decoder.h"
#include "third_party/libvpx/include/vpx/vp8dx.h"
}

namespace remoting {

DecoderVp8::DecoderVp8()
    : state_(kWaitingForBeginRect),
      rect_x_(0),
      rect_y_(0),
      rect_width_(0),
      rect_height_(0),
      updated_rects_(NULL),
      codec_(NULL) {
}

DecoderVp8::~DecoderVp8() {
  if (codec_) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec_);
    CHECK(ret == VPX_CODEC_OK) << "Failed to destroy codec";
  }
  delete codec_;
}

bool DecoderVp8::BeginDecode(scoped_refptr<media::VideoFrame> frame,
                             UpdatedRects* updated_rects,
                             Task* partial_decode_done,
                             Task* decode_done) {
  DCHECK(!partial_decode_done_.get());
  DCHECK(!decode_done_.get());
  DCHECK(!updated_rects_);
  DCHECK_EQ(kWaitingForBeginRect, state_);

  partial_decode_done_.reset(partial_decode_done);
  decode_done_.reset(decode_done);
  updated_rects_ = updated_rects;

  if (frame->format() != media::VideoFrame::RGB32) {
    LOG(INFO) << "DecoderVp8 only supports RGB32 as output";
    return false;
  }
  frame_ = frame;
  return true;
}

bool DecoderVp8::PartialDecode(ChromotingHostMessage* message) {
  scoped_ptr<ChromotingHostMessage> msg_deleter(message);
  DCHECK(message->has_update_stream_packet());

  bool ret = true;
  if (message->update_stream_packet().has_begin_rect())
    ret = HandleBeginRect(message);
  if (ret && message->update_stream_packet().has_rect_data())
    ret = HandleRectData(message);
  if (ret && message->update_stream_packet().has_end_rect())
    ret = HandleEndRect(message);
  return ret;
}

void DecoderVp8::EndDecode() {
  DCHECK_EQ(kWaitingForBeginRect, state_);
  decode_done_->Run();

  partial_decode_done_.reset();
  decode_done_.reset();
  frame_ = NULL;
  updated_rects_ = NULL;
}

bool DecoderVp8::HandleBeginRect(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForBeginRect, state_);
  state_ = kWaitingForRectData;

  rect_width_ = message->update_stream_packet().begin_rect().width();
  rect_height_ = message->update_stream_packet().begin_rect().height();
  rect_x_ = message->update_stream_packet().begin_rect().x();
  rect_y_ = message->update_stream_packet().begin_rect().y();

  PixelFormat pixel_format =
      message->update_stream_packet().begin_rect().pixel_format();
  if (pixel_format != PixelFormatYv12)
    return false;
  return true;
}

bool DecoderVp8::HandleRectData(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForRectData, state_);
  DCHECK_EQ(0,
            message->update_stream_packet().rect_data().sequence_number());

  // Initialize the codec as needed.
  if (!codec_) {
    codec_ = new vpx_codec_ctx_t();
    vpx_codec_err_t ret =
        vpx_codec_dec_init(
            codec_,
            (const vpx_codec_iface_t*)media::GetVp8DxAlgoAddress(), NULL, 0);
    if (ret != VPX_CODEC_OK) {
      LOG(INFO) << "Cannot initialize codec.";
      delete codec_;
      codec_ = NULL;
      return false;
    }
  }

  // Do the actual decoding.
  vpx_codec_err_t ret = vpx_codec_decode(
      codec_,
      (uint8_t*)message->update_stream_packet().rect_data().data().c_str(),
      message->update_stream_packet().rect_data().data().size(),
      NULL, 0);
  if (ret != VPX_CODEC_OK) {
    LOG(INFO) << "Decoding failed:"
              << vpx_codec_err_to_string(ret)
              << "\n"
              << "Details: "
              << vpx_codec_error(codec_)
              << "\n"
              << vpx_codec_error_detail(codec_);
    return false;
  }

  // Gets the decoded data.
  vpx_codec_iter_t iter = NULL;
  vpx_image_t* image = vpx_codec_get_frame(codec_, &iter);
  if (!image) {
    LOG(INFO) << "No video frame decoded";
    return false;
  }

  // Perform YUV conversion.
  media::ConvertYUVToRGB32(image->planes[0], image->planes[1], image->planes[2],
                           frame_->data(media::VideoFrame::kRGBPlane),
                           rect_width_, rect_height_,
                           image->stride[0], image->stride[1],
                           frame_->stride(media::VideoFrame::kRGBPlane),
                           media::YV12);

  updated_rects_->clear();
  updated_rects_->push_back(gfx::Rect(rect_x_, rect_y_,
                                      rect_width_, rect_height_));
  partial_decode_done_->Run();
  return true;
}

bool DecoderVp8::HandleEndRect(ChromotingHostMessage* message) {
  DCHECK_EQ(kWaitingForRectData, state_);
  state_ = kWaitingForBeginRect;
  return true;
}

}  // namespace remoting
