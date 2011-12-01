// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#include "webkit/media/video_renderer_impl.h"
#include "webkit/media/webmediaplayer_impl.h"

using media::NetworkEvent;
using media::PipelineStatus;

namespace webkit_media {

// Limits the maximum outstanding repaints posted on render thread.
// This number of 50 is a guess, it does not take too much memory on the task
// queue but gives up a pretty good latency on repaint.
static const int kMaxOutstandingRepaints = 50;

WebMediaPlayerProxy::WebMediaPlayerProxy(MessageLoop* render_loop,
                                         WebMediaPlayerImpl* webmediaplayer)
    : render_loop_(render_loop),
      webmediaplayer_(webmediaplayer),
      outstanding_repaints_(0) {
  DCHECK(render_loop_);
  DCHECK(webmediaplayer_);
}

WebMediaPlayerProxy::~WebMediaPlayerProxy() {
  Detach();
}

void WebMediaPlayerProxy::Repaint() {
  base::AutoLock auto_lock(lock_);
  if (outstanding_repaints_ < kMaxOutstandingRepaints) {
    ++outstanding_repaints_;

    render_loop_->PostTask(FROM_HERE, base::Bind(
        &WebMediaPlayerProxy::RepaintTask, this));
  }
}

void WebMediaPlayerProxy::SetVideoRenderer(
    const scoped_refptr<VideoRendererImpl>& video_renderer) {
  video_renderer_ = video_renderer;
}

WebDataSourceBuildObserverHack WebMediaPlayerProxy::GetBuildObserver() {
  return base::Bind(&WebMediaPlayerProxy::AddDataSource, this);
}

void WebMediaPlayerProxy::Paint(SkCanvas* canvas, const gfx::Rect& dest_rect) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (video_renderer_) {
    video_renderer_->Paint(canvas, dest_rect);
  }
}

bool WebMediaPlayerProxy::HasSingleOrigin() {
  DCHECK(MessageLoop::current() == render_loop_);

  base::AutoLock auto_lock(data_sources_lock_);

  for (DataSourceList::iterator itr = data_sources_.begin();
       itr != data_sources_.end();
       itr++) {
    if (!(*itr)->HasSingleOrigin())
      return false;
  }
  return true;
}

void WebMediaPlayerProxy::AbortDataSources() {
  DCHECK(MessageLoop::current() == render_loop_);
  base::AutoLock auto_lock(data_sources_lock_);

  for (DataSourceList::iterator itr = data_sources_.begin();
       itr != data_sources_.end();
       itr++) {
    (*itr)->Abort();
  }
}

void WebMediaPlayerProxy::Detach() {
  DCHECK(MessageLoop::current() == render_loop_);
  webmediaplayer_ = NULL;
  video_renderer_ = NULL;

  {
    base::AutoLock auto_lock(data_sources_lock_);
    data_sources_.clear();
  }
}

void WebMediaPlayerProxy::PipelineInitializationCallback(
    PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::PipelineInitializationTask, this, status));
}

void WebMediaPlayerProxy::PipelineSeekCallback(PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::PipelineSeekTask, this, status));
}

void WebMediaPlayerProxy::PipelineEndedCallback(PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::PipelineEndedTask, this, status));
}

void WebMediaPlayerProxy::PipelineErrorCallback(PipelineStatus error) {
  DCHECK_NE(error, media::PIPELINE_OK);
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::PipelineErrorTask, this, error));
}

void WebMediaPlayerProxy::NetworkEventCallback(NetworkEvent type) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::NetworkEventTask, this, type));
}

void WebMediaPlayerProxy::AddDataSource(WebDataSource* data_source) {
  base::AutoLock auto_lock(data_sources_lock_);
  data_sources_.push_back(make_scoped_refptr(data_source));
}

void WebMediaPlayerProxy::RepaintTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  {
    base::AutoLock auto_lock(lock_);
    --outstanding_repaints_;
    DCHECK_GE(outstanding_repaints_, 0);
  }
  if (webmediaplayer_) {
    webmediaplayer_->Repaint();
  }
}

void WebMediaPlayerProxy::PipelineInitializationTask(PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineInitialize(status);
}

void WebMediaPlayerProxy::PipelineSeekTask(PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineSeek(status);
}

void WebMediaPlayerProxy::PipelineEndedTask(PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineEnded(status);
}

void WebMediaPlayerProxy::PipelineErrorTask(PipelineStatus error) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineError(error);
}

void WebMediaPlayerProxy::NetworkEventTask(NetworkEvent type) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_)
    webmediaplayer_->OnNetworkEvent(type);
}

void WebMediaPlayerProxy::GetCurrentFrame(
    scoped_refptr<media::VideoFrame>* frame_out) {
  if (video_renderer_)
    video_renderer_->GetCurrentFrame(frame_out);
}

void WebMediaPlayerProxy::PutCurrentFrame(
    scoped_refptr<media::VideoFrame> frame) {
  if (video_renderer_)
    video_renderer_->PutCurrentFrame(frame);
}

void WebMediaPlayerProxy::DemuxerOpened(media::ChunkDemuxer* demuxer) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::DemuxerOpenedTask, this,
      scoped_refptr<media::ChunkDemuxer>(demuxer)));
}

void WebMediaPlayerProxy::DemuxerClosed() {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::DemuxerClosedTask, this));
}

void WebMediaPlayerProxy::DemuxerFlush() {
  if (chunk_demuxer_.get())
    chunk_demuxer_->FlushData();
}

bool WebMediaPlayerProxy::DemuxerAppend(const uint8* data, size_t length) {
  if (chunk_demuxer_.get())
    return chunk_demuxer_->AppendData(data, length);
  return false;
}

void WebMediaPlayerProxy::DemuxerEndOfStream(media::PipelineStatus status) {
  if (chunk_demuxer_.get())
    chunk_demuxer_->EndOfStream(status);
}

void WebMediaPlayerProxy::DemuxerShutdown() {
  if (chunk_demuxer_.get())
    chunk_demuxer_->Shutdown();
}

void WebMediaPlayerProxy::DemuxerOpenedTask(
    const scoped_refptr<media::ChunkDemuxer>& demuxer) {
  DCHECK(MessageLoop::current() == render_loop_);
  chunk_demuxer_ = demuxer;
  if (webmediaplayer_)
    webmediaplayer_->OnDemuxerOpened();
}

void WebMediaPlayerProxy::DemuxerClosedTask() {
  chunk_demuxer_ = NULL;
}

}  // namespace webkit_media
