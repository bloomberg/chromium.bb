// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/scoped_web_callbacks.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class WebMediaStreamTrack;

// TODO(crbug.com/945851): Avoid referencing to WebCallbacks, and reference to
// CallbackPromiseAdapter directly.
using WebImageCaptureGrabFrameCallbacks = WebCallbacks<sk_sp<SkImage>, void>;

// This class grabs Video Frames from a given Media Stream Video Track, binding
// a method of an ephemeral SingleShotFrameHandler every time grabFrame() is
// called. This method receives an incoming media::VideoFrame on a background
// thread and converts it into the appropriate SkBitmap which is sent back to
// OnSkBitmap(). This class is single threaded throughout.
class ImageCaptureFrameGrabber final : public MediaStreamVideoSink {
 public:
  using SkImageDeliverCB = WTF::CrossThreadFunction<void(sk_sp<SkImage>)>;

  ImageCaptureFrameGrabber();
  ~ImageCaptureFrameGrabber() override;

  void GrabFrame(WebMediaStreamTrack* track,
                 std::unique_ptr<WebImageCaptureGrabFrameCallbacks> callbacks,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  // Internal class to receive, convert and forward one frame.
  class SingleShotFrameHandler;

  void OnSkImage(
      ScopedWebCallbacks<WebImageCaptureGrabFrameCallbacks> callbacks,
      sk_sp<SkImage> image);

  // Flag to indicate that there is a frame grabbing in progress.
  bool frame_grab_in_progress_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ImageCaptureFrameGrabber> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageCaptureFrameGrabber);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_
