// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_MEDIA_SIMPLE_VIDEO_FRAME_PROVIDER_H_
#define WEBKIT_RENDERER_MEDIA_SIMPLE_VIDEO_FRAME_PROVIDER_H_

#include "base/time.h"
#include "ui/gfx/size.h"
#include "webkit/renderer/media/video_frame_provider.h"

namespace base {
class MessageLoopProxy;
}

namespace webkit_media {

// A simple implementation of VideoFrameProvider generates raw frames and
// passes them to webmediaplayer.
// Since non-black pixel values are required in the layout test, e.g.,
// media/video-capture-canvas.html, this class should generate frame with
// only non-black pixels.
class SimpleVideoFrameProvider : public VideoFrameProvider {
 public:
  SimpleVideoFrameProvider(
      const gfx::Size& size,
      const base::TimeDelta& frame_duration,
      const base::Closure& error_cb,
      const RepaintCB& repaint_cb);

  // VideoFrameProvider implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;

 protected:
  virtual ~SimpleVideoFrameProvider();

 private:
  enum State {
    kStarted,
    kPaused,
    kStopped,
  };

  void GenerateFrame();

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  gfx::Size size_;
  State state_;

  base::TimeDelta current_time_;
  base::TimeDelta frame_duration_;
  base::Closure error_cb_;
  RepaintCB repaint_cb_;

  DISALLOW_COPY_AND_ASSIGN(SimpleVideoFrameProvider);
};

}  // namespace webkit_media

#endif  // WEBKIT_RENDERER_MEDIA_SIMPLE_VIDEO_FRAME_PROVIDER_H_
