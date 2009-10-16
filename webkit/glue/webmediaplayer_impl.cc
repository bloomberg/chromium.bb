// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webmediaplayer_impl.h"

#include "base/command_line.h"
#include "googleurl/src/gurl.h"
#include "media/base/media_format.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/null_audio_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/media/video_renderer_impl.h"

using WebKit::WebCanvas;
using WebKit::WebRect;
using WebKit::WebSize;

namespace {

// Limits the maximum outstanding repaints posted on render thread.
// This number of 50 is a guess, it does not take too much memory on the task
// queue but gives up a pretty good latency on repaint.
const int kMaxOutstandingRepaints = 50;

// Limits the range of playback rate.
//
// TODO(kylep): Revisit these.
//
// Vista has substantially lower performance than XP or Windows7.  If you speed
// up a video too much, it can't keep up, and rendering stops updating except on
// the time bar. For really high speeds, audio becomes a bottleneck and we just
// use up the data we have, which may not achieve the speed requested, but will
// not crash the tab.
//
// A very slow speed, ie 0.00000001x, causes the machine to lock up. (It seems
// like a busy loop). It gets unresponsive, although its not completely dead.
//
// Also our timers are not very accurate (especially for ogg), which becomes
// evident at low speeds and on Vista. Since other speeds are risky and outside
// the norms, we think 1/16x to 16x is a safe and useful range for now.
const float kMinRate = 0.0625f;
const float kMaxRate = 16.0f;

}  // namespace

namespace webkit_glue {

/////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerImpl::Proxy implementation

WebMediaPlayerImpl::Proxy::Proxy(MessageLoop* render_loop,
                                 WebMediaPlayerImpl* webmediaplayer)
    : render_loop_(render_loop),
      webmediaplayer_(webmediaplayer),
      outstanding_repaints_(0) {
  DCHECK(render_loop_);
  DCHECK(webmediaplayer_);
}

WebMediaPlayerImpl::Proxy::~Proxy() {
  Detach();
}

void WebMediaPlayerImpl::Proxy::Repaint() {
  AutoLock auto_lock(lock_);
  if (outstanding_repaints_ < kMaxOutstandingRepaints) {
    ++outstanding_repaints_;

    render_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebMediaPlayerImpl::Proxy::RepaintTask));
  }
}

void WebMediaPlayerImpl::Proxy::SetVideoRenderer(
    VideoRendererImpl* video_renderer) {
  video_renderer_ = video_renderer;
}

void WebMediaPlayerImpl::Proxy::Paint(skia::PlatformCanvas* canvas,
                                      const gfx::Rect& dest_rect) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (video_renderer_) {
    video_renderer_->Paint(canvas, dest_rect);
  }
}

void WebMediaPlayerImpl::Proxy::SetSize(const gfx::Rect& rect) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (video_renderer_) {
    video_renderer_->SetRect(rect);
  }
}

void WebMediaPlayerImpl::Proxy::Detach() {
  DCHECK(MessageLoop::current() == render_loop_);
  webmediaplayer_ = NULL;
  video_renderer_ = NULL;
}

void WebMediaPlayerImpl::Proxy::PipelineInitializationCallback() {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &WebMediaPlayerImpl::Proxy::PipelineInitializationTask));
}

void WebMediaPlayerImpl::Proxy::PipelineSeekCallback() {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &WebMediaPlayerImpl::Proxy::PipelineSeekTask));
}

void WebMediaPlayerImpl::Proxy::PipelineEndedCallback() {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &WebMediaPlayerImpl::Proxy::PipelineEndedTask));
}

void WebMediaPlayerImpl::Proxy::PipelineErrorCallback() {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &WebMediaPlayerImpl::Proxy::PipelineErrorTask));
}

void WebMediaPlayerImpl::Proxy::NetworkEventCallback() {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &WebMediaPlayerImpl::Proxy::NetworkEventTask));
}

void WebMediaPlayerImpl::Proxy::RepaintTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  {
    AutoLock auto_lock(lock_);
    --outstanding_repaints_;
    DCHECK_GE(outstanding_repaints_, 0);
  }
  if (webmediaplayer_) {
    webmediaplayer_->Repaint();
  }
}

void WebMediaPlayerImpl::Proxy::PipelineInitializationTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineInitialize();
  }
}

void WebMediaPlayerImpl::Proxy::PipelineSeekTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineSeek();
  }
}

void WebMediaPlayerImpl::Proxy::PipelineEndedTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineEnded();
  }
}

void WebMediaPlayerImpl::Proxy::PipelineErrorTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineError();
  }
}

void WebMediaPlayerImpl::Proxy::NetworkEventTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnNetworkEvent();
  }
}

/////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerImpl implementation

WebMediaPlayerImpl::WebMediaPlayerImpl(WebKit::WebMediaPlayerClient* client,
                                       media::FilterFactoryCollection* factory)
    : network_state_(WebKit::WebMediaPlayer::Empty),
      ready_state_(WebKit::WebMediaPlayer::HaveNothing),
      main_loop_(NULL),
      filter_factory_(factory),
      pipeline_thread_("PipelineThread"),
      paused_(true),
      playback_rate_(0.0f),
      client_(client) {
  // Saves the current message loop.
  DCHECK(!main_loop_);
  main_loop_ = MessageLoop::current();

  // Create the pipeline and its thread.
  if (!pipeline_thread_.Start()) {
    NOTREACHED() << "Could not start PipelineThread";
    return;
  }

  pipeline_ = new media::PipelineImpl(pipeline_thread_.message_loop());

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);

  // Creates the proxy.
  proxy_ = new Proxy(main_loop_, this);

  // Set our pipeline callbacks.
  pipeline_->SetPipelineEndedCallback(NewCallback(proxy_.get(),
      &WebMediaPlayerImpl::Proxy::PipelineEndedCallback));
  pipeline_->SetPipelineErrorCallback(NewCallback(proxy_.get(),
      &WebMediaPlayerImpl::Proxy::PipelineErrorCallback));
  pipeline_->SetNetworkEventCallback(NewCallback(proxy_.get(),
      &WebMediaPlayerImpl::Proxy::NetworkEventCallback));

  // Add in the default filter factories.
  filter_factory_->AddFactory(media::FFmpegDemuxer::CreateFilterFactory());
  filter_factory_->AddFactory(media::FFmpegAudioDecoder::CreateFactory());
  filter_factory_->AddFactory(media::FFmpegVideoDecoder::CreateFactory());
  filter_factory_->AddFactory(media::NullAudioRenderer::CreateFilterFactory());
  filter_factory_->AddFactory(VideoRendererImpl::CreateFactory(proxy_));
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  Destroy();

  // Finally tell the |main_loop_| we don't want to be notified of destruction
  // event.
  if (main_loop_) {
    main_loop_->RemoveDestructionObserver(this);
  }
}

void WebMediaPlayerImpl::load(const WebKit::WebURL& url) {
  DCHECK(MessageLoop::current() == main_loop_);
  DCHECK(proxy_);

  // Initialize the pipeline.
  SetNetworkState(WebKit::WebMediaPlayer::Loading);
  SetReadyState(WebKit::WebMediaPlayer::HaveNothing);
  pipeline_->Start(
      filter_factory_.get(),
      url.spec(),
      NewCallback(proxy_.get(),
                  &WebMediaPlayerImpl::Proxy::PipelineInitializationCallback));
}

void WebMediaPlayerImpl::cancelLoad() {
  DCHECK(MessageLoop::current() == main_loop_);
}

void WebMediaPlayerImpl::play() {
  DCHECK(MessageLoop::current() == main_loop_);

  paused_ = false;
  pipeline_->SetPlaybackRate(playback_rate_);
}

void WebMediaPlayerImpl::pause() {
  DCHECK(MessageLoop::current() == main_loop_);

  paused_ = true;
  pipeline_->SetPlaybackRate(0.0f);
}

bool WebMediaPlayerImpl::supportsFullscreen() const {
  DCHECK(MessageLoop::current() == main_loop_);
  return true;
}

bool WebMediaPlayerImpl::supportsSave() const {
  DCHECK(MessageLoop::current() == main_loop_);
  return true;
}

void WebMediaPlayerImpl::seek(float seconds) {
  DCHECK(MessageLoop::current() == main_loop_);

  // WebKit fires a seek(0) at the very start, however pipeline already does a
  // seek(0) internally.  Avoid doing seek(0) the second time because this will
  // cause extra pre-rolling and will break servers without range request
  // support.
  //
  // We still have to notify WebKit that time has changed otherwise
  // HTMLMediaElement gets into an inconsistent state.
  if (pipeline_->GetCurrentTime().ToInternalValue() == 0 && seconds == 0) {
    GetClient()->timeChanged();
    return;
  }

  // Try to preserve as much accuracy as possible.
  float microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  SetReadyState(WebKit::WebMediaPlayer::HaveMetadata);
  pipeline_->Seek(
      base::TimeDelta::FromMicroseconds(static_cast<int64>(microseconds)),
      NewCallback(proxy_.get(),
                  &WebMediaPlayerImpl::Proxy::PipelineSeekCallback));
}

void WebMediaPlayerImpl::setEndTime(float seconds) {
  DCHECK(MessageLoop::current() == main_loop_);

  // TODO(hclam): add method call when it has been implemented.
  return;
}

void WebMediaPlayerImpl::setRate(float rate) {
  DCHECK(MessageLoop::current() == main_loop_);

  // TODO(kylep): Remove when support for negatives is added. Also, modify the
  // following checks so rewind uses reasonable values also.
  if (rate < 0.0f)
    return;

  // Limit rates to reasonable values by clamping.
  if (rate != 0.0f) {
    if (rate < kMinRate)
      rate = kMinRate;
    else if (rate > kMaxRate)
      rate = kMaxRate;
  }

  playback_rate_ = rate;
  if (!paused_) {
    pipeline_->SetPlaybackRate(rate);
  }
}

void WebMediaPlayerImpl::setVolume(float volume) {
  DCHECK(MessageLoop::current() == main_loop_);

  pipeline_->SetVolume(volume);
}

void WebMediaPlayerImpl::setVisible(bool visible) {
  DCHECK(MessageLoop::current() == main_loop_);

  // TODO(hclam): add appropriate method call when pipeline has it implemented.
  return;
}

bool WebMediaPlayerImpl::setAutoBuffer(bool autoBuffer) {
  DCHECK(MessageLoop::current() == main_loop_);

  return false;
}

bool WebMediaPlayerImpl::totalBytesKnown() {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->GetTotalBytes() != 0;
}

bool WebMediaPlayerImpl::hasVideo() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->IsRendered(media::mime_type::kMajorTypeVideo);
}

bool WebMediaPlayerImpl::hasAudio() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->IsRendered(media::mime_type::kMajorTypeAudio);
}

WebKit::WebSize WebMediaPlayerImpl::naturalSize() const {
  DCHECK(MessageLoop::current() == main_loop_);

  size_t width, height;
  pipeline_->GetVideoSize(&width, &height);
  return WebKit::WebSize(width, height);
}

bool WebMediaPlayerImpl::paused() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::seeking() const {
  DCHECK(MessageLoop::current() == main_loop_);

  if (ready_state_ == WebKit::WebMediaPlayer::HaveNothing)
    return false;

  return ready_state_ == WebKit::WebMediaPlayer::HaveMetadata;
}

float WebMediaPlayerImpl::duration() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_->GetDuration().InSecondsF());
}

float WebMediaPlayerImpl::currentTime() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_->GetCurrentTime().InSecondsF());
}

int WebMediaPlayerImpl::dataRate() const {
  DCHECK(MessageLoop::current() == main_loop_);

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

const WebKit::WebTimeRanges& WebMediaPlayerImpl::buffered() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return buffered_;
}

float WebMediaPlayerImpl::maxTimeSeekable() const {
  DCHECK(MessageLoop::current() == main_loop_);

  // If we are performing streaming, we report that we cannot seek at all.
  // We are using this flag to indicate if the data source supports seeking
  // or not. We should be able to seek even if we are performing streaming.
  // TODO(hclam): We need to update this when we have better caching.
  if (pipeline_->IsStreaming())
    return 0.0f;
  return static_cast<float>(pipeline_->GetDuration().InSecondsF());
}

unsigned long long WebMediaPlayerImpl::bytesLoaded() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->GetBufferedBytes();
}

unsigned long long WebMediaPlayerImpl::totalBytes() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->GetTotalBytes();
}

void WebMediaPlayerImpl::setSize(const WebSize& size) {
  DCHECK(MessageLoop::current() == main_loop_);
  DCHECK(proxy_);

  proxy_->SetSize(gfx::Rect(0, 0, size.width, size.height));
}

void WebMediaPlayerImpl::paint(WebCanvas* canvas,
                               const WebRect& rect) {
  DCHECK(MessageLoop::current() == main_loop_);
  DCHECK(proxy_);

#if WEBKIT_USING_SKIA
  proxy_->Paint(canvas, rect);
#elif WEBKIT_USING_CG
  // If there is no preexisting platform canvas, or if the size has
  // changed, recreate the canvas.  This is to avoid recreating the bitmap
  // buffer over and over for each frame of video.
  if (!skia_canvas_.get() ||
      skia_canvas_->getDevice()->width() != rect.width ||
      skia_canvas_->getDevice()->height() != rect.height) {
    skia_canvas_.reset(new skia::PlatformCanvas(rect.width, rect.height, true));
  }

  // Draw to our temporary skia canvas.
  gfx::Rect normalized_rect(rect.width, rect.height);
  proxy_->Paint(skia_canvas_.get(), normalized_rect);

  // The mac coordinate system is flipped vertical from the normal skia
  // coordinates.  During painting of the frame, flip the coordinates
  // system and, for simplicity, also translate the clip rectangle to
  // start at 0,0.
  CGContextSaveGState(canvas);
  CGContextTranslateCTM(canvas, rect.x, rect.height + rect.y);
  CGContextScaleCTM(canvas, 1.0, -1.0);

  // We need a local variable CGRect version for DrawToContext.
  CGRect normalized_cgrect =
      CGRectMake(normalized_rect.x(), normalized_rect.y(),
                 normalized_rect.width(), normalized_rect.height());

  // Copy the frame rendered to our temporary skia canvas onto the passed in
  // canvas.
  skia_canvas_->getTopPlatformDevice().DrawToContext(canvas, 0, 0,
                                                     &normalized_cgrect);

  CGContextRestoreGState(canvas);
#else
  NOTIMPLEMENTED() << "We only support rendering to skia or CG";
#endif
}

bool WebMediaPlayerImpl::hasSingleSecurityOrigin() const {
  // TODO(hclam): Implement this.
  return false;
}

WebKit::WebMediaPlayer::MovieLoadType
    WebMediaPlayerImpl::movieLoadType() const {
  DCHECK(MessageLoop::current() == main_loop_);

  // TODO(hclam): If the pipeline is performing streaming, we say that this is
  // a live stream. But instead it should be a StoredStream if we have proper
  // caching.
  if (pipeline_->IsStreaming())
    return WebKit::WebMediaPlayer::LiveStream;
  return WebKit::WebMediaPlayer::Unknown;
}

void WebMediaPlayerImpl::WillDestroyCurrentMessageLoop() {
  Destroy();
  main_loop_ = NULL;
}

void WebMediaPlayerImpl::Repaint() {
  DCHECK(MessageLoop::current() == main_loop_);
  GetClient()->repaint();
}

void WebMediaPlayerImpl::OnPipelineInitialize() {
  DCHECK(MessageLoop::current() == main_loop_);
  if (pipeline_->GetError() == media::PIPELINE_OK) {
    // Only keep one time range starting from 0.
    WebKit::WebTimeRanges new_buffered(static_cast<size_t>(1));
    new_buffered[0].start = 0.0f;
    new_buffered[0].end =
        static_cast<float>(pipeline_->GetDuration().InSecondsF());
    buffered_.swap(new_buffered);

    // Since we have initialized the pipeline, say we have everything.
    // TODO(hclam): change this to report the correct status.
    SetReadyState(WebKit::WebMediaPlayer::HaveMetadata);
    SetReadyState(WebKit::WebMediaPlayer::HaveEnoughData);
    if (pipeline_->IsLoaded()) {
      SetNetworkState(WebKit::WebMediaPlayer::Loaded);
    } else {
      SetNetworkState(WebKit::WebMediaPlayer::Loading);
    }
  } else {
    // TODO(hclam): should use pipeline_->GetError() to determine the state
    // properly and reports error using MediaError.
    // WebKit uses FormatError to indicate an error for bogus URL or bad file.
    // Since we are at the initialization stage we can safely treat every error
    // as format error. Should post a task to call to |webmediaplayer_|.
    SetNetworkState(WebKit::WebMediaPlayer::FormatError);
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnPipelineSeek() {
  DCHECK(MessageLoop::current() == main_loop_);
  if (pipeline_->GetError() == media::PIPELINE_OK) {
    SetReadyState(WebKit::WebMediaPlayer::HaveEnoughData);
    GetClient()->timeChanged();
  }
}

void WebMediaPlayerImpl::OnPipelineEnded() {
  DCHECK(MessageLoop::current() == main_loop_);
  if (pipeline_->GetError() == media::PIPELINE_OK) {
    GetClient()->timeChanged();
  }
}

void WebMediaPlayerImpl::OnPipelineError() {
  DCHECK(MessageLoop::current() == main_loop_);
  switch (pipeline_->GetError()) {
    case media::PIPELINE_OK:
    case media::PIPELINE_STOPPING:
      NOTREACHED() << "We shouldn't get called with these non-errors";
      break;

    case media::PIPELINE_ERROR_INITIALIZATION_FAILED:
    case media::PIPELINE_ERROR_REQUIRED_FILTER_MISSING:
    case media::PIPELINE_ERROR_COULD_NOT_RENDER:
    case media::PIPELINE_ERROR_URL_NOT_FOUND:
    case media::PIPELINE_ERROR_NETWORK:
    case media::PIPELINE_ERROR_READ:
    case media::DEMUXER_ERROR_COULD_NOT_OPEN:
    case media::DEMUXER_ERROR_COULD_NOT_PARSE:
    case media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case media::DEMUXER_ERROR_COULD_NOT_CREATE_THREAD:
      // Format error.
      SetNetworkState(WebMediaPlayer::FormatError);
      break;

    case media::PIPELINE_ERROR_DECODE:
    case media::PIPELINE_ERROR_ABORT:
    case media::PIPELINE_ERROR_OUT_OF_MEMORY:
    case media::PIPELINE_ERROR_AUDIO_HARDWARE:
      // Decode error.
      SetNetworkState(WebMediaPlayer::DecodeError);
      break;
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnNetworkEvent() {
  DCHECK(MessageLoop::current() == main_loop_);
  if (pipeline_->GetError() == media::PIPELINE_OK) {
    if (pipeline_->IsNetworkActive())
      SetNetworkState(WebKit::WebMediaPlayer::Loading);
    else
      SetNetworkState(WebKit::WebMediaPlayer::Idle);
  }
}

void WebMediaPlayerImpl::SetNetworkState(
    WebKit::WebMediaPlayer::NetworkState state) {
  DCHECK(MessageLoop::current() == main_loop_);
  // Always notify to ensure client has the latest value.
  network_state_ = state;
  GetClient()->networkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(
    WebKit::WebMediaPlayer::ReadyState state) {
  DCHECK(MessageLoop::current() == main_loop_);
  // Always notify to ensure client has the latest value.
  ready_state_ = state;
  GetClient()->readyStateChanged();
}

void WebMediaPlayerImpl::Destroy() {
  DCHECK(MessageLoop::current() == main_loop_);

  // Make sure to kill the pipeline so there's no more media threads running.
  // TODO(hclam): stopping the pipeline might block for a long time.
  pipeline_->Stop(NULL);
  pipeline_thread_.Stop();

  // And then detach the proxy, it may live on the render thread for a little
  // longer until all the tasks are finished.
  if (proxy_) {
    proxy_->Detach();
    proxy_ = NULL;
  }
}

WebKit::WebMediaPlayerClient* WebMediaPlayerImpl::GetClient() {
  DCHECK(MessageLoop::current() == main_loop_);
  DCHECK(client_);
  return client_;
}

}  // namespace webkit_glue
