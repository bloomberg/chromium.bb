// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_IMPL_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_IMPL_ANDROID_H_

#include <string>

#include <jni.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"
#include "webkit/media/android/webmediaplayer_android.h"

namespace WebKit {
class WebFrame;
}

namespace webkit_media {

class StreamTextureFactory;
class WebMediaPlayerManagerAndroid;
class WebMediaPlayerProxyAndroid;

// This class implements WebKit::WebMediaPlayer by keeping the android
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class WebMediaPlayerImplAndroid : public WebMediaPlayerAndroid {
 public:
  // Construct a WebMediaPlayerImplAndroid object. This class communicates
  // with the MediaPlayerBridge object in the browser process through
  // |proxy|.
  // TODO(qinmin): |frame| argument is used to determine whether the current
  // player can enter fullscreen.
  WebMediaPlayerImplAndroid(WebKit::WebFrame* frame,
                            WebKit::WebMediaPlayerClient* client,
                            WebMediaPlayerManagerAndroid* manager,
                            WebMediaPlayerProxyAndroid* proxy,
                            StreamTextureFactory* factory);
  virtual ~WebMediaPlayerImplAndroid();

  // WebKit::WebMediaPlayer implementation.
  virtual void enterFullscreen();
  virtual void exitFullscreen();
  virtual bool canEnterFullscreen() const;

  // Functions called when media player status changes.
  void OnMediaPlayerPlay();
  void OnMediaPlayerPause();
  void OnDidEnterFullscreen();
  void OnDidExitFullscreen();

  // WebMediaPlayerAndroid implementation.
  virtual void OnTimeUpdate(base::TimeDelta current_time) OVERRIDE;
  virtual void SetVideoSurface(jobject j_surface) OVERRIDE;

 private:
  // Methods inherited from WebMediaPlayerAndroid.
  virtual void InitializeMediaPlayer(GURL url) OVERRIDE;
  virtual void PlayInternal() OVERRIDE;
  virtual void PauseInternal() OVERRIDE;
  virtual void SeekInternal(base::TimeDelta time) OVERRIDE;
  virtual float GetCurrentTimeInternal() const OVERRIDE;
  virtual void ReleaseResourcesInternal() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void RequestExternalSurface() OVERRIDE;

  WebKit::WebFrame* const frame_;

  // Proxy object that delegates method calls on Render Thread.
  // This object is created on the Render Thread and is only called in the
  // destructor.
  WebMediaPlayerProxyAndroid* proxy_;

  // The current playing time. Because the mediaplayer is in the browser
  // process, it will regularly update the |current_time_| by calling
  // OnTimeUpdate().
  float current_time_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_IMPL_ANDROID_H_
