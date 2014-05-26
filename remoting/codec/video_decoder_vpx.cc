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

namespace {

const uint32 kTransparentColor = 0;

// Fills the rectangle |rect| with the given ARGB color |color| in |buffer|.
void FillRect(uint8* buffer,
              int stride,
              const webrtc::DesktopRect& rect,
              uint32 color) {
  uint32* ptr = reinterpret_cast<uint32*>(buffer + (rect.top() * stride) +
      (rect.left() * VideoDecoder::kBytesPerPixel));
  int width = rect.width();
  for (int height = rect.height(); height > 0; --height) {
    std::fill(ptr, ptr + width, color);
    ptr += stride / VideoDecoder::kBytesPerPixel;
  }
}

} // namespace

// static
scoped_ptr<VideoDecoderVpx> VideoDecoderVpx::CreateForVP8() {
  ScopedVpxCodec codec(new vpx_codec_ctx_t);

  // TODO(hclam): Scale the number of threads with number of cores of the
  // machine.
  vpx_codec_dec_cfg config;
  config.w = 0;
  config.h = 0;
  config.threads = 2;
  vpx_codec_err_t ret =
      vpx_codec_dec_init(codec.get(), vpx_codec_vp8_dx(), &config, 0);
  if (ret != VPX_CODEC_OK) {
    LOG(ERROR) << "Cannot initialize codec.";
    return scoped_ptr<VideoDecoderVpx>();
  }

  return scoped_ptr<VideoDecoderVpx>(new VideoDecoderVpx(codec.Pass()));
}

// static
scoped_ptr<VideoDecoderVpx> VideoDecoderVpx::CreateForVP9() {
  ScopedVpxCodec codec(new vpx_codec_ctx_t);

  // TODO(hclam): Scale the number of threads with number of cores of the
  // machine.
  vpx_codec_dec_cfg config;
  config.w = 0;
  config.h = 0;
  config.threads = 2;
  vpx_codec_err_t ret =
      vpx_codec_dec_init(codec.get(), vpx_codec_vp9_dx(), &config, 0);
  if (ret != VPX_CODEC_OK) {
    LOG(ERROR) << "Cannot initialize codec.";
    return scoped_ptr<VideoDecoderVpx>();
  }

  return scoped_ptr<VideoDecoderVpx>(new VideoDecoderVpx(codec.Pass()));
}

VideoDecoderVpx::~VideoDecoderVpx() {}

void VideoDecoderVpx::Initialize(const webrtc::DesktopSize& screen_size) {
  DCHECK(!screen_size.is_empty());

  screen_size_ = screen_size;

  transparent_region_.SetRect(webrtc::DesktopRect::MakeSize(screen_size_));
}

bool VideoDecoderVpx::DecodePacket(const VideoPacket& packet) {
  DCHECK(!screen_size_.is_empty());

  // Do the actual decoding.
  vpx_codec_err_t ret = vpx_codec_decode(
      codec_.get(), reinterpret_cast<const uint8*>(packet.data().data()),
      packet.data().size(), NULL, 0);
  if (ret != VPX_CODEC_OK) {
    const char* error = vpx_codec_error(codec_.get());
    const char* error_detail = vpx_codec_error_detail(codec_.get());
    LOG(ERROR) << "Decoding failed:" << (error ? error : "(NULL)") << "\n"
               << "Details: " << (error_detail ? error_detail : "(NULL)");
    return false;
  }

  // Gets the decoded data.
  vpx_codec_iter_t iter = NULL;
  vpx_image_t* image = vpx_codec_get_frame(codec_.get(), &iter);
  if (!image) {
    LOG(ERROR) << "No video frame decoded";
    return false;
  }
  last_image_ = image;

  webrtc::DesktopRegion region;
  for (int i = 0; i < packet.dirty_rects_size(); ++i) {
    Rect remoting_rect = packet.dirty_rects(i);
    region.AddRect(webrtc::DesktopRect::MakeXYWH(
        remoting_rect.x(), remoting_rect.y(),
        remoting_rect.width(), remoting_rect.height()));
  }

  updated_region_.AddRegion(region);

  // Update the desktop shape region.
  webrtc::DesktopRegion desktop_shape_region;
  if (packet.has_use_desktop_shape()) {
    for (int i = 0; i < packet.desktop_shape_rects_size(); ++i) {
      Rect remoting_rect = packet.desktop_shape_rects(i);
      desktop_shape_region.AddRect(webrtc::DesktopRect::MakeXYWH(
          remoting_rect.x(), remoting_rect.y(),
          remoting_rect.width(), remoting_rect.height()));
    }
  } else {
    // Fallback for the case when the host didn't include the desktop shape
    // region.
    desktop_shape_region =
        webrtc::DesktopRegion(webrtc::DesktopRect::MakeSize(screen_size_));
  }

  UpdateImageShapeRegion(&desktop_shape_region);

  return true;
}

void VideoDecoderVpx::Invalidate(const webrtc::DesktopSize& view_size,
                                 const webrtc::DesktopRegion& region) {
  DCHECK(!view_size.is_empty());

  for (webrtc::DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    updated_region_.AddRect(ScaleRect(i.rect(), view_size, screen_size_));
  }

  // Updated areas outside of the new desktop shape region should be made
  // transparent, not repainted.
  webrtc::DesktopRegion difference = updated_region_;
  difference.Subtract(desktop_shape_);
  updated_region_.Subtract(difference);
  transparent_region_.AddRegion(difference);
}

void VideoDecoderVpx::RenderFrame(const webrtc::DesktopSize& view_size,
                                  const webrtc::DesktopRect& clip_area,
                                  uint8* image_buffer,
                                  int image_stride,
                                  webrtc::DesktopRegion* output_region) {
  DCHECK(!screen_size_.is_empty());
  DCHECK(!view_size.is_empty());

  // Early-return and do nothing if we haven't yet decoded any frames.
  if (!last_image_)
    return;

  webrtc::DesktopRect source_clip =
      webrtc::DesktopRect::MakeWH(last_image_->d_w, last_image_->d_h);

  // VP8 only outputs I420 frames, but VP9 can also produce I444.
  switch (last_image_->fmt) {
    case VPX_IMG_FMT_I444: {
      // TODO(wez): Add scaling support to the I444 conversion path.
      if (view_size.equals(screen_size_)) {
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
          int y_offset = last_image_->stride[0] * rect.top() + rect.left();
          int u_offset = last_image_->stride[1] * rect.top() + rect.left();
          int v_offset = last_image_->stride[2] * rect.top() + rect.left();
          libyuv::I444ToARGB(last_image_->planes[0] + y_offset,
                             last_image_->stride[0],
                             last_image_->planes[1] + u_offset,
                             last_image_->stride[1],
                             last_image_->planes[2] + v_offset,
                             last_image_->stride[2],
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
            ScaleRect(clip_area, view_size, screen_size_);
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
                                        last_image_->stride[0]);
        int uv_offset = CalculateUVOffset(source_rect.left(), source_rect.top(),
                                          last_image_->stride[1]);
        ScaleYUVToRGB32(last_image_->planes[0] + y_offset,
                        last_image_->planes[1] + uv_offset,
                        last_image_->planes[2] + uv_offset,
                        image_buffer,
                        source_rect.width(),
                        source_rect.height(),
                        clip_area.width(),
                        clip_area.height(),
                        last_image_->stride[0],
                        last_image_->stride[1],
                        image_stride,
                        media::YV12,
                        media::ROTATE_0,
                        media::FILTER_BILINEAR);

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
        rect = ScaleRect(rect, screen_size_, view_size);
        rect.IntersectWith(clip_area);
        if (rect.is_empty())
          continue;

        ConvertAndScaleYUVToRGB32Rect(last_image_->planes[0],
                                      last_image_->planes[1],
                                      last_image_->planes[2],
                                      last_image_->stride[0],
                                      last_image_->stride[1],
                                      screen_size_,
                                      source_clip,
                                      image_buffer,
                                      image_stride,
                                      view_size,
                                      clip_area,
                                      rect);

        output_region->AddRect(rect);
      }

      updated_region_.Subtract(ScaleRect(clip_area, view_size, screen_size_));
      break;
    }
    default: {
      LOG(ERROR) << "Unsupported image format:" << last_image_->fmt;
      return;
    }
  }

  for (webrtc::DesktopRegion::Iterator i(transparent_region_);
       !i.IsAtEnd(); i.Advance()) {
    // Determine the scaled area affected by this rectangle changing.
    webrtc::DesktopRect rect = i.rect();
    rect.IntersectWith(source_clip);
    if (rect.is_empty())
      continue;
    rect = ScaleRect(rect, screen_size_, view_size);
    rect.IntersectWith(clip_area);
    if (rect.is_empty())
      continue;

    // Fill the rectange with transparent pixels.
    FillRect(image_buffer, image_stride, rect, kTransparentColor);
    output_region->AddRect(rect);
  }

  webrtc::DesktopRect scaled_clip_area =
      ScaleRect(clip_area, view_size, screen_size_);
  updated_region_.Subtract(scaled_clip_area);
  transparent_region_.Subtract(scaled_clip_area);
}

const webrtc::DesktopRegion* VideoDecoderVpx::GetImageShape() {
  return &desktop_shape_;
}

VideoDecoderVpx::VideoDecoderVpx(ScopedVpxCodec codec)
    : codec_(codec.Pass()),
      last_image_(NULL) {
  DCHECK(codec_);
}

void VideoDecoderVpx::UpdateImageShapeRegion(
    webrtc::DesktopRegion* new_desktop_shape) {
  // Add all areas that have been updated or become transparent to the
  // transparent region. Exclude anything within the new desktop shape.
  transparent_region_.AddRegion(desktop_shape_);
  transparent_region_.AddRegion(updated_region_);
  transparent_region_.Subtract(*new_desktop_shape);

  // Add newly exposed areas to the update region and limit updates to the new
  // desktop shape.
  webrtc::DesktopRegion difference = *new_desktop_shape;
  difference.Subtract(desktop_shape_);
  updated_region_.AddRegion(difference);
  updated_region_.IntersectWith(*new_desktop_shape);

  // Set the new desktop shape region.
  desktop_shape_.Swap(new_desktop_shape);
}

}  // namespace remoting
