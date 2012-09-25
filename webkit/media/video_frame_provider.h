// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_VIDEO_FRAME_PROVIDER_H_
#define WEBKIT_MEDIA_VIDEO_FRAME_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace media {
class VideoFrame;
}

namespace webkit_media {

// Define an interface to provide a sequence of video frames to clients.
// TODO(wjia): remove ref count.
class VideoFrameProvider
    : public base::RefCountedThreadSafe<VideoFrameProvider> {
 public:
  typedef base::Callback<void(const scoped_refptr<media::VideoFrame>&)>
      RepaintCB;

  // Start to provide video frames to the caller.
  virtual void Start() = 0;

  // Stop to provide video frames to the caller.
  virtual void Stop() = 0;

  // Resume to provide video frames to the caller after being paused.
  virtual void Play() = 0;

  // Put the provider in pause state and the caller will not receive video
  // frames, but the provider might still generate video frames which are
  // thrown away.
  virtual void Pause() = 0;

 protected:
  friend class base::RefCountedThreadSafe<VideoFrameProvider>;
  VideoFrameProvider();
  virtual ~VideoFrameProvider();

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFrameProvider);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_VIDEO_FRAME_PROVIDER_H_
