// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/video_renderer_base.h"
#include "webkit/media/webmediaplayer_impl.h"

using media::NetworkEvent;
using media::PipelineStatus;

namespace webkit_media {

// Limits the maximum outstanding repaints posted on render thread.
// This number of 50 is a guess, it does not take too much memory on the task
// queue but gives up a pretty good latency on repaint.
static const int kMaxOutstandingRepaints = 50;

WebMediaPlayerProxy::WebMediaPlayerProxy(
    const scoped_refptr<base::MessageLoopProxy>& render_loop,
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

void WebMediaPlayerProxy::SetOpaque(bool opaque) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::SetOpaqueTask, this, opaque));
}

void WebMediaPlayerProxy::Paint(SkCanvas* canvas,
                                const gfx::Rect& dest_rect,
                                uint8_t alpha) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (frame_provider_) {
    scoped_refptr<media::VideoFrame> video_frame;
    frame_provider_->GetCurrentFrame(&video_frame);
    video_renderer_.Paint(video_frame, canvas, dest_rect, alpha);
    frame_provider_->PutCurrentFrame(video_frame);
  }
}

bool WebMediaPlayerProxy::HasSingleOrigin() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (data_source_)
    return data_source_->HasSingleOrigin();
  return true;
}

void WebMediaPlayerProxy::AbortDataSource() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (data_source_)
    data_source_->Abort();
}

void WebMediaPlayerProxy::Detach() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  webmediaplayer_ = NULL;
  data_source_ = NULL;
  frame_provider_ = NULL;
  video_decoder_ = NULL;
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

void WebMediaPlayerProxy::RepaintTask() {
  DCHECK(render_loop_->BelongsToCurrentThread());
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
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineInitialize(status);
}

void WebMediaPlayerProxy::PipelineSeekTask(PipelineStatus status) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineSeek(status);
}

void WebMediaPlayerProxy::PipelineEndedTask(PipelineStatus status) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineEnded(status);
}

void WebMediaPlayerProxy::PipelineErrorTask(PipelineStatus error) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnPipelineError(error);
}

void WebMediaPlayerProxy::NetworkEventTask(NetworkEvent type) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnNetworkEvent(type);
}

void WebMediaPlayerProxy::SetOpaqueTask(bool opaque) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->SetOpaque(opaque);
}

void WebMediaPlayerProxy::GetCurrentFrame(
    scoped_refptr<media::VideoFrame>* frame_out) {
  if (frame_provider_)
    frame_provider_->GetCurrentFrame(frame_out);
}

void WebMediaPlayerProxy::PutCurrentFrame(
    scoped_refptr<media::VideoFrame> frame) {
  if (frame_provider_)
    frame_provider_->PutCurrentFrame(frame);
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

void WebMediaPlayerProxy::KeyNeeded(scoped_array<uint8> init_data,
                                    int init_data_size) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &WebMediaPlayerProxy::KeyNeededTask, this,
      base::Passed(&init_data), init_data_size));
}

void WebMediaPlayerProxy::DemuxerFlush() {
  if (chunk_demuxer_.get())
    chunk_demuxer_->FlushData();
}

media::ChunkDemuxer::Status WebMediaPlayerProxy::DemuxerAddId(
    const std::string& id,
    const std::string& type,
    std::vector<std::string>& codecs) {
  return chunk_demuxer_->AddId(id, type, codecs);
}

void WebMediaPlayerProxy::DemuxerRemoveId(const std::string& id) {
  chunk_demuxer_->RemoveId(id);
}

bool WebMediaPlayerProxy::DemuxerBufferedRange(
    const std::string& id,
    media::ChunkDemuxer::Ranges* ranges_out) {
  return chunk_demuxer_->GetBufferedRanges(id, ranges_out);
}

bool WebMediaPlayerProxy::DemuxerAppend(const std::string& id,
                                        const uint8* data,
                                        size_t length) {
  return chunk_demuxer_->AppendData(id, data, length);
}

void WebMediaPlayerProxy::DemuxerAbort(const std::string& id) {
  chunk_demuxer_->Abort(id);
}

void WebMediaPlayerProxy::DemuxerEndOfStream(media::PipelineStatus status) {
  chunk_demuxer_->EndOfStream(status);
}

void WebMediaPlayerProxy::DemuxerShutdown() {
  if (chunk_demuxer_.get())
    chunk_demuxer_->Shutdown();
}

void WebMediaPlayerProxy::DemuxerOpenedTask(
    const scoped_refptr<media::ChunkDemuxer>& demuxer) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  chunk_demuxer_ = demuxer;
  if (webmediaplayer_)
    webmediaplayer_->OnDemuxerOpened();
}

void WebMediaPlayerProxy::DemuxerClosedTask() {
  chunk_demuxer_ = NULL;
}

void WebMediaPlayerProxy::KeyNeededTask(scoped_array<uint8> init_data,
                                        int init_data_size) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnKeyNeeded(init_data.Pass(), init_data_size);
}

}  // namespace webkit_media
