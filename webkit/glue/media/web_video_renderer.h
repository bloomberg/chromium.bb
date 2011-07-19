// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_WEB_VIDEO_RENDERER_H_
#define WEBKIT_GLUE_MEDIA_WEB_VIDEO_RENDERER_H_

#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"
#include "webkit/glue/webmediaplayer_impl.h"

namespace webkit_glue {

// A specialized version of a VideoRenderer designed to be used inside WebKit.
class WebVideoRenderer : public media::VideoRendererBase {
 public:
  WebVideoRenderer() : media::VideoRendererBase() {}
  virtual ~WebVideoRenderer() {}

  // Saves the reference to WebMediaPlayerImpl::Proxy.
  virtual void SetWebMediaPlayerImplProxy(WebMediaPlayerImpl::Proxy* proxy) = 0;

  // This method is called with the same rect as the Paint() method and could
  // be used by future implementations to implement an improved color space +
  // scale code on a separate thread.  Since we always do the stretch on the
  // same thread as the Paint method, we just ignore the call for now.
  //
  // Method called on the render thread.
  virtual void SetRect(const gfx::Rect& rect) = 0;

  // Paint the current front frame on the |canvas| stretching it to fit the
  // |dest_rect|.
  //
  // Method called on the render thread.
  virtual void Paint(SkCanvas* canvas,
                     const gfx::Rect& dest_rect) = 0;

  // Clients of this class (painter/compositor) should use GetCurrentFrame()
  // obtain ownership of VideoFrame, it should always relinquish the ownership
  // by use PutCurrentFrame(). Current frame is not guaranteed to be non-NULL.
  // It expects clients to use color-fill the background if current frame
  // is NULL. This could happen when before pipeline is pre-rolled or during
  // pause/flush/seek.
  virtual void GetCurrentFrame(scoped_refptr<media::VideoFrame>* frame_out) {}
  virtual void PutCurrentFrame(scoped_refptr<media::VideoFrame> frame) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebVideoRenderer);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_WEB_VIDEO_RENDERER_H_
