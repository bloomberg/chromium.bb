// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_VIDEO_RENDERER_IMPL_H_
#define WEBKIT_MEDIA_VIDEO_RENDERER_IMPL_H_

#include "base/callback.h"
#include "media/base/filters.h"
#include "media/filters/video_renderer_base.h"
#include "ui/gfx/rect.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkCanvas;

namespace webkit_media {

// The video renderer implementation used by the media pipeline. It runs
// primarily on the video renderer thread with the render thread used for
// painting.
//
// TODO(scherkus): don't extend VideoRendererBase and instead bind Paint()
// and pass to VideoRendererBase as a callback. This will be easier to do
// when http://crbug.com/108435 is fixed.
class VideoRendererImpl : public media::VideoRendererBase {
 public:
  // |paint_cb| is executed whenever a new frame is available for painting.
  // |set_opaque_cb| is used to imform the player whether the video is opaque.
  VideoRendererImpl(
      const base::Closure& paint_cb,
      const SetOpaqueCB& set_opaque_cb);
  virtual ~VideoRendererImpl();

  // Paint the current front frame on the |canvas| stretching it to fit the
  // |dest_rect|.
  //
  // Method called on the render thread.
  void Paint(SkCanvas* canvas, const gfx::Rect& dest_rect);

 private:
  // An RGB bitmap and corresponding timestamp of converted video frame data.
  SkBitmap bitmap_;
  base::TimeDelta bitmap_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_VIDEO_RENDERER_IMPL_H_
