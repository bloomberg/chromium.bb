// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_vpx.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
}

namespace remoting {

namespace {

// Defines the dimension of a macro block. This is used to compute the active
// map for the encoder.
const int kMacroBlockSize = 16;

ScopedVpxCodec CreateVP8Codec(const webrtc::DesktopSize& size) {
  ScopedVpxCodec codec(new vpx_codec_ctx_t);

  // Configure the encoder.
  vpx_codec_enc_cfg_t config;
  const vpx_codec_iface_t* algo = vpx_codec_vp8_cx();
  CHECK(algo);
  vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
  if (ret != VPX_CODEC_OK)
    return ScopedVpxCodec();

  config.rc_target_bitrate = size.width() * size.height() *
      config.rc_target_bitrate / config.g_w / config.g_h;
  config.g_w = size.width();
  config.g_h = size.height();
  config.g_pass = VPX_RC_ONE_PASS;

  // Value of 2 means using the real time profile. This is basically a
  // redundant option since we explicitly select real time mode when doing
  // encoding.
  config.g_profile = 2;

  // Using 2 threads gives a great boost in performance for most systems with
  // adequate processing power. NB: Going to multiple threads on low end
  // windows systems can really hurt performance.
  // http://crbug.com/99179
  config.g_threads = (base::SysInfo::NumberOfProcessors() > 2) ? 2 : 1;
  config.rc_min_quantizer = 20;
  config.rc_max_quantizer = 30;
  config.g_timebase.num = 1;
  config.g_timebase.den = 20;

  if (vpx_codec_enc_init(codec.get(), algo, &config, 0))
    return ScopedVpxCodec();

  // Value of 16 will have the smallest CPU load. This turns off subpixel
  // motion search.
  if (vpx_codec_control(codec.get(), VP8E_SET_CPUUSED, 16))
    return ScopedVpxCodec();

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  if (vpx_codec_control(codec.get(), VP8E_SET_NOISE_SENSITIVITY, 0))
    return ScopedVpxCodec();

  return codec.Pass();
}

ScopedVpxCodec CreateVP9Codec(const webrtc::DesktopSize& size) {
  ScopedVpxCodec codec(new vpx_codec_ctx_t);

  // Configure the encoder.
  vpx_codec_enc_cfg_t config;
  const vpx_codec_iface_t* algo = vpx_codec_vp9_cx();
  CHECK(algo);
  vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
  if (ret != VPX_CODEC_OK)
    return ScopedVpxCodec();

  //config.rc_target_bitrate = size.width() * size.height() *
  //    config.rc_target_bitrate / config.g_w / config.g_h;
  config.g_w = size.width();
  config.g_h = size.height();
  config.g_pass = VPX_RC_ONE_PASS;

  // Only the default profile is currently supported for VP9 encoding.
  config.g_profile = 0;

  // Start emitting packets immediately.
  config.g_lag_in_frames = 0;

  // Prevent VP9 from ruining output quality with quantization.
  config.rc_max_quantizer = 0;

  if (vpx_codec_enc_init(codec.get(), algo, &config, 0))
    return ScopedVpxCodec();

  // VP9 encode doesn't yet support Realtime, so falls back to Good quality,
  // for which 4 is the lowest CPU usage.
  // Note that this is configured via the same parameter as for VP8.
  if (vpx_codec_control(codec.get(), VP8E_SET_CPUUSED, 4))
    return ScopedVpxCodec();

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  // Note that this is configured via the same parameter as for VP8.
  if (vpx_codec_control(codec.get(), VP8E_SET_NOISE_SENSITIVITY, 0))
    return ScopedVpxCodec();

  return codec.Pass();
}

}  // namespace

// static
scoped_ptr<VideoEncoderVpx> VideoEncoderVpx::CreateForVP8() {
  return scoped_ptr<VideoEncoderVpx>(
      new VideoEncoderVpx(base::Bind(&CreateVP8Codec)));
}

// static
scoped_ptr<VideoEncoderVpx> VideoEncoderVpx::CreateForVP9() {
  return scoped_ptr<VideoEncoderVpx>(
      new VideoEncoderVpx(base::Bind(&CreateVP9Codec)));
}

VideoEncoderVpx::~VideoEncoderVpx() {}

scoped_ptr<VideoPacket> VideoEncoderVpx::Encode(
    const webrtc::DesktopFrame& frame) {
  DCHECK_LE(32, frame.size().width());
  DCHECK_LE(32, frame.size().height());

  base::Time encode_start_time = base::Time::Now();

  if (!codec_ ||
      !frame.size().equals(webrtc::DesktopSize(image_->w, image_->h))) {
    bool ret = Initialize(frame.size());
    // TODO(hclam): Handle error better.
    CHECK(ret) << "Initialization of encoder failed";
  }

  // Convert the updated capture data ready for encode.
  webrtc::DesktopRegion updated_region;
  PrepareImage(frame, &updated_region);

  // Update active map based on updated region.
  PrepareActiveMap(updated_region);

  // Apply active map to the encoder.
  vpx_active_map_t act_map;
  act_map.rows = active_map_height_;
  act_map.cols = active_map_width_;
  act_map.active_map = active_map_.get();
  if (vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &act_map)) {
    LOG(ERROR) << "Unable to apply active map";
  }

  // Do the actual encoding.
  vpx_codec_err_t ret = vpx_codec_encode(codec_.get(), image_.get(),
                                         last_timestamp_,
                                         1, 0, VPX_DL_REALTIME);
  DCHECK_EQ(ret, VPX_CODEC_OK)
      << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
      << "Details: " << vpx_codec_error(codec_.get()) << "\n"
      << vpx_codec_error_detail(codec_.get());

  // TODO(hclam): Apply the proper timestamp here.
  last_timestamp_ += 50;

  // Read the encoded data.
  vpx_codec_iter_t iter = NULL;
  bool got_data = false;

  // TODO(hclam): Make sure we get exactly one frame from the packet.
  // TODO(hclam): We should provide the output buffer to avoid one copy.
  scoped_ptr<VideoPacket> packet(new VideoPacket());

  while (!got_data) {
    const vpx_codec_cx_pkt_t* vpx_packet =
        vpx_codec_get_cx_data(codec_.get(), &iter);
    if (!vpx_packet)
      continue;

    switch (vpx_packet->kind) {
      case VPX_CODEC_CX_FRAME_PKT:
        got_data = true;
        packet->set_data(vpx_packet->data.frame.buf, vpx_packet->data.frame.sz);
        break;
      default:
        break;
    }
  }

  // Construct the VideoPacket message.
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);
  packet->mutable_format()->set_screen_width(frame.size().width());
  packet->mutable_format()->set_screen_height(frame.size().height());
  packet->set_capture_time_ms(frame.capture_time_ms());
  packet->set_encode_time_ms(
      (base::Time::Now() - encode_start_time).InMillisecondsRoundedUp());
  if (!frame.dpi().is_zero()) {
    packet->mutable_format()->set_x_dpi(frame.dpi().x());
    packet->mutable_format()->set_y_dpi(frame.dpi().y());
  }
  for (webrtc::DesktopRegion::Iterator r(updated_region); !r.IsAtEnd();
       r.Advance()) {
    Rect* rect = packet->add_dirty_rects();
    rect->set_x(r.rect().left());
    rect->set_y(r.rect().top());
    rect->set_width(r.rect().width());
    rect->set_height(r.rect().height());
  }

  return packet.Pass();
}

VideoEncoderVpx::VideoEncoderVpx(const InitializeCodecCallback& init_codec)
    : init_codec_(init_codec),
      active_map_width_(0),
      active_map_height_(0),
      last_timestamp_(0) {
}

bool VideoEncoderVpx::Initialize(const webrtc::DesktopSize& size) {
  codec_.reset();

  image_.reset(new vpx_image_t());
  memset(image_.get(), 0, sizeof(vpx_image_t));

  image_->fmt = VPX_IMG_FMT_YV12;

  // libvpx seems to require both to be assigned.
  image_->d_w = size.width();
  image_->w = size.width();
  image_->d_h = size.height();
  image_->h = size.height();

  // libvpx should derive this from|fmt| but currently has a bug:
  // https://code.google.com/p/webm/issues/detail?id=627
  image_->x_chroma_shift = 1;
  image_->y_chroma_shift = 1;

  // Initialize active map.
  active_map_width_ = (image_->w + kMacroBlockSize - 1) / kMacroBlockSize;
  active_map_height_ = (image_->h + kMacroBlockSize - 1) / kMacroBlockSize;
  active_map_.reset(new uint8[active_map_width_ * active_map_height_]);

  // YUV image size is 1.5 times of a plane. Multiplication is performed first
  // to avoid rounding error.
  const int y_plane_size = image_->w * image_->h;
  const int uv_width = (image_->w + 1) / 2;
  const int uv_height = (image_->h + 1) / 2;
  const int uv_plane_size = uv_width * uv_height;
  const int yuv_image_size = y_plane_size + uv_plane_size * 2;

  // libvpx may try to access memory after the buffer (it still
  // doesn't use it) - it copies the data in 16x16 blocks:
  // crbug.com/119633 . Here we workaround that problem by adding
  // padding at the end of the buffer. Overreading to U and V buffers
  // is safe so the padding is necessary only at the end.
  //
  // TODO(sergeyu): Remove this padding when the bug is fixed in libvpx.
  const int active_map_area = active_map_width_ * kMacroBlockSize *
      active_map_height_ * kMacroBlockSize;
  const int padding_size = active_map_area - y_plane_size;
  const int buffer_size = yuv_image_size + padding_size;

  yuv_image_.reset(new uint8[buffer_size]);

  // Reset image value to 128 so we just need to fill in the y plane.
  memset(yuv_image_.get(), 128, yuv_image_size);

  // Fill in the information for |image_|.
  unsigned char* image = reinterpret_cast<unsigned char*>(yuv_image_.get());
  image_->planes[0] = image;
  image_->planes[1] = image + y_plane_size;
  image_->planes[2] = image + y_plane_size + uv_plane_size;
  image_->stride[0] = image_->w;
  image_->stride[1] = uv_width;
  image_->stride[2] = uv_width;

  // Initialize the codec.
  codec_ = init_codec_.Run(size);

  return codec_;
}

void VideoEncoderVpx::PrepareImage(const webrtc::DesktopFrame& frame,
                                   webrtc::DesktopRegion* updated_region) {
  if (frame.updated_region().is_empty()) {
    updated_region->Clear();
    return;
  }

  // Align the region to macroblocks, to avoid encoding artefacts.
  // This also ensures that all rectangles have even-aligned top-left, which
  // is required for ConvertRGBToYUVWithRect() to work.
  std::vector<webrtc::DesktopRect> aligned_rects;
  for (webrtc::DesktopRegion::Iterator r(frame.updated_region());
       !r.IsAtEnd(); r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    aligned_rects.push_back(AlignRect(webrtc::DesktopRect::MakeLTRB(
        rect.left(), rect.top(), rect.right(), rect.bottom())));
  }
  DCHECK(!aligned_rects.empty());
  updated_region->Clear();
  updated_region->AddRects(&aligned_rects[0], aligned_rects.size());

  // Clip back to the screen dimensions, in case they're not macroblock aligned.
  // The conversion routines don't require even width & height, so this is safe
  // even if the source dimensions are not even.
  updated_region->IntersectWith(
      webrtc::DesktopRect::MakeWH(image_->w, image_->h));

  // Convert the updated region to YUV ready for encoding.
  const uint8* rgb_data = frame.data();
  const int rgb_stride = frame.stride();
  const int y_stride = image_->stride[0];
  DCHECK_EQ(image_->stride[1], image_->stride[2]);
  const int uv_stride = image_->stride[1];
  uint8* y_data = image_->planes[0];
  uint8* u_data = image_->planes[1];
  uint8* v_data = image_->planes[2];
  for (webrtc::DesktopRegion::Iterator r(*updated_region); !r.IsAtEnd();
       r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    ConvertRGB32ToYUVWithRect(
        rgb_data, y_data, u_data, v_data,
        rect.left(), rect.top(), rect.width(), rect.height(),
        rgb_stride, y_stride, uv_stride);
  }
}

void VideoEncoderVpx::PrepareActiveMap(
    const webrtc::DesktopRegion& updated_region) {
  // Clear active map first.
  memset(active_map_.get(), 0, active_map_width_ * active_map_height_);

  // Mark updated areas active.
  for (webrtc::DesktopRegion::Iterator r(updated_region); !r.IsAtEnd();
       r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    int left = rect.left() / kMacroBlockSize;
    int right = (rect.right() - 1) / kMacroBlockSize;
    int top = rect.top() / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;
    DCHECK_LT(right, active_map_width_);
    DCHECK_LT(bottom, active_map_height_);

    uint8* map = active_map_.get() + top * active_map_width_;
    for (int y = top; y <= bottom; ++y) {
      for (int x = left; x <= right; ++x)
        map[x] = 1;
      map += active_map_width_;
    }
  }
}

}  // namespace remoting
