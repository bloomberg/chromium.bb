// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_IN_PROCESS_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_IN_PROCESS_ANDROID_H_

#include <string>

#include <jni.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/android/cookie_getter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"
#include "webkit/media/android/webmediaplayer_android.h"

namespace WebKit {
class WebCookieJar;
class WebFrame;
}

namespace media {
class MediaPlayerBridge;
class MediaPlayerBridgeManager;
}

namespace webkit_media {

class StreamTextureFactory;
class WebMediaPlayerManagerAndroid;

// Class for retrieving the cookies from WebCookieJar.
class InProcessCookieGetter : public media::CookieGetter {
 public:
  // Construct an InProcessCookieGetter object from a WebCookieJar.
  explicit InProcessCookieGetter(WebKit::WebCookieJar* cookie_jar);
  virtual ~InProcessCookieGetter();

  // media::CookieGetter implementation.
  virtual void GetCookies(const std::string& url,
                          const std::string& first_party_for_cookies,
                          const GetCookieCB& callback) OVERRIDE;

private:
  WebKit::WebCookieJar* cookie_jar_;
  DISALLOW_COPY_AND_ASSIGN(InProcessCookieGetter);
};

// This class implements WebKit::WebMediaPlayer by keeping the android
// mediaplayer in the render process. This mode is being deprecated
// as mediaplayer is going to be moved to the browser process.
class WebMediaPlayerInProcessAndroid : public WebMediaPlayerAndroid {
 public:
  // Construct a WebMediaPlayerInProcessAndroid object.
  WebMediaPlayerInProcessAndroid(
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client,
      WebKit::WebCookieJar* cookie_jar,
      WebMediaPlayerManagerAndroid* manager,
      media::MediaPlayerBridgeManager* resource_manager,
      StreamTextureFactory* factory,
      bool disable_media_history_logging);
  virtual ~WebMediaPlayerInProcessAndroid();

  // Getters of playback state.
  virtual bool paused() const;

  // Callbacks from media::MediaPlayerBridge to WebMediaPlayerInProcessAndroid.
  void MediaErrorCallback(int player_id, int error_type);
  void VideoSizeChangedCallback(int player_id, int width, int height);
  void BufferingUpdateCallback(int player_id, int percent);
  void PlaybackCompleteCallback(int player_id);
  void SeekCompleteCallback(int player_id, base::TimeDelta current_time);
  void MediaPreparedCallback(int player_id, base::TimeDelta duration);
  void TimeUpdateCallback(int player_id, base::TimeDelta current_time) {}
  void MediaInterruptedCallback(int player_id);

  // WebMediaPlayerAndroid implementation.
  virtual void SetVideoSurface(jobject j_surface) OVERRIDE;
  virtual void OnTimeUpdate(base::TimeDelta current_time) OVERRIDE;

 private:
  // Methods inherited from WebMediaPlayerAndroid.
  virtual void InitializeMediaPlayer(GURL url) OVERRIDE;
  virtual void PlayInternal() OVERRIDE;
  virtual void PauseInternal() OVERRIDE;
  virtual void SeekInternal(base::TimeDelta time) OVERRIDE;
  virtual float GetCurrentTimeInternal() const OVERRIDE;
  virtual void ReleaseResourcesInternal() OVERRIDE;
  virtual void Destroy() OVERRIDE;

  WebKit::WebFrame* const frame_;

  // Bridge to the android media player.
  scoped_ptr<media::MediaPlayerBridge> media_player_;

  // Whether playback has completed.
  float playback_completed_;

  // Pointer to the cookie jar to get the cookie for the media url.
  WebKit::WebCookieJar* cookie_jar_;

  // Manager for managing all the hardware player resources.
  media::MediaPlayerBridgeManager* resource_manager_;

  // Whether we should disable history logging.
  bool disable_history_logging_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerInProcessAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_IN_PROCESS_ANDROID_H_
