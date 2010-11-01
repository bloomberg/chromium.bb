// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/callback.h"
#include "media/base/data_buffer.h"
#include "media/base/media.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/encoder_vp8.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/include/vpx/vpx_codec.h"
#include "third_party/libvpx/include/vpx/vpx_encoder.h"
#include "third_party/libvpx/include/vpx/vp8cx.h"
}

namespace remoting {

EncoderVp8::EncoderVp8()
    : initialized_(false),
      codec_(NULL),
      image_(NULL),
      last_timestamp_(0) {
}

EncoderVp8::~EncoderVp8() {
  if (initialized_) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec_.get());
    DCHECK(ret == VPX_CODEC_OK) << "Failed to destroy codec";
  }
}

bool EncoderVp8::Init(int width, int height) {
  codec_.reset(new vpx_codec_ctx_t());
  image_.reset(new vpx_image_t());
  memset(image_.get(), 0, sizeof(vpx_image_t));

  image_->fmt = VPX_IMG_FMT_YV12;

  // libvpx seems to require both to be assigned.
  image_->d_w = width;
  image_->w = width;
  image_->d_h = height;
  image_->h = height;

  vpx_codec_enc_cfg_t config;
  const vpx_codec_iface_t* algo =
      (const vpx_codec_iface_t*)media::GetVp8CxAlgoAddress();
  vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
  if (ret != VPX_CODEC_OK)
    return false;

  // TODO(hclam): Tune the parameters to better suit the application.
  config.rc_target_bitrate = width * height * config.rc_target_bitrate
      / config.g_w / config.g_h;
  config.g_w = width;
  config.g_h = height;
  config.g_pass = VPX_RC_ONE_PASS;
  config.g_profile = 1;
  config.g_threads = 2;
  config.rc_min_quantizer = 0;
  config.rc_max_quantizer = 15;
  config.g_timebase.num = 1;
  config.g_timebase.den = 30;

  if (vpx_codec_enc_init(codec_.get(), algo, &config, 0))
    return false;
  return true;
}

bool EncoderVp8::PrepareImage(scoped_refptr<CaptureData> capture_data) {
  if (!yuv_image_.get()) {
    const int plane_size = capture_data->width() * capture_data->height();

    // YUV image size is 1.5 times of a plane. Multiplication is performed first
    // to avoid rounding error.
    const int size = plane_size * 3 / 2;

    yuv_image_.reset(new uint8[size]);

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(yuv_image_.get(), 128, size);

    // Fill in the information for |image_|.
    unsigned char* image = reinterpret_cast<unsigned char*>(yuv_image_.get());
    image_->planes[0] = image;
    image_->planes[1] = image + plane_size;

    // The V plane starts from 1.25 of the plane size.
    image_->planes[2] = image + plane_size + plane_size / 4;

    // In YV12 Y plane has full width, UV plane has half width because of
    // subsampling.
    image_->stride[0] = image_->w;
    image_->stride[1] = image_->w / 2;
    image_->stride[2] = image_->w / 2;
  }

  // And then do RGB->YUV conversion.
  // Currently we just produce the Y channel as the average of RGB. This will
  // give a gray scale image after conversion.
  // TODO(hclam): Implement the actual color space conversion.
  DCHECK(capture_data->pixel_format() == PixelFormatRgb32)
      << "Only RGB32 is supported";
  uint8* in = capture_data->data_planes().data[0];
  const int in_stride = capture_data->data_planes().strides[0];
  uint8* out = yuv_image_.get();
  const int out_stride = image_->stride[0];
  for (int i = 0; i < capture_data->height(); ++i) {
    for (int j = 0; j < capture_data->width(); ++j) {
      // Since the input pixel format is RGB32, there are 4 bytes per pixel.
      uint8* pixel = in + 4 * j;
      out[j] = (pixel[0] + pixel[1] + pixel[2]) / 3;
    }
    in += in_stride;
    out += out_stride;
  }
  return true;
}

void EncoderVp8::Encode(scoped_refptr<CaptureData> capture_data,
                        bool key_frame,
                        DataAvailableCallback* data_available_callback) {
  if (!initialized_) {
    bool ret = Init(capture_data->width(), capture_data->height());
    // TODO(hclam): Handle error better.
    DCHECK(ret) << "Initialization of encoder failed";
    initialized_ = ret;
  }

  if (!PrepareImage(capture_data)) {
    NOTREACHED() << "Can't image data for encoding";
  }

  // Do the actual encoding.
  vpx_codec_err_t ret = vpx_codec_encode(codec_.get(), image_.get(),
                                         last_timestamp_,
                                         1, 0, VPX_DL_REALTIME);
  DCHECK(ret == VPX_CODEC_OK) << "Encoding error: "
                              << vpx_codec_err_to_string(ret)
                              << "\n"
                              << "Details: "
                              << vpx_codec_error(codec_.get())
                              << "\n"
                              << vpx_codec_error_detail(codec_.get());

  // TODO(hclam): fix this.
  last_timestamp_ += 100;

  // Read the encoded data.
  vpx_codec_iter_t iter = NULL;
  bool got_data = false;

  // TODO(hclam): Make sure we get exactly one frame from the packet.
  // TODO(hclam): We should provide the output buffer to avoid one copy.
  ChromotingHostMessage* message = new ChromotingHostMessage();
  UpdateStreamPacketMessage* packet = message->mutable_update_stream_packet();

  // Prepare the begin rect.
  packet->mutable_begin_rect()->set_x(0);
  packet->mutable_begin_rect()->set_y(0);
  packet->mutable_begin_rect()->set_width(capture_data->width());
  packet->mutable_begin_rect()->set_height(capture_data->height());
  packet->mutable_begin_rect()->set_encoding(EncodingVp8);
  packet->mutable_begin_rect()->set_pixel_format(PixelFormatYv12);

  while (!got_data) {
    const vpx_codec_cx_pkt_t* packet = vpx_codec_get_cx_data(codec_.get(),
                                                             &iter);
    if (!packet)
      continue;

    switch (packet->kind) {
      case VPX_CODEC_CX_FRAME_PKT:
        got_data = true;
        message->mutable_update_stream_packet()->mutable_rect_data()->set_data(
            packet->data.frame.buf, packet->data.frame.sz);
        break;
      default:
        break;
    }
  }

  // Enter the end rect.
  message->mutable_update_stream_packet()->mutable_end_rect();
  data_available_callback->Run(
      message,
      EncodingStarting | EncodingInProgress | EncodingEnded);
  delete data_available_callback;
}

}  // namespace remoting
