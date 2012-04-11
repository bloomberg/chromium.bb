// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webvideoframe_impl.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"

using WebKit::WebVideoFrame;

namespace webkit_media {

media::VideoFrame* WebVideoFrameImpl::toVideoFrame(
    WebVideoFrame* web_video_frame) {
  WebVideoFrameImpl* wrapped_frame =
      static_cast<WebVideoFrameImpl*>(web_video_frame);
  if (wrapped_frame)
    return wrapped_frame->video_frame_.get();
  return NULL;
}

WebVideoFrameImpl::WebVideoFrameImpl(
    scoped_refptr<media::VideoFrame> video_frame)
    : video_frame_(video_frame) {
}

WebVideoFrameImpl::~WebVideoFrameImpl() {}

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, chromium_name) \
    COMPILE_ASSERT(int(WebKit::WebVideoFrame::webkit_name) == \
                   int(media::VideoFrame::chromium_name), \
                   mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(FormatInvalid, INVALID);
COMPILE_ASSERT_MATCHING_ENUM(FormatRGB32, RGB32);
COMPILE_ASSERT_MATCHING_ENUM(FormatYV12, YV12);
COMPILE_ASSERT_MATCHING_ENUM(FormatYV16, YV16);
COMPILE_ASSERT_MATCHING_ENUM(FormatEmpty, EMPTY);
COMPILE_ASSERT_MATCHING_ENUM(FormatI420, I420);
COMPILE_ASSERT_MATCHING_ENUM(FormatNativeTexture, NATIVE_TEXTURE);

WebVideoFrame::Format WebVideoFrameImpl::format() const {
  if (video_frame_.get())
    return static_cast<WebVideoFrame::Format>(video_frame_->format());
  return WebVideoFrame::FormatInvalid;
}

unsigned WebVideoFrameImpl::width() const {
  if (video_frame_.get())
    return video_frame_->width();
  return 0;
}

unsigned WebVideoFrameImpl::height() const {
  if (video_frame_.get())
    return video_frame_->height();
  return 0;
}

unsigned WebVideoFrameImpl::planes() const {
  if (!video_frame_.get())
    return 0;
  switch (video_frame_->format()) {
    case media::VideoFrame::RGB32:
      return 1;
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV16:
      return 3;
    case media::VideoFrame::INVALID:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::I420:
      break;
    case media::VideoFrame::NATIVE_TEXTURE:
      return 0;
  }
  NOTREACHED();
  return 0;
}

int WebVideoFrameImpl::stride(unsigned plane) const {
  if (video_frame_.get())
    return static_cast<int>(video_frame_->stride(plane));
  return 0;
}

const void* WebVideoFrameImpl::data(unsigned plane) const {
  if (!video_frame_.get() || format() == FormatNativeTexture)
    return NULL;
  return static_cast<const void*>(video_frame_->data(plane));
}

unsigned WebVideoFrameImpl::textureId() const {
  if (!video_frame_.get() || format() != FormatNativeTexture)
    return 0;
  return video_frame_->texture_id();
}

unsigned WebVideoFrameImpl::textureTarget() const {
  if (!video_frame_.get() || format() != FormatNativeTexture)
    return 0;
  return video_frame_->texture_target();
}

}  // namespace webkit_media
