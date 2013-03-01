// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Delegate calls from WebCore::MediaPlayerPrivate to Chrome's video player.
// It contains Pipeline which is the actual media player pipeline, it glues
// the media player pipeline, data source, audio renderer and renderer.
// Pipeline would creates multiple threads and access some public methods
// of this class, so we need to be extra careful about concurrent access of
// methods and members.
//
// Other issues:
// During tear down of the whole browser or a tab, the DOM tree may not be
// destructed nicely, and there will be some dangling media threads trying to
// the main thread, so we need this class to listen to destruction event of the
// main thread and cleanup the media threads when the even is received. Also
// at destruction of this class we will need to unhook it from destruction event
// list of the main thread.

#ifndef WEBKIT_MEDIA_WEBMEDIAPLAYER_IMPL_H_
#define WEBKIT_MEDIA_WEBMEDIAPLAYER_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "googleurl/src/gurl.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/base/pipeline.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProvider.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "webkit/media/crypto/key_systems.h"
#include "webkit/media/crypto/proxy_decryptor.h"

class RenderAudioSourceProvider;

namespace WebKit {
class WebFrame;
}

namespace base {
class MessageLoopProxy;
}

namespace media {
class ChunkDemuxer;
class MediaLog;
}

namespace webkit_media {

class BufferedDataSource;
class MediaStreamClient;
class WebAudioSourceProviderImpl;
class WebMediaPlayerDelegate;
class WebMediaPlayerParams;

class WebMediaPlayerImpl
    : public WebKit::WebMediaPlayer,
      public MessageLoop::DestructionObserver,
      public base::SupportsWeakPtr<WebMediaPlayerImpl> {
 public:
  // Constructs a WebMediaPlayer implementation using Chromium's media stack.
  //
  // |delegate| may be null.
  WebMediaPlayerImpl(
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client,
      base::WeakPtr<WebMediaPlayerDelegate> delegate,
      const WebMediaPlayerParams& params);
  virtual ~WebMediaPlayerImpl();

  virtual void load(const WebKit::WebURL& url, CORSMode cors_mode);
  virtual void cancelLoad();

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual bool supportsFullscreen() const;
  virtual bool supportsSave() const;
  virtual void seek(float seconds);
  virtual void setEndTime(float seconds);
  virtual void setRate(float rate);
  virtual void setVolume(float volume);
  virtual void setVisible(bool visible);
  virtual void setPreload(WebKit::WebMediaPlayer::Preload preload);
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

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  virtual WebKit::WebMediaPlayer::NetworkState networkState() const;
  virtual WebKit::WebMediaPlayer::ReadyState readyState() const;

  virtual bool didLoadingProgress() const;
  virtual unsigned long long totalBytes() const;

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;
  virtual WebKit::WebMediaPlayer::MovieLoadType movieLoadType() const;

  virtual float mediaTimeForTimeValue(float timeValue) const;

  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  virtual WebKit::WebVideoFrame* getCurrentFrame();
  virtual void putCurrentFrame(WebKit::WebVideoFrame* web_video_frame);

  virtual WebKit::WebAudioSourceProvider* audioSourceProvider();

  virtual AddIdStatus sourceAddId(
      const WebKit::WebString& id,
      const WebKit::WebString& type,
      const WebKit::WebVector<WebKit::WebString>& codecs);
  virtual bool sourceRemoveId(const WebKit::WebString& id);
  virtual WebKit::WebTimeRanges sourceBuffered(const WebKit::WebString& id);
  virtual bool sourceAppend(const WebKit::WebString& id,
                            const unsigned char* data,
                            unsigned length);
  virtual bool sourceAbort(const WebKit::WebString& id);
  virtual void sourceSetDuration(double new_duration);
  virtual void sourceEndOfStream(EndOfStreamStatus status);
  virtual bool sourceSetTimestampOffset(const WebKit::WebString& id,
                                        double offset);

  virtual MediaKeyException generateKeyRequest(
      const WebKit::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);

  virtual MediaKeyException addKey(const WebKit::WebString& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const WebKit::WebString& session_id);

  virtual MediaKeyException cancelKeyRequest(
      const WebKit::WebString& key_system,
      const WebKit::WebString& session_id);

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  void Repaint();

  void OnPipelineSeek(media::PipelineStatus status);
  void OnPipelineEnded();
  void OnPipelineError(media::PipelineStatus error);
  void OnPipelineBufferingState(
      media::Pipeline::BufferingState buffering_state);
  void OnDemuxerOpened();
  void OnKeyAdded(const std::string& key_system, const std::string& session_id);
  void OnKeyError(const std::string& key_system,
                  const std::string& session_id,
                  media::Decryptor::KeyError error_code,
                  int system_code);
  void OnKeyMessage(const std::string& key_system,
                    const std::string& session_id,
                    const std::string& message,
                    const std::string& default_url);
  void OnNeedKey(const std::string& key_system,
                 const std::string& type,
                 const std::string& session_id,
                 scoped_array<uint8> init_data,
                 int init_data_size);
  void SetOpaque(bool);

 private:
  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(const GURL& gurl, bool success);

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(WebKit::WebMediaPlayer::NetworkState state);
  void SetReadyState(WebKit::WebMediaPlayer::ReadyState state);

  // Destroy resources held.
  void Destroy();

  // Getter method to |client_|.
  WebKit::WebMediaPlayerClient* GetClient();

  // Lets V8 know that player uses extra resources not managed by V8.
  void IncrementExternallyAllocatedMemory();

  // Actually do the work for generateKeyRequest/addKey so they can easily
  // report results to UMA.
  MediaKeyException GenerateKeyRequestInternal(
      const WebKit::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);
  MediaKeyException AddKeyInternal(const WebKit::WebString& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const WebKit::WebString& session_id);
  MediaKeyException CancelKeyRequestInternal(
      const WebKit::WebString& key_system,
      const WebKit::WebString& session_id);

  // Gets the duration value reported by the pipeline.
  double GetPipelineDuration() const;

  // Notifies WebKit of the duration change.
  void OnDurationChange();

  // Called by VideoRendererBase on its internal thread with the new frame to be
  // painted.
  void FrameReady(const scoped_refptr<media::VideoFrame>& frame);

  WebKit::WebFrame* frame_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  // Keep a list of buffered time ranges.
  WebKit::WebTimeRanges buffered_;

  // Message loops for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::MessageLoopProxy> main_loop_;

  scoped_ptr<media::FilterCollection> filter_collection_;
  scoped_refptr<media::Pipeline> pipeline_;
  base::Thread media_thread_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  WebKit::WebString current_key_system_;

  // Playback state.
  //
  // TODO(scherkus): we have these because Pipeline favours the simplicity of a
  // single "playback rate" over worrying about paused/stopped etc...  It forces
  // all clients to manage the pause+playback rate externally, but is that
  // really a bad thing?
  //
  // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't want
  // to hang the render thread during pause(), we record the time at the same
  // time we pause and then return that value in currentTime().  Otherwise our
  // clock can creep forward a little bit while the asynchronous
  // SetPlaybackRate(0) is being executed.
  bool paused_;
  bool seeking_;
  float playback_rate_;
  base::TimeDelta paused_time_;

  // Seek gets pending if another seek is in progress. Only last pending seek
  // will have effect.
  bool pending_seek_;
  float pending_seek_seconds_;

  WebKit::WebMediaPlayerClient* client_;

  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  MediaStreamClient* media_stream_client_;

  scoped_refptr<media::MediaLog> media_log_;

  // Since accelerated compositing status is only known after the first layout,
  // we delay reporting it to UMA until that time.
  bool accelerated_compositing_reported_;

  bool incremented_externally_allocated_memory_;

  // Routes audio playback to either AudioRendererSink or WebAudio.
  scoped_refptr<WebAudioSourceProviderImpl> audio_source_provider_;

  bool is_local_source_;
  bool supports_save_;

  // The decryptor that manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> decryptor_;

  bool starting_;

  // These two are mutually exclusive:
  //   |data_source_| is used for regular resource loads.
  //   |chunk_demuxer_| is used for Media Source resource loads.
  scoped_refptr<BufferedDataSource> data_source_;
  scoped_refptr<media::ChunkDemuxer> chunk_demuxer_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // Video frame rendering members.
  //
  // |lock_| protects |current_frame_| since new frames arrive on the video
  // rendering thread, yet are accessed for rendering on either the main thread
  // or compositing thread depending on whether accelerated compositing is used.
  base::Lock lock_;
  media::SkCanvasVideoRenderer skcanvas_video_renderer_;
  scoped_refptr<media::VideoFrame> current_frame_;
  bool pending_repaint_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_IMPL_H_
