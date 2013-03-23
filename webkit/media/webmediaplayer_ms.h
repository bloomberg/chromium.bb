// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBMEDIAPLAYER_MS_H_
#define WEBKIT_MEDIA_WEBMEDIAPLAYER_MS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/layers/video_frame_provider.h"
#include "googleurl/src/gurl.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"

namespace WebKit {
class WebFrame;
class WebMediaPlayerClient;
class WebVideoFrame;
}

namespace media {
class MediaLog;
}

namespace webkit {
class WebLayerImpl;
}

namespace webkit_media {

class MediaStreamAudioRenderer;
class MediaStreamClient;
class VideoFrameProvider;
class WebMediaPlayerDelegate;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// VideoFrameProvider
//   provides video frames for rendering.
//
// TODO(wjia): add AudioPlayer.
// AudioPlayer
//   plays audio streams.
//
// WebKit::WebMediaPlayerClient
//   WebKit client of this media player object.
class WebMediaPlayerMS
    : public WebKit::WebMediaPlayer,
#ifdef REMOVE_WEBVIDEOFRAME
      public cc::VideoFrameProvider,
#endif
      public base::SupportsWeakPtr<WebMediaPlayerMS> {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides VideoFrameProvider.
  WebMediaPlayerMS(WebKit::WebFrame* frame,
                   WebKit::WebMediaPlayerClient* client,
                   base::WeakPtr<WebMediaPlayerDelegate> delegate,
                   MediaStreamClient* media_stream_client,
                   media::MediaLog* media_log);
  virtual ~WebMediaPlayerMS();

  virtual void load(const WebKit::WebURL& url, CORSMode cors_mode) OVERRIDE;
  virtual void load(const WebKit::WebURL& url,
                    WebKit::WebMediaSource* media_source,
                    CORSMode cors_mode) OVERRIDE;
  virtual void cancelLoad() OVERRIDE;

  // Playback controls.
  virtual void play() OVERRIDE;
  virtual void pause() OVERRIDE;
  virtual bool supportsFullscreen() const OVERRIDE;
  virtual bool supportsSave() const OVERRIDE;
  virtual void seek(float seconds) OVERRIDE;
  virtual void setEndTime(float seconds) OVERRIDE;
  virtual void setRate(float rate) OVERRIDE;
  virtual void setVolume(float volume) OVERRIDE;
  virtual void setVisible(bool visible) OVERRIDE;
  virtual void setPreload(WebKit::WebMediaPlayer::Preload preload) OVERRIDE;
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

  // Internal states of loading and network.
  virtual WebKit::WebMediaPlayer::NetworkState networkState() const OVERRIDE;
  virtual WebKit::WebMediaPlayer::ReadyState readyState() const OVERRIDE;

  virtual bool didLoadingProgress() const OVERRIDE;
  virtual unsigned long long totalBytes() const OVERRIDE;

  virtual bool hasSingleSecurityOrigin() const OVERRIDE;
  virtual bool didPassCORSAccessCheck() const OVERRIDE;
  virtual WebKit::WebMediaPlayer::MovieLoadType movieLoadType() const OVERRIDE;

  virtual float mediaTimeForTimeValue(float timeValue) const OVERRIDE;

  virtual unsigned decodedFrameCount() const OVERRIDE;
  virtual unsigned droppedFrameCount() const OVERRIDE;
  virtual unsigned audioDecodedByteCount() const OVERRIDE;
  virtual unsigned videoDecodedByteCount() const OVERRIDE;

#ifndef REMOVE_WEBVIDEOFRAME
  virtual WebKit::WebVideoFrame* getCurrentFrame() OVERRIDE;
  virtual void putCurrentFrame(WebKit::WebVideoFrame* web_video_frame) OVERRIDE;
#else
  // VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;
#endif

 private:
  // The callback for VideoFrameProvider to signal a new frame is available.
  void OnFrameAvailable(const scoped_refptr<media::VideoFrame>& frame);
  // Need repaint due to state change.
  void RepaintInternal();

  // The callback for source to report error.
  void OnSourceError();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(WebKit::WebMediaPlayer::NetworkState state);
  void SetReadyState(WebKit::WebMediaPlayer::ReadyState state);

  // Getter method to |client_|.
  WebKit::WebMediaPlayerClient* GetClient();

  WebKit::WebFrame* frame_;

  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  WebKit::WebTimeRanges buffered_;

  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  WebKit::WebMediaPlayerClient* client_;

  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  MediaStreamClient* media_stream_client_;
  scoped_refptr<webkit_media::VideoFrameProvider> video_frame_provider_;
  bool paused_;

  // |current_frame_| is updated only on main thread. The object it holds
  // can be freed on the compositor thread if it is the last to hold a
  // reference but media::VideoFrame is a thread-safe ref-pointer.
  scoped_refptr<media::VideoFrame> current_frame_;
  // |current_frame_used_| is updated on both main and compositing thread.
  // It's used to track whether |current_frame_| was painted for detecting
  // when to increase |dropped_frame_count_|.
  bool current_frame_used_;
  base::Lock current_frame_lock_;
  bool pending_repaint_;

  scoped_ptr<webkit::WebLayerImpl> video_weblayer_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  bool received_first_frame_;
  bool sequence_started_;
  base::TimeDelta start_time_;
  unsigned total_frame_count_;
  unsigned dropped_frame_count_;
  media::SkCanvasVideoRenderer video_renderer_;

  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;

  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_MS_H_
