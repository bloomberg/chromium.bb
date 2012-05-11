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
// WebMediaPlayerImpl works with multiple objects, the most important ones are:
//
// media::Pipeline
//   The media playback pipeline.
//
// VideoRendererBase
//   Video renderer object.
//
// WebKit::WebMediaPlayerClient
//   WebKit client of this media player object.
//
// The following diagram shows the relationship of these objects:
//   (note: ref-counted reference is marked by a "r".)
//
// WebMediaPlayerClient (WebKit object)
//    ^
//    |
// WebMediaPlayerImpl ---> Pipeline
//    |        ^                  |
//    |        |                  v r
//    |        |        VideoRendererBase
//    |        |          |       ^ r
//    |   r    |          v r     |
//    '---> WebMediaPlayerProxy --'
//
// Notice that WebMediaPlayerProxy and VideoRendererBase are referencing each
// other. This interdependency has to be treated carefully.
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

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/filters.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProvider.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"

class RenderAudioSourceProvider;

namespace WebKit {
class WebAudioSourceProvider;
class WebFrame;
}

namespace media {
class MediaLog;
}

namespace webkit_media {

class MediaStreamClient;
class WebMediaPlayerDelegate;
class WebMediaPlayerProxy;

class WebMediaPlayerImpl
    : public WebKit::WebMediaPlayer,
      public MessageLoop::DestructionObserver,
      public base::SupportsWeakPtr<WebMediaPlayerImpl> {
 public:
  // Construct a WebMediaPlayerImpl with reference to the client, and media
  // filter collection. By providing the filter collection the implementor can
  // provide more specific media filters that does resource loading and
  // rendering.
  //
  // WebMediaPlayerImpl comes packaged with the following media filters:
  //   - URL fetching
  //   - Demuxing
  //   - Software audio/video decoding
  //   - Video rendering
  //
  // Clients are expected to add their platform-specific audio rendering media
  // filter if they wish to hear any sound coming out the speakers, otherwise
  // audio data is discarded and media plays back based on wall clock time.
  //
  WebMediaPlayerImpl(WebKit::WebFrame* frame,
                     WebKit::WebMediaPlayerClient* client,
                     base::WeakPtr<WebMediaPlayerDelegate> delegate,
                     media::FilterCollection* collection,
                     WebKit::WebAudioSourceProvider* audio_source_provider,
                     media::MessageLoopFactory* message_loop_factory,
                     MediaStreamClient* media_stream_client,
                     media::MediaLog* media_log);
  virtual ~WebMediaPlayerImpl();

  virtual void load(const WebKit::WebURL& url);
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

  // This variant (without alpha) is just present during staging of this API
  // change. Later we will again only have one virtual paint().
  virtual void paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);
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

  virtual unsigned long long bytesLoaded() const;
  virtual unsigned long long totalBytes() const;

  virtual bool hasSingleSecurityOrigin() const;
  virtual WebKit::WebMediaPlayer::MovieLoadType movieLoadType() const;

  virtual float mediaTimeForTimeValue(float timeValue) const;

  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  virtual WebKit::WebVideoFrame* getCurrentFrame();
  virtual void putCurrentFrame(WebKit::WebVideoFrame* web_video_frame);

  virtual WebKit::WebAudioSourceProvider* audioSourceProvider();

  // TODO(acolwell): Remove once new sourceAddId() signature is checked into
  // WebKit.
  virtual AddIdStatus sourceAddId(const WebKit::WebString& id,
                                  const WebKit::WebString& type);
  virtual AddIdStatus sourceAddId(
      const WebKit::WebString& id,
      const WebKit::WebString& type,
      const WebKit::WebVector<WebKit::WebString>& codecs);
  virtual bool sourceRemoveId(const WebKit::WebString& id);
  virtual WebKit::WebTimeRanges sourceBuffered(const WebKit::WebString& id);
  // TODO(acolwell): Remove non-id version when http://webk.it/83788 fix lands.
  virtual bool sourceAppend(const unsigned char* data, unsigned length);
  virtual bool sourceAppend(const WebKit::WebString& id,
                            const unsigned char* data,
                            unsigned length);
  virtual bool sourceAbort(const WebKit::WebString& id);
  virtual void sourceEndOfStream(EndOfStreamStatus status);

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

  void OnPipelineInitialize(media::PipelineStatus status);
  void OnPipelineSeek(media::PipelineStatus status);
  void OnPipelineEnded(media::PipelineStatus status);
  void OnPipelineError(media::PipelineStatus error);
  void OnNetworkEvent(media::NetworkEvent type);
  void OnDemuxerOpened();
  void OnKeyNeeded(scoped_array<uint8> init_data, int init_data_size);
  void SetOpaque(bool);

 private:
  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(const GURL& gurl, media::PipelineStatus status);

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

  WebKit::WebFrame* frame_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  // Keep a list of buffered time ranges.
  WebKit::WebTimeRanges buffered_;

  // Message loops for posting tasks between Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  MessageLoop* main_loop_;

  // A collection of filters.
  scoped_ptr<media::FilterCollection> filter_collection_;

  // The media pipeline and a bool tracking whether we have started it yet.
  //
  // TODO(scherkus): replace |started_| with a pointer check for |pipeline_| and
  // have WebMediaPlayerImpl return the default values to WebKit instead of
  // relying on Pipeline to take care of default values.
  scoped_refptr<media::Pipeline> pipeline_;
  bool started_;

  scoped_ptr<media::MessageLoopFactory> message_loop_factory_;

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

  scoped_refptr<WebMediaPlayerProxy> proxy_;

  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  MediaStreamClient* media_stream_client_;

  scoped_refptr<media::MediaLog> media_log_;

  // Since accelerated compositing status is only known after the first layout,
  // we delay reporting it to UMA until that time.
  bool accelerated_compositing_reported_;

  bool incremented_externally_allocated_memory_;

  WebKit::WebAudioSourceProvider* audio_source_provider_;

  bool is_local_source_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_IMPL_H_
