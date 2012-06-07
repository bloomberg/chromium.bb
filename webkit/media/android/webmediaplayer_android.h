// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#pragma once

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
class WebFrame;
}

namespace media {
class MediaPlayerBridge;
}

namespace webkit_media {

class StreamTextureFactory;
class StreamTextureProxy;
class WebMediaPlayerManagerAndroid;
class WebMediaPlayerProxyAndroid;

// This class serves as the android implementation of WebKit::WebMediaPlayer.
// It implements all the playback functions by forwarding calls to android
// media player, and reports player state changes to the webkit.
class WebMediaPlayerAndroid :
    public WebKit::WebMediaPlayer,
    public base::SupportsWeakPtr<WebMediaPlayerAndroid> {
 public:
  WebMediaPlayerAndroid(WebKit::WebFrame* frame,
                        WebKit::WebMediaPlayerClient* client,
                        WebKit::WebCookieJar* cookie_jar,
                        webkit_media::WebMediaPlayerManagerAndroid* manager,
                        webkit_media::StreamTextureFactory* factory);
  virtual ~WebMediaPlayerAndroid();

  // Set |incognito_mode_| to true if in incognito mode.
  static void InitIncognito(bool incognito_mode);

  // TODO(fischman): remove the single-param version once WebKit stops calling
  // it.
  virtual void load(const WebKit::WebURL& url);
  // Resource loading.
  virtual void load(const WebKit::WebURL& url, CORSMode cors_mode);
  virtual void cancelLoad();

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual void seek(float seconds);
  virtual bool supportsFullscreen() const;
  virtual bool supportsSave() const;
  virtual void setEndTime(float seconds);
  virtual void setRate(float rate);
  virtual void setVolume(float volume);
  virtual void setVisible(bool visible);
  virtual bool totalBytesKnown();
  virtual const WebKit::WebTimeRanges& buffered();
  virtual float maxTimeSeekable() const;

  // Methods for painting.
  virtual void setSize(const WebKit::WebSize& size);
  virtual void paint(WebKit::WebCanvas* canvas,
                     const WebKit::WebRect& rect,
                     uint8_t alpha);

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  // Dimensions of the video.
  virtual WebKit::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual float duration() const;
  virtual float currentTime() const;

  // Get rate of loading the resource.
  virtual int32 dataRate() const;

  virtual unsigned long long bytesLoaded() const;
  virtual bool didLoadingProgress() const;
  virtual unsigned long long totalBytes() const;

  // Internal states of loading and network.
  virtual WebKit::WebMediaPlayer::NetworkState networkState() const;
  virtual WebKit::WebMediaPlayer::ReadyState readyState() const;

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;
  virtual WebKit::WebMediaPlayer::MovieLoadType movieLoadType() const;

  virtual float mediaTimeForTimeValue(float timeValue) const;

  // Provide statistics.
  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  // Methods called from VideoLayerChromium. These methods are running on the
  // compositor thread.
  virtual WebKit::WebVideoFrame* getCurrentFrame();
  virtual void putCurrentFrame(WebKit::WebVideoFrame*);
  virtual void setStreamTextureClient(
      WebKit::WebStreamTextureClient* client);

  // Media player callback handlers.
  void OnMediaPrepared();
  void OnPlaybackComplete();
  void OnBufferingUpdate(int percentage);
  void OnSeekComplete();
  void OnMediaError(int error_type);
  void OnMediaInfo(int info_type);
  void OnVideoSizeChanged(int width, int height);

  // This function is called by WebMediaPlayerManagerAndroid to pause the video
  // and release |media_player_| and its surface texture when we switch tabs.
  // However, the actual GlTexture is not released to keep the video screenshot.
  void ReleaseMediaResources();

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

  // Methods for creation and deletion of stream texture.
  void CreateStreamTexture();
  void DestroyStreamTexture();

  // whether the current process is incognito mode
  static bool incognito_mode_;

  WebKit::WebFrame* frame_;

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

  // Whether loading has progressed since the last call to didLoadingProgress.
  mutable bool did_loading_progress_;

  // Pointer to the cookie jar to get the cookie for the media url.
  WebKit::WebCookieJar* cookie_jar_;

  // Manager for managing this media player.
  webkit_media::WebMediaPlayerManagerAndroid* manager_;

  // Player ID assigned by the media player manager.
  int player_id_;

  // Whether the user has clicked the play button while media player
  // is preparing.
  bool pending_play_event_;

  // Current player states.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  // GL texture ID allocated to the video.
  unsigned int texture_id_;

  // Stream texture ID allocated to the video.
  unsigned int stream_id_;

  // Whether |media_player_| needs to re-establish the surface texture peer.
  bool needs_establish_peer_;

  // Object for allocating stream textures.
  scoped_ptr<webkit_media::StreamTextureFactory> stream_texture_factory_;

  // Object for calling back the compositor thread to repaint the video when a
  // frame available. It should be initialized on the compositor thread.
  scoped_ptr<webkit_media::StreamTextureProxy> stream_texture_proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
