// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBMEDIAPLAYER_PROXY_H_
#define WEBKIT_MEDIA_WEBMEDIAPLAYER_PROXY_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/filters/chunk_demuxer_client.h"
#include "webkit/media/web_data_source.h"

class MessageLoop;
class SkCanvas;

namespace gfx {
class Rect;
}

namespace webkit_media {

class WebMediaPlayerImpl;
class WebVideoRenderer;

// Acts as a thread proxy between the various threads used for multimedia and
// the render thread that WebMediaPlayerImpl is running on.
class WebMediaPlayerProxy
    : public base::RefCountedThreadSafe<WebMediaPlayerProxy>,
      public media::ChunkDemuxerClient {
 public:
  WebMediaPlayerProxy(MessageLoop* render_loop,
                      WebMediaPlayerImpl* webmediaplayer);

  // Methods for Filter -> WebMediaPlayerImpl communication.
  void Repaint();
  void SetVideoRenderer(scoped_refptr<WebVideoRenderer> video_renderer);
  WebDataSourceBuildObserverHack GetBuildObserver();

  // Methods for WebMediaPlayerImpl -> Filter communication.
  void Paint(SkCanvas* canvas, const gfx::Rect& dest_rect);
  void SetSize(const gfx::Rect& rect);
  void Detach();
  void GetCurrentFrame(scoped_refptr<media::VideoFrame>* frame_out);
  void PutCurrentFrame(scoped_refptr<media::VideoFrame> frame);
  bool HasSingleOrigin();
  void AbortDataSources();

  // Methods for PipelineImpl -> WebMediaPlayerImpl communication.
  void PipelineInitializationCallback(media::PipelineStatus status);
  void PipelineSeekCallback(media::PipelineStatus status);
  void PipelineEndedCallback(media::PipelineStatus status);
  void PipelineErrorCallback(media::PipelineStatus error);
  void NetworkEventCallback(bool network_activity);

  // ChunkDemuxerClient implementation.
  virtual void DemuxerOpened(media::ChunkDemuxer* demuxer) OVERRIDE;
  virtual void DemuxerClosed() OVERRIDE;

  // Methods for Demuxer communication.
  void DemuxerFlush();
  bool DemuxerAppend(const uint8* data, size_t length);
  void DemuxerEndOfStream(media::PipelineStatus status);
  void DemuxerShutdown();

  void DemuxerOpenedTask(const scoped_refptr<media::ChunkDemuxer>& demuxer);
  void DemuxerClosedTask();

  // Returns the message loop used by the proxy.
  MessageLoop* message_loop() { return render_loop_; }

 private:
  friend class base::RefCountedThreadSafe<WebMediaPlayerProxy>;
  virtual ~WebMediaPlayerProxy();

  // Adds a data source to data_sources_.
  void AddDataSource(WebDataSource* data_source);

  // Invoke |webmediaplayer_| to perform a repaint.
  void RepaintTask();

  // Notify |webmediaplayer_| that initialization has finished.
  void PipelineInitializationTask(media::PipelineStatus status);

  // Notify |webmediaplayer_| that a seek has finished.
  void PipelineSeekTask(media::PipelineStatus status);

  // Notify |webmediaplayer_| that the media has ended.
  void PipelineEndedTask(media::PipelineStatus status);

  // Notify |webmediaplayer_| that a pipeline error has occurred during
  // playback.
  void PipelineErrorTask(media::PipelineStatus error);

  // Notify |webmediaplayer_| that there's a network event.
  void NetworkEventTask(bool network_activity);

  // The render message loop where WebKit lives.
  MessageLoop* render_loop_;
  WebMediaPlayerImpl* webmediaplayer_;

  base::Lock data_sources_lock_;
  typedef std::list<scoped_refptr<WebDataSource> > DataSourceList;
  DataSourceList data_sources_;

  scoped_refptr<WebVideoRenderer> video_renderer_;

  base::Lock lock_;
  int outstanding_repaints_;

  scoped_refptr<media::ChunkDemuxer> chunk_demuxer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerProxy);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_PROXY_H_
