// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/callback.h"
#include "media/base/data_buffer.h"
#include "remoting/host/encoder_vp8.h"

extern "C" {
// TODO(garykac): Rix with correct path to vp8 header.
#include "remoting/third_party/on2/include/vp8cx.h"
}

namespace remoting {

EncoderVp8::EncoderVp8()
    : initialized_(false),
      last_timestamp_(0) {
}

EncoderVp8::~EncoderVp8() {
}

bool EncoderVp8::Init() {
  // TODO(hclam): Now always assume we receive YV12. May need to extend this
  // so we can do color space conversion manually.
  image_.fmt = IMG_FMT_YV12;
  image_.w = width_;
  image_.h = height_;

  on2_codec_enc_cfg_t config;
  on2_codec_err_t result = on2_codec_enc_config_default(&on2_codec_vp8_cx_algo,
                                                        &config, 0);

  // TODO(hclam): Adjust the parameters.
  config.g_w = width_;
  config.g_h = height_;
  config.g_pass = ON2_RC_ONE_PASS;
  config.g_profile = 1;
  config.g_threads = 2;
  config.rc_target_bitrate = 1000000;
  config.rc_min_quantizer = 0;
  config.rc_max_quantizer = 15;
  config.g_timebase.num = 1;
  config.g_timebase.den = 30;

  if (on2_codec_enc_init(&codec_, &on2_codec_vp8_cx_algo, &config, 0))
    return false;

  on2_codec_control_(&codec_, VP8E_SET_CPUUSED, -15);
  return true;
}

void EncoderVp8::Encode(const DirtyRects& dirty_rects,
                        const uint8** input_data,
                        const int* strides,
                        bool key_frame,
                        UpdateStreamPacketHeader* header,
                        scoped_refptr<media::DataBuffer>* output_data,
                        bool* encode_done,
                        Task* data_available_task) {
  // This will allow the task be called when this method exits.
  media::AutoTaskRunner task(data_available_task);
  *encode_done = false;

  // TODO(hclam): We only initialize the encoder once. We may have to
  // allow encoder be initialized with difference sizes.
  if (!initialized_) {
    if (!Init()) {
      LOG(ERROR) << "Can't initialize VP8 encoder";
      return;
    }
    initialized_ = true;
  }

  // Assume the capturer has done the color space conversion.
  if (!input_data || !strides)
    return;

  image_.planes[0] = (unsigned char*)input_data[0];
  image_.planes[1] = (unsigned char*)input_data[1];
  image_.planes[2] = (unsigned char*)input_data[2];
  image_.stride[0] = strides[0];
  image_.stride[1] = strides[1];
  image_.stride[2] = strides[2];

  // Do the actual encoding.
  if (on2_codec_encode(&codec_, &image_,
                       last_timestamp_, 1, 0, ON2_DL_REALTIME)) {
    return;
  }

  // TODO(hclam): fix this.
  last_timestamp_ += 100;

  // Read the encoded data.
  on2_codec_iter_t iter = NULL;
  bool got_data = false;

  // TODO(hclam: We assume one frame of input will get exactly one frame of
  // output. This assumption may not be valid.
  while (!got_data) {
    on2_codec_cx_pkt_t* packet = on2_codec_get_cx_data(&codec_, &iter);
    if (!packet)
      continue;

    switch (packet->kind) {
      case ON2_CODEC_CX_FRAME_PKT:
        got_data = true;
        *encode_done = true;
        *output_data = new media::DataBuffer(packet->data.frame.sz);
        memcpy((*output_data)->GetWritableData(),
               packet->data.frame.buf,
               packet->data.frame.sz);
        break;
      default:
        break;
    }
  }
  return;
}

void EncoderVp8::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
}

void EncoderVp8::SetPixelFormat(PixelFormat pixel_format) {
  pixel_format_ = pixel_format;
}

}  // namespace remoting
