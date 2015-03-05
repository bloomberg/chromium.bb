// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_vpx.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
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

// Name of command-line flag to enable VP9 to use I444 by default.
const char kEnableI444SwitchName[] = "enable-i444";

// Number of bytes in an RGBx pixel.
const int kBytesPerRgbPixel = 4;

// Defines the dimension of a macro block. This is used to compute the active
// map for the encoder.
const int kMacroBlockSize = 16;

// Magic encoder profile numbers for I420 and I444 input formats.
const int kVp9I420ProfileNumber = 0;
const int kVp9I444ProfileNumber = 1;

void SetCommonCodecParameters(vpx_codec_enc_cfg_t* config,
                              const webrtc::DesktopSize& size) {
  // Use millisecond granularity time base.
  config->g_timebase.num = 1;
  config->g_timebase.den = 1000;

  // Adjust default target bit-rate to account for actual desktop size.
  config->rc_target_bitrate = size.width() * size.height() *
      config->rc_target_bitrate / config->g_w / config->g_h;

  config->g_w = size.width();
  config->g_h = size.height();
  config->g_pass = VPX_RC_ONE_PASS;

  // Start emitting packets immediately.
  config->g_lag_in_frames = 0;

  // Since the transport layer is reliable, keyframes should not be necessary.
  // However, due to crbug.com/440223, decoding fails after 30,000 non-key
  // frames, so take the hit of an "unnecessary" key-frame every 10,000 frames.
  config->kf_min_dist = 10000;
  config->kf_max_dist = 10000;

  // Using 2 threads gives a great boost in performance for most systems with
  // adequate processing power. NB: Going to multiple threads on low end
  // windows systems can really hurt performance.
  // http://crbug.com/99179
  config->g_threads = (base::SysInfo::NumberOfProcessors() > 2) ? 2 : 1;
}

void SetVp8CodecParameters(vpx_codec_enc_cfg_t* config,
                           const webrtc::DesktopSize& size) {
  SetCommonCodecParameters(config, size);

  // Value of 2 means using the real time profile. This is basically a
  // redundant option since we explicitly select real time mode when doing
  // encoding.
  config->g_profile = 2;

  // Clamping the quantizer constrains the worst-case quality and CPU usage.
  config->rc_min_quantizer = 20;
  config->rc_max_quantizer = 30;
}

void SetVp9CodecParameters(vpx_codec_enc_cfg_t* config,
                           const webrtc::DesktopSize& size,
                           bool lossless_color,
                           bool lossless_encode) {
  SetCommonCodecParameters(config, size);

  // Configure VP9 for I420 or I444 source frames.
  config->g_profile =
      lossless_color ? kVp9I444ProfileNumber : kVp9I420ProfileNumber;

  if (lossless_encode) {
    // Disable quantization entirely, putting the encoder in "lossless" mode.
    config->rc_min_quantizer = 0;
    config->rc_max_quantizer = 0;
  } else {
    // Lossy encode using the same settings as for VP8.
    config->rc_min_quantizer = 20;
    config->rc_max_quantizer = 30;
  }
}

void SetVp8CodecOptions(vpx_codec_ctx_t* codec) {
  // CPUUSED of 16 will have the smallest CPU load. This turns off sub-pixel
  // motion search.
  vpx_codec_err_t ret = vpx_codec_control(codec, VP8E_SET_CPUUSED, 16);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set CPUUSED";

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  ret = vpx_codec_control(codec, VP8E_SET_NOISE_SENSITIVITY, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set noise sensitivity";
}

void SetVp9CodecOptions(vpx_codec_ctx_t* codec, bool lossless_encode) {
  // Request the lowest-CPU usage that VP9 supports, which depends on whether
  // we are encoding lossy or lossless.
  // Note that this is configured via the same parameter as for VP8.
  int cpu_used = lossless_encode ? 5 : 6;
  vpx_codec_err_t ret = vpx_codec_control(codec, VP8E_SET_CPUUSED, cpu_used);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set CPUUSED";

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  ret = vpx_codec_control(codec, VP9E_SET_NOISE_SENSITIVITY, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set noise sensitivity";

  // Configure the codec to tune it for screen media.
  ret = vpx_codec_control(
      codec, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set screen content mode";
}

void CreateImage(bool use_i444,
                 const webrtc::DesktopSize& size,
                 scoped_ptr<vpx_image_t>* out_image,
                 scoped_ptr<uint8[]>* out_image_buffer) {
  DCHECK(!size.is_empty());

  scoped_ptr<vpx_image_t> image(new vpx_image_t());
  memset(image.get(), 0, sizeof(vpx_image_t));

  // libvpx seems to require both to be assigned.
  image->d_w = size.width();
  image->w = size.width();
  image->d_h = size.height();
  image->h = size.height();

  // libvpx should derive chroma shifts from|fmt| but currently has a bug:
  // https://code.google.com/p/webm/issues/detail?id=627
  if (use_i444) {
    image->fmt = VPX_IMG_FMT_I444;
    image->x_chroma_shift = 0;
    image->y_chroma_shift = 0;
  } else { // I420
    image->fmt = VPX_IMG_FMT_YV12;
    image->x_chroma_shift = 1;
    image->y_chroma_shift = 1;
  }

  // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad
  // the Y, U and V planes' strides to multiples of 16 bytes.
  const int y_stride = ((image->w - 1) & ~15) + 16;
  const int uv_unaligned_stride = y_stride >> image->x_chroma_shift;
  const int uv_stride = ((uv_unaligned_stride - 1) & ~15) + 16;

  // libvpx accesses the source image in macro blocks, and will over-read
  // if the image is not padded out to the next macroblock: crbug.com/119633.
  // Pad the Y, U and V planes' height out to compensate.
  // Assuming macroblocks are 16x16, aligning the planes' strides above also
  // macroblock aligned them.
  COMPILE_ASSERT(kMacroBlockSize == 16, macroblock_size_not_16);
  const int y_rows = ((image->h - 1) & ~(kMacroBlockSize-1)) + kMacroBlockSize;
  const int uv_rows = y_rows >> image->y_chroma_shift;

  // Allocate a YUV buffer large enough for the aligned data & padding.
  const int buffer_size = y_stride * y_rows + 2*uv_stride * uv_rows;
  scoped_ptr<uint8[]> image_buffer(new uint8[buffer_size]);

  // Reset image value to 128 so we just need to fill in the y plane.
  memset(image_buffer.get(), 128, buffer_size);

  // Fill in the information for |image_|.
  unsigned char* uchar_buffer =
      reinterpret_cast<unsigned char*>(image_buffer.get());
  image->planes[0] = uchar_buffer;
  image->planes[1] = image->planes[0] + y_stride * y_rows;
  image->planes[2] = image->planes[1] + uv_stride * uv_rows;
  image->stride[0] = y_stride;
  image->stride[1] = uv_stride;
  image->stride[2] = uv_stride;

  *out_image = image.Pass();
  *out_image_buffer = image_buffer.Pass();
}

} // namespace

// static
scoped_ptr<VideoEncoderVpx> VideoEncoderVpx::CreateForVP8() {
  return make_scoped_ptr(new VideoEncoderVpx(false));
}

// static
scoped_ptr<VideoEncoderVpx> VideoEncoderVpx::CreateForVP9() {
  return make_scoped_ptr(new VideoEncoderVpx(true));
}

VideoEncoderVpx::~VideoEncoderVpx() {}

void VideoEncoderVpx::SetLosslessEncode(bool want_lossless) {
  if (use_vp9_ && (want_lossless != lossless_encode_)) {
    lossless_encode_ = want_lossless;
    if (codec_)
      Configure(webrtc::DesktopSize(image_->w, image_->h));
  }
}

void VideoEncoderVpx::SetLosslessColor(bool want_lossless) {
  if (use_vp9_ && (want_lossless != lossless_color_)) {
    lossless_color_ = want_lossless;
    // TODO(wez): Switch to ConfigureCodec() path once libvpx supports it.
    // See https://code.google.com/p/webm/issues/detail?id=913.
    //if (codec_)
    //  Configure(webrtc::DesktopSize(image_->w, image_->h));
    codec_.reset();
  }
}

scoped_ptr<VideoPacket> VideoEncoderVpx::Encode(
    const webrtc::DesktopFrame& frame) {
  DCHECK_LE(32, frame.size().width());
  DCHECK_LE(32, frame.size().height());

  base::TimeTicks encode_start_time = base::TimeTicks::Now();

  // Create or reconfigure the codec to match the size of |frame|.
  if (!codec_ ||
      !frame.size().equals(webrtc::DesktopSize(image_->w, image_->h))) {
    Configure(frame.size());
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
  int timestamp = (encode_start_time - timestamp_base_).InMilliseconds();
  vpx_codec_err_t ret = vpx_codec_encode(
      codec_.get(), image_.get(), timestamp, 1, 0, VPX_DL_REALTIME);
  DCHECK_EQ(ret, VPX_CODEC_OK)
      << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
      << "Details: " << vpx_codec_error(codec_.get()) << "\n"
      << vpx_codec_error_detail(codec_.get());

  // Read the encoded data.
  vpx_codec_iter_t iter = NULL;
  bool got_data = false;

  // TODO(hclam): Make sure we get exactly one frame from the packet.
  // TODO(hclam): We should provide the output buffer to avoid one copy.
  scoped_ptr<VideoPacket> packet(
      helper_.CreateVideoPacketWithUpdatedRegion(frame, updated_region));
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);

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

  // Note the time taken to encode the pixel data.
  packet->set_encode_time_ms(
      (base::TimeTicks::Now() - encode_start_time).InMillisecondsRoundedUp());

  return packet.Pass();
}

VideoEncoderVpx::VideoEncoderVpx(bool use_vp9)
    : use_vp9_(use_vp9),
      lossless_encode_(false),
      lossless_color_(false),
      active_map_width_(0),
      active_map_height_(0) {
  if (use_vp9_) {
    // Use I444 colour space, by default, if specified on the command-line.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            kEnableI444SwitchName)) {
      SetLosslessColor(true);
    }
  }
}

void VideoEncoderVpx::Configure(const webrtc::DesktopSize& size) {
  DCHECK(use_vp9_ || !lossless_color_);
  DCHECK(use_vp9_ || !lossless_encode_);

  // (Re)Create the VPX image structure and pixel buffer.
  CreateImage(lossless_color_, size, &image_, &image_buffer_);

  // Initialize active map.
  active_map_width_ = (size.width() + kMacroBlockSize - 1) / kMacroBlockSize;
  active_map_height_ = (size.height() + kMacroBlockSize - 1) / kMacroBlockSize;
  active_map_.reset(new uint8[active_map_width_ * active_map_height_]);

  // TODO(wez): Remove this hack once VPX can handle frame size reconfiguration.
  // See https://code.google.com/p/webm/issues/detail?id=912.
  if (codec_) {
    // If the frame size has changed then force re-creation of the codec.
    if (codec_->config.enc->g_w != static_cast<unsigned int>(size.width()) ||
        codec_->config.enc->g_h != static_cast<unsigned int>(size.height())) {
      codec_.reset();
    }
  }

  // (Re)Set the base for frame timestamps if the codec is being (re)created.
  if (!codec_) {
    timestamp_base_ = base::TimeTicks::Now();
  }

  // Fetch a default configuration for the desired codec.
  const vpx_codec_iface_t* interface =
      use_vp9_ ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
  vpx_codec_enc_cfg_t config;
  vpx_codec_err_t ret = vpx_codec_enc_config_default(interface, &config, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to fetch default configuration";

  // Customize the default configuration to our needs.
  if (use_vp9_) {
    SetVp9CodecParameters(&config, size, lossless_color_, lossless_encode_);
  } else {
    SetVp8CodecParameters(&config, size);
  }

  // Initialize or re-configure the codec with the custom configuration.
  if (!codec_) {
    codec_.reset(new vpx_codec_ctx_t);
    ret = vpx_codec_enc_init(codec_.get(), interface, &config, 0);
    CHECK_EQ(VPX_CODEC_OK, ret) << "Failed to initialize codec";
  } else {
    ret = vpx_codec_enc_config_set(codec_.get(), &config);
    CHECK_EQ(VPX_CODEC_OK, ret) << "Failed to reconfigure codec";
  }

  // Apply further customizations to the codec now it's initialized.
  if (use_vp9_) {
    SetVp9CodecOptions(codec_.get(), lossless_encode_);
  } else {
    SetVp8CodecOptions(codec_.get());
  }
}

void VideoEncoderVpx::PrepareImage(const webrtc::DesktopFrame& frame,
                                   webrtc::DesktopRegion* updated_region) {
  if (frame.updated_region().is_empty()) {
    updated_region->Clear();
    return;
  }

  // Pad each rectangle to avoid the block-artefact filters in libvpx from
  // introducing artefacts; VP9 includes up to 8px either side, and VP8 up to
  // 3px, so unchanged pixels up to that far out may still be affected by the
  // changes in the updated region, and so must be listed in the active map.
  // After padding we align each rectangle to 16x16 active-map macroblocks.
  // This implicitly ensures all rects have even top-left coords, which is
  // is required by ConvertRGBToYUVWithRect().
  // TODO(wez): Do we still need 16x16 align, or is even alignment sufficient?
  updated_region->Clear();
  int padding = use_vp9_ ? 8 : 3;
  for (webrtc::DesktopRegion::Iterator r(frame.updated_region());
       !r.IsAtEnd(); r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    updated_region->AddRect(AlignRect(webrtc::DesktopRect::MakeLTRB(
        rect.left() - padding, rect.top() - padding, rect.right() + padding,
        rect.bottom() + padding)));
  }
  DCHECK(!updated_region->is_empty());

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

  switch (image_->fmt) {
    case VPX_IMG_FMT_I444:
      for (webrtc::DesktopRegion::Iterator r(*updated_region); !r.IsAtEnd();
           r.Advance()) {
        const webrtc::DesktopRect& rect = r.rect();
        int rgb_offset = rgb_stride * rect.top() +
                         rect.left() * kBytesPerRgbPixel;
        int yuv_offset = uv_stride * rect.top() + rect.left();
        libyuv::ARGBToI444(rgb_data + rgb_offset, rgb_stride,
                           y_data + yuv_offset, y_stride,
                           u_data + yuv_offset, uv_stride,
                           v_data + yuv_offset, uv_stride,
                           rect.width(), rect.height());
      }
      break;
    case VPX_IMG_FMT_YV12:
      for (webrtc::DesktopRegion::Iterator r(*updated_region); !r.IsAtEnd();
           r.Advance()) {
        const webrtc::DesktopRect& rect = r.rect();
        int rgb_offset = rgb_stride * rect.top() +
                         rect.left() * kBytesPerRgbPixel;
        int y_offset = y_stride * rect.top() + rect.left();
        int uv_offset = uv_stride * rect.top() / 2 + rect.left() / 2;
        libyuv::ARGBToI420(rgb_data + rgb_offset, rgb_stride,
                           y_data + y_offset, y_stride,
                           u_data + uv_offset, uv_stride,
                           v_data + uv_offset, uv_stride,
                           rect.width(), rect.height());
      }
      break;
    default:
      NOTREACHED();
      break;
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
