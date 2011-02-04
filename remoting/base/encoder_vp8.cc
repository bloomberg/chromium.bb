// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_vp8.h"

#include "base/logging.h"
#include "media/base/callback.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
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
  const vpx_codec_iface_t* algo = vpx_codec_vp8_cx();
  CHECK(algo);
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
  config.rc_min_quantizer = 20;
  config.rc_max_quantizer = 30;
  config.g_timebase.num = 1;
  config.g_timebase.den = 30;

  if (vpx_codec_enc_init(codec_.get(), algo, &config, 0))
    return false;
  return true;
}

static int RoundToTwosMultiple(int x) {
  return x & (~1);
}

// Align the sides of the rectange to multiples of 2.
static gfx::Rect AlignRect(const gfx::Rect& rect, int width, int height) {
  CHECK(rect.width() > 0 && rect.height() > 0);
  int x = RoundToTwosMultiple(rect.x());
  int y = RoundToTwosMultiple(rect.y());
  int right = std::min(RoundToTwosMultiple(rect.right() + 1),
                       RoundToTwosMultiple(width));
  int bottom = std::min(RoundToTwosMultiple(rect.bottom() + 1),
                        RoundToTwosMultiple(height));

  // Do the final check to make sure the width and height are not negative.
  gfx::Rect r(x, y, right - x, bottom - y);
  if (r.width() <= 0 || r.height() <= 0) {
    r.set_width(0);
    r.set_height(0);
  }
  return r;
}

bool EncoderVp8::PrepareImage(scoped_refptr<CaptureData> capture_data,
                              std::vector<gfx::Rect>* updated_rects) {
  const int plane_size = capture_data->width() * capture_data->height();

  if (!yuv_image_.get()) {

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

  // Perform RGB->YUV conversion.
  if (capture_data->pixel_format() != media::VideoFrame::RGB32) {
    LOG(ERROR) << "Only RGB32 is supported";
    return false;
  }

  const InvalidRects& rects = capture_data->dirty_rects();
  const uint8* in = capture_data->data_planes().data[0];
  const int in_stride = capture_data->data_planes().strides[0];
  uint8* y_out = yuv_image_.get();
  uint8* u_out = yuv_image_.get() + plane_size;
  uint8* v_out = yuv_image_.get() + plane_size + plane_size / 4;
  const int y_stride = image_->stride[0];
  const int uv_stride = image_->stride[1];

  DCHECK(updated_rects->empty());
  for (InvalidRects::const_iterator r = rects.begin(); r != rects.end(); ++r) {
    // Align the rectangle report it as updated.
    gfx::Rect rect = AlignRect(*r, image_->w, image_->h);
    updated_rects->push_back(rect);

    ConvertRGB32ToYUVWithRect(in,
                              y_out,
                              u_out,
                              v_out,
                              rect.x(),
                              rect.y(),
                              rect.width(),
                              rect.height(),
                              in_stride,
                              y_stride,
                              uv_stride);
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

  std::vector<gfx::Rect> updated_rects;
  if (!PrepareImage(capture_data, &updated_rects)) {
    NOTREACHED() << "Can't image data for encoding";
  }

  // Do the actual encoding.
  vpx_codec_err_t ret = vpx_codec_encode(codec_.get(), image_.get(),
                                         last_timestamp_,
                                         1, 0, VPX_DL_REALTIME);
  DCHECK_EQ(ret, VPX_CODEC_OK)
      << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
      << "Details: " << vpx_codec_error(codec_.get()) << "\n"
      << vpx_codec_error_detail(codec_.get());

  // TODO(hclam): fix this.
  last_timestamp_ += 100;

  // Read the encoded data.
  vpx_codec_iter_t iter = NULL;
  bool got_data = false;

  // TODO(hclam): Make sure we get exactly one frame from the packet.
  // TODO(hclam): We should provide the output buffer to avoid one copy.
  VideoPacket* message = new VideoPacket();

  while (!got_data) {
    const vpx_codec_cx_pkt_t* packet = vpx_codec_get_cx_data(codec_.get(),
                                                             &iter);
    if (!packet)
      continue;

    switch (packet->kind) {
      case VPX_CODEC_CX_FRAME_PKT:
        got_data = true;
        // TODO(sergeyu): Split each frame into multiple partitions.
        message->set_data(packet->data.frame.buf, packet->data.frame.sz);
        break;
      default:
        break;
    }
  }

  message->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);
  message->set_flags(VideoPacket::FIRST_PACKET | VideoPacket::LAST_PACKET |
                     VideoPacket::LAST_PARTITION);
  for (size_t i = 0; i < updated_rects.size(); ++i) {
    Rect* rect = message->add_dirty_rects();
    rect->set_x(updated_rects[i].x());
    rect->set_y(updated_rects[i].y());
    rect->set_width(updated_rects[i].width());
    rect->set_height(updated_rects[i].height());
  }

  data_available_callback->Run(message);
  delete data_available_callback;
}

}  // namespace remoting
