// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class MessageLoopProxy;
}

namespace webkit_media {

class WebMediaPlayerAndroid;

// Acts as a thread proxy between media::MediaPlayerBridge and
// WebMediaPlayerAndroid so that callbacks are posted onto the render thread.
class WebMediaPlayerProxyAndroid
    : public base::RefCountedThreadSafe<WebMediaPlayerProxyAndroid> {
 public:
  WebMediaPlayerProxyAndroid(
      const scoped_refptr<base::MessageLoopProxy>& render_loop,
      base::WeakPtr<WebMediaPlayerAndroid> webmediaplayer);

  // Callbacks from media::MediaPlayerBridge to WebMediaPlayerAndroid.
  void MediaErrorCallback(int error_type);
  void MediaInfoCallback(int info_type);
  void VideoSizeChangedCallback(int width, int height);
  void BufferingUpdateCallback(int percent);
  void PlaybackCompleteCallback();
  void SeekCompleteCallback();
  void MediaPreparedCallback();

 private:
  friend class base::RefCountedThreadSafe<WebMediaPlayerProxyAndroid>;
  virtual ~WebMediaPlayerProxyAndroid();

  // Notify |webmediaplayer_| that an error has occured.
  void MediaErrorTask(int error_type);

  // Notify |webmediaplayer_| that some info has been received from
  // media::MediaPlayerBridge.
  void MediaInfoTask(int info_type);

  // Notify |webmediaplayer_| that the video size has changed.
  void VideoSizeChangedTask(int width, int height);

  // Notify |webmediaplayer_| that an update in buffering has occured.
  void BufferingUpdateTask(int percent);

  // Notify |webmediaplayer_| that playback has completed.
  void PlaybackCompleteTask();

  // Notify |webmediaplayer_| that seek has completed.
  void SeekCompleteTask();

  // Notify |webmediaplayer_| that media has been prepared successfully.
  void MediaPreparedTask();

  // The render message loop where WebKit lives.
  scoped_refptr<base::MessageLoopProxy> render_loop_;

  // The WebMediaPlayerAndroid object all the callbacks should be send to.
  base::WeakPtr<WebMediaPlayerAndroid> webmediaplayer_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerProxyAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
