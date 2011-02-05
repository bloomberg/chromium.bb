// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Delegate calls from WebCore::MediaPlayerPrivate to Chrome's video player.
// It contains PipelineImpl which is the actual media player pipeline, it glues
// the media player pipeline, data source, audio renderer and renderer.
// PipelineImpl would creates multiple threads and access some public methods
// of this class, so we need to be extra careful about concurrent access of
// methods and members.
//
// WebMediaPlayerImpl works with multiple objects, the most important ones are:
//
// media::PipelineImpl
//   The media playback pipeline.
//
// WebVideoRenderer
//   Video renderer object.
//
// WebMediaPlayerImpl::Proxy
//   Proxies methods calls from the media pipeline to WebKit.
//
// WebKit::WebMediaPlayerClient
//   WebKit client of this media player object.
//
// The following diagram shows the relationship of these objects:
//   (note: ref-counted reference is marked by a "r".)
//
// WebMediaPlayerImpl ------> PipelineImpl
//    |            ^               | r
//    |            |               v
//    |            |        WebVideoRenderer
//    |            |          ^ r
//    |            |          |
//    |      r     |    r     |
//    .------>   Proxy  <-----.
//    |
//    |
//    v
// WebMediaPlayerClient
//
// Notice that Proxy and WebVideoRenderer are referencing each other. This
// interdependency has to be treated carefully.
//
// Other issues:
// During tear down of the whole browser or a tab, the DOM tree may not be
// destructed nicely, and there will be some dangling media threads trying to
// the main thread, so we need this class to listen to destruction event of the
// main thread and cleanup the media threads when the even is received. Also
// at destruction of this class we will need to unhook it from destruction event
// list of the main thread.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/filters.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerClient.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class GURL;

namespace WebKit {
class WebFrame;
}

namespace webkit_glue {

class MediaResourceLoaderBridgeFactory;
class WebDataSource;
class WebVideoRenderer;

class WebMediaPlayerImpl : public WebKit::WebMediaPlayer,
                           public MessageLoop::DestructionObserver {
 public:
  // A proxy class that dispatches method calls from the media pipeline to
  // WebKit. Since there are multiple threads in the media pipeline and there's
  // need for the media pipeline to call to WebKit, e.g. repaint requests,
  // initialization events, etc, we have this class to bridge all method calls
  // from the media pipeline on different threads and serialize these calls
  // on the render thread.
  // Because of the nature of this object that it works with different threads,
  // it is made ref-counted.
  class Proxy : public base::RefCountedThreadSafe<Proxy> {
   public:
    Proxy(MessageLoop* render_loop,
          WebMediaPlayerImpl* webmediaplayer);

    // Methods for Filter -> WebMediaPlayerImpl communication.
    void Repaint();
    void SetVideoRenderer(scoped_refptr<WebVideoRenderer> video_renderer);
    void SetDataSource(scoped_refptr<WebDataSource> data_source);

    // Methods for WebMediaPlayerImpl -> Filter communication.
    void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& dest_rect);
    void SetSize(const gfx::Rect& rect);
    void Detach();
    void GetCurrentFrame(scoped_refptr<media::VideoFrame>* frame_out);
    void PutCurrentFrame(scoped_refptr<media::VideoFrame> frame);
    bool HasSingleOrigin();
    void AbortDataSource();

    // Methods for PipelineImpl -> WebMediaPlayerImpl communication.
    void PipelineInitializationCallback();
    void PipelineSeekCallback();
    void PipelineEndedCallback();
    void PipelineErrorCallback();
    void NetworkEventCallback();

    // Returns the message loop used by the proxy.
    MessageLoop* message_loop() { return render_loop_; }

   private:
    friend class base::RefCountedThreadSafe<Proxy>;

    virtual ~Proxy();

    // Invoke |webmediaplayer_| to perform a repaint.
    void RepaintTask();

    // Notify |webmediaplayer_| that initialization has finished.
    void PipelineInitializationTask();

    // Notify |webmediaplayer_| that a seek has finished.
    void PipelineSeekTask();

    // Notify |webmediaplayer_| that the media has ended.
    void PipelineEndedTask();

    // Notify |webmediaplayer_| that a pipeline error has been set.
    void PipelineErrorTask();

    // Notify |webmediaplayer_| that there's a network event.
    void NetworkEventTask();

    // The render message loop where WebKit lives.
    MessageLoop* render_loop_;
    WebMediaPlayerImpl* webmediaplayer_;
    scoped_refptr<WebDataSource> data_source_;
    scoped_refptr<WebVideoRenderer> video_renderer_;

    base::Lock lock_;
    int outstanding_repaints_;
  };

  // Construct a WebMediaPlayerImpl with reference to the client, and media
  // filter collection. By providing the filter collection the implementor can
  // provide more specific media filters that does resource loading and
  // rendering. |collection| should contain filter factories for:
  // 1. Data source
  // 2. Audio renderer
  // 3. Video renderer (optional)
  //
  // There are some default filters provided by this method:
  // 1. FFmpeg demuxer
  // 2. FFmpeg audio decoder
  // 3. FFmpeg video decoder
  // 4. Video renderer
  // 5. Null audio renderer
  // The video renderer provided by this class is using the graphics context
  // provided by WebKit to perform renderering. The simple data source does
  // resource loading by loading the whole resource object into memory. Null
  // audio renderer is a fake audio device that plays silence. Provider of the
  // |collection| can override the default filters by adding extra filters to
  // |collection| before calling this method.
  //
  // Callers must call |Initialize()| before they can use the object.
  WebMediaPlayerImpl(WebKit::WebMediaPlayerClient* client,
                     media::FilterCollection* collection,
                     media::MessageLoopFactory* message_loop_factory);
  virtual ~WebMediaPlayerImpl();

  // Finalizes initialization of the object.
  bool Initialize(
      WebKit::WebFrame* frame,
      bool use_simple_data_source,
      scoped_refptr<WebVideoRenderer> web_video_renderer);

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
  virtual bool setAutoBuffer(bool autoBuffer);
  virtual bool totalBytesKnown();
  virtual const WebKit::WebTimeRanges& buffered();
  virtual float maxTimeSeekable() const;

  // Methods for painting.
  virtual void setSize(const WebKit::WebSize& size);

  virtual void paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);

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

  virtual WebKit::WebVideoFrame* getCurrentFrame();
  virtual void putCurrentFrame(WebKit::WebVideoFrame* web_video_frame);

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  virtual void WillDestroyCurrentMessageLoop();

  void Repaint();

  void OnPipelineInitialize();

  void OnPipelineSeek();

  void OnPipelineEnded();

  void OnPipelineError();

  void OnNetworkEvent();

 private:
  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(WebKit::WebMediaPlayer::NetworkState state);
  void SetReadyState(WebKit::WebMediaPlayer::ReadyState state);

  // Destroy resources held.
  void Destroy();

  // Callback executed after |pipeline_| stops which signals Destroy()
  // to continue.
  void PipelineStoppedCallback();

  // Getter method to |client_|.
  WebKit::WebMediaPlayerClient* GetClient();

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

  // The actual pipeline and the thread it runs on.
  scoped_refptr<media::Pipeline> pipeline_;

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

  WebKit::WebMediaPlayerClient* client_;

  scoped_refptr<Proxy> proxy_;

  // Used to block Destroy() until Pipeline::Stop() is completed.
  base::WaitableEvent pipeline_stopped_;

#if WEBKIT_USING_CG
  scoped_ptr<skia::PlatformCanvas> skia_canvas_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_
