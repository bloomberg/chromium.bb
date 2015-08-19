// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_vpx.h"

#include <math.h>

#include <algorithm>

#include "base/logging.h"
#include "media/base/media.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
}

namespace remoting {

// static
scoped_ptr<VideoDecoderVpx> VideoDecoderVpx::CreateForVP8() {
  return make_scoped_ptr(new VideoDecoderVpx(vpx_codec_vp8_dx()));
}

// static
scoped_ptr<VideoDecoderVpx> VideoDecoderVpx::CreateForVP9() {
  return make_scoped_ptr(new VideoDecoderVpx(vpx_codec_vp9_dx()));
}

VideoDecoderVpx::~VideoDecoderVpx() {}

bool VideoDecoderVpx::DecodePacket(const VideoPacket& packet) {
  // Pass the packet to the codec to process.
  vpx_codec_err_t ret = vpx_codec_decode(
      codec_.get(), reinterpret_cast<const uint8*>(packet.data().data()),
      packet.data().size(), nullptr, 0);
  if (ret != VPX_CODEC_OK) {
    const char* error = vpx_codec_error(codec_.get());
    const char* error_detail = vpx_codec_error_detail(codec_.get());
    LOG(ERROR) << "Decoding failed:" << (error ? error : "(NULL)") << "\n"
               << "Details: " << (error_detail ? error_detail : "(NULL)");
    return false;
  }

  // Fetch the decoded video frame.
  vpx_codec_iter_t iter = nullptr;
  image_ = vpx_codec_get_frame(codec_.get(), &iter);
  if (!image_) {
    LOG(ERROR) << "No video frame decoded";
    return false;
  }
  DCHECK(!image_size().is_empty());

  // Determine which areas have been updated.
  webrtc::DesktopRegion region;
  for (int i = 0; i < packet.dirty_rects_size(); ++i) {
    Rect remoting_rect = packet.dirty_rects(i);
    region.AddRect(webrtc::DesktopRect::MakeXYWH(
        remoting_rect.x(), remoting_rect.y(),
        remoting_rect.width(), remoting_rect.height()));
  }
  updated_region_.AddRegion(region);

  // Process the frame shape, if supplied.
  if (packet.has_use_desktop_shape()) {
    if (packet.use_desktop_shape()) {
      if (!desktop_shape_)
        desktop_shape_ = make_scoped_ptr(new webrtc::DesktopRegion);
      desktop_shape_->Clear();
      for (int i = 0; i < packet.desktop_shape_rects_size(); ++i) {
        Rect remoting_rect = packet.desktop_shape_rects(i);
        desktop_shape_->AddRect(webrtc::DesktopRect::MakeXYWH(
            remoting_rect.x(), remoting_rect.y(), remoting_rect.width(),
            remoting_rect.height()));
      }
    } else {
      desktop_shape_.reset();
    }
  }

  return true;
}

void VideoDecoderVpx::Invalidate(const webrtc::DesktopSize& view_size,
                                 const webrtc::DesktopRegion& region) {
  DCHECK(!view_size.is_empty());

  for (webrtc::DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    updated_region_.AddRect(ScaleRect(i.rect(), view_size, image_size()));
  }
}

void VideoDecoderVpx::RenderFrame(const webrtc::DesktopSize& view_size,
                                  const webrtc::DesktopRect& clip_area,
                                  uint8* image_buffer,
                                  int image_stride,
                                  webrtc::DesktopRegion* output_region) {
  DCHECK(!image_size().is_empty());
  DCHECK(!view_size.is_empty());

  // Early-return and do nothing if we haven't yet decoded any frames.
  if (!image_)
    return;

  webrtc::DesktopRect source_clip = webrtc::DesktopRect::MakeSize(image_size());

  // VP8 only outputs I420 frames, but VP9 can also produce I444.
  switch (image_->fmt) {
    case VPX_IMG_FMT_I444: {
      // TODO(wez): Add scaling support to the I444 conversion path.
      if (view_size.equals(image_size())) {
        for (webrtc::DesktopRegion::Iterator i(updated_region_);
             !i.IsAtEnd(); i.Advance()) {
          // Determine the scaled area affected by this rectangle changing.
          webrtc::DesktopRect rect = i.rect();
          rect.IntersectWith(source_clip);
          rect.IntersectWith(clip_area);
          if (rect.is_empty())
            continue;

          int image_offset = image_stride * rect.top() +
                             rect.left() * VideoDecoder::kBytesPerPixel;
          int y_offset = image_->stride[0] * rect.top() + rect.left();
          int u_offset = image_->stride[1] * rect.top() + rect.left();
          int v_offset = image_->stride[2] * rect.top() + rect.left();
          libyuv::I444ToARGB(image_->planes[0] + y_offset, image_->stride[0],
                             image_->planes[1] + u_offset, image_->stride[1],
                             image_->planes[2] + v_offset, image_->stride[2],
                             image_buffer + image_offset, image_stride,
                             rect.width(), rect.height());

          output_region->AddRect(rect);
        }
      }
      break;
    }
    case VPX_IMG_FMT_I420: {
      // ScaleYUVToRGB32WithRect does not currently support up-scaling.  We
      // won't be asked to up-scale except during resizes or if page zoom is
      // >100%, so we work-around the limitation by using the slower
      // ScaleYUVToRGB32.
      // TODO(wez): Remove this hack if/when ScaleYUVToRGB32WithRect can
      // up-scale.
      if (!updated_region_.is_empty() &&
          (source_clip.width() < view_size.width() ||
           source_clip.height() < view_size.height())) {
        // We're scaling only |clip_area| into the |image_buffer|, so we need to
        // work out which source rectangle that corresponds to.
        webrtc::DesktopRect source_rect =
            ScaleRect(clip_area, view_size, image_size());
        source_rect = webrtc::DesktopRect::MakeLTRB(
            RoundToTwosMultiple(source_rect.left()),
            RoundToTwosMultiple(source_rect.top()),
            source_rect.right(),
            source_rect.bottom());

        // If there were no changes within the clip source area then don't
        // render.
        webrtc::DesktopRegion intersection(source_rect);
        intersection.IntersectWith(updated_region_);
        if (intersection.is_empty())
          return;

        // Scale & convert the entire clip area.
        int y_offset = CalculateYOffset(source_rect.left(), source_rect.top(),
                                        image_->stride[0]);
        int uv_offset = CalculateUVOffset(source_rect.left(), source_rect.top(),
                                          image_->stride[1]);
        ScaleYUVToRGB32(
            image_->planes[0] + y_offset, image_->planes[1] + uv_offset,
            image_->planes[2] + uv_offset, image_buffer, source_rect.width(),
            source_rect.height(), clip_area.width(), clip_area.height(),
            image_->stride[0], image_->stride[1], image_stride, media::YV12,
            media::ROTATE_0, media::FILTER_BILINEAR);

        output_region->AddRect(clip_area);
        updated_region_.Subtract(source_rect);
        return;
      }

      for (webrtc::DesktopRegion::Iterator i(updated_region_);
           !i.IsAtEnd(); i.Advance()) {
        // Determine the scaled area affected by this rectangle changing.
        webrtc::DesktopRect rect = i.rect();
        rect.IntersectWith(source_clip);
        if (rect.is_empty())
          continue;
        rect = ScaleRect(rect, image_size(), view_size);
        rect.IntersectWith(clip_area);
        if (rect.is_empty())
          continue;

        ConvertAndScaleYUVToRGB32Rect(
            image_->planes[0], image_->planes[1], image_->planes[2],
            image_->stride[0], image_->stride[1], image_size(), source_clip,
            image_buffer, image_stride, view_size, clip_area, rect);

        output_region->AddRect(rect);
      }

      updated_region_.Subtract(ScaleRect(clip_area, view_size, image_size()));
      break;
    }
    default: {
      LOG(ERROR) << "Unsupported image format:" << image_->fmt;
      return;
    }
  }

  webrtc::DesktopRect scaled_clip_area =
      ScaleRect(clip_area, view_size, image_size());
  updated_region_.Subtract(scaled_clip_area);
}

const webrtc::DesktopRegion* VideoDecoderVpx::GetImageShape() {
  return desktop_shape_.get();
}

VideoDecoderVpx::VideoDecoderVpx(vpx_codec_iface_t* codec) : image_(nullptr) {
  codec_.reset(new vpx_codec_ctx_t);

  vpx_codec_dec_cfg config;
  config.w = 0;
  config.h = 0;
  config.threads = 2;
  vpx_codec_err_t ret = vpx_codec_dec_init(codec_.get(), codec, &config, 0);
  CHECK_EQ(VPX_CODEC_OK, ret);
}

webrtc::DesktopSize VideoDecoderVpx::image_size() const {
  return image_ ? webrtc::DesktopSize(image_->d_w, image_->d_h)
                : webrtc::DesktopSize();
}

}  // namespace remoting
