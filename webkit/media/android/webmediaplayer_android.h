// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_

#include <jni.h>

#include "base/basictypes.h"
#include "base/message_loop_proxy.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"

namespace WebKit {
class WebCookieJar;
}

namespace media {
class MediaPlayerBridge;
}

namespace webkit_media {

class WebMediaPlayerProxyAndroid;

// This class serves as the android implementation of WebKit::WebMediaPlayer.
// It implements all the playback functions by forwarding calls to android
// media player, and reports player state changes to the webkit.
class WebMediaPlayerAndroid :
    public WebKit::WebMediaPlayer,
    public base::SupportsWeakPtr<WebMediaPlayerAndroid> {
 public:
  WebMediaPlayerAndroid(WebKit::WebMediaPlayerClient* client,
                        WebKit::WebCookieJar* cookie_jar);
  virtual ~WebMediaPlayerAndroid() OVERRIDE;

  // Set |incognito_mode_| to true if in incognito mode.
  static void InitIncognito(bool incognito_mode);

  // Resource loading.
  virtual void load(const WebKit::WebURL& url) OVERRIDE;
  virtual void cancelLoad() OVERRIDE;

  // Playback controls.
  virtual void play() OVERRIDE;
  virtual void pause() OVERRIDE;
  virtual void seek(float seconds) OVERRIDE;
  virtual bool supportsFullscreen() const OVERRIDE;
  virtual bool supportsSave() const OVERRIDE;
  virtual void setEndTime(float seconds) OVERRIDE;
  virtual void setRate(float rate) OVERRIDE;
  virtual void setVolume(float volume) OVERRIDE;
  virtual void setVisible(bool visible) OVERRIDE;
  virtual bool totalBytesKnown() OVERRIDE;
  virtual const WebKit::WebTimeRanges& buffered() OVERRIDE;
  virtual float maxTimeSeekable() const OVERRIDE;

  // Methods for painting.
  virtual void setSize(const WebKit::WebSize& size) OVERRIDE;
  virtual void paint(WebKit::WebCanvas* canvas,
                     const WebKit::WebRect& rect,
                     uint8_t alpha) OVERRIDE;

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const OVERRIDE;
  virtual bool hasAudio() const OVERRIDE;

  // Dimensions of the video.
  virtual WebKit::WebSize naturalSize() const OVERRIDE;

  // Getters of playback state.
  virtual bool paused() const OVERRIDE;
  virtual bool seeking() const OVERRIDE;
  virtual float duration() const OVERRIDE;
  virtual float currentTime() const OVERRIDE;

  // Get rate of loading the resource.
  virtual int32 dataRate() const OVERRIDE;

  virtual unsigned long long bytesLoaded() const OVERRIDE;
  virtual unsigned long long totalBytes() const OVERRIDE;

  // Internal states of loading and network.
  virtual WebKit::WebMediaPlayer::NetworkState networkState() const OVERRIDE;
  virtual WebKit::WebMediaPlayer::ReadyState readyState() const OVERRIDE;

  virtual bool hasSingleSecurityOrigin() const OVERRIDE;
  virtual WebKit::WebMediaPlayer::MovieLoadType movieLoadType() const OVERRIDE;

  virtual float mediaTimeForTimeValue(float timeValue) const OVERRIDE;

  // Provide statistics.
  virtual unsigned decodedFrameCount() const OVERRIDE;
  virtual unsigned droppedFrameCount() const OVERRIDE;
  virtual unsigned audioDecodedByteCount() const OVERRIDE;
  virtual unsigned videoDecodedByteCount() const OVERRIDE;

  // Methods called from VideoLayerChromium. These methods are running on the
  // compositor thread.
  virtual WebKit::WebVideoFrame* getCurrentFrame() OVERRIDE;
  virtual void putCurrentFrame(WebKit::WebVideoFrame*) OVERRIDE;

  // Media player callback handlers.
  void OnMediaPrepared();
  void OnPlaybackComplete();
  void OnBufferingUpdate(int percentage);
  void OnSeekComplete();
  void OnMediaError(int error_type);
  void OnMediaInfo(int info_type);
  void OnVideoSizeChanged(int width, int height);

  // Method to set the video surface for android media player.
  void SetVideoSurface(jobject j_surface);

 private:
  // Create a media player to load the |url_| and prepare for playback.
  // Because of limited decoding resources on mobile devices, idle media players
  // could get released. In that case, we call this function to get a new media
  // player when needed.
  void InitializeMediaPlayer();

  // Functions that implements media player control.
  void PlayInternal();
  void PauseInternal();
  void SeekInternal(float seconds);

  // Helper methods for posting task for setting states and update WebKit.
  void UpdateNetworkState(WebKit::WebMediaPlayer::NetworkState state);
  void UpdateReadyState(WebKit::WebMediaPlayer::ReadyState state);

  // whether the current process is incognito mode
  static bool incognito_mode_;

  WebKit::WebMediaPlayerClient* const client_;

  // Save the list of buffered time ranges.
  WebKit::WebTimeRanges buffered_;

  // Bridge to the android media player.
  scoped_ptr<media::MediaPlayerBridge> media_player_;

  // Size of the media element.
  WebKit::WebSize texture_size_;

  // Size of the video.
  WebKit::WebSize natural_size_;

  // The video frame object used for renderering by WebKit.
  scoped_ptr<WebKit::WebVideoFrame> video_frame_;

  // Proxy object that delegates method calls on Render Thread.
  // This object is created on the Render Thread and is only called in the
  // destructor.
  scoped_refptr<WebMediaPlayerProxyAndroid> proxy_;

  // If this is set to true, prepare of the media player is done.
  bool prepared_;

  // URL of the media file to be fetched.
  GURL url_;

  // Media duration.
  float duration_;

  // When switching tabs, we release the media player. This variable keeps
  // track of the current playback time so that a seek will be performed
  // next time the media player gets recreated.
  float pending_seek_;

  // Internal seek state.
  bool seeking_;

  // Whether playback has completed.
  float playback_completed_;

  // Fake it by self increasing on every OnBufferingUpdate event.
  int64 buffered_bytes_;

  // Pointer to the cookie jar to get the cookie for the media url.
  WebKit::WebCookieJar* cookie_jar_;

  // Whether the user has clicked the play button while media player
  // is preparing.
  bool pending_play_event_;

  // Current player states.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
