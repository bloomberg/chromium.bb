// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webmediaplayer_impl.h"

#include <limits>

#include "base/callback.h"
#include "base/command_line.h"
#include "media/base/composite_data_source_factory.h"
#include "media/base/filter_collection.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_impl.h"
#include "media/base/video_frame.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/null_audio_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/media/web_video_renderer.h"
#include "webkit/glue/webvideoframe_impl.h"

using WebKit::WebCanvas;
using WebKit::WebRect;
using WebKit::WebSize;
using media::PipelineStatus;

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

// Platform independent method for converting and rounding floating point
// seconds to an int64 timestamp.
//
// Refer to https://bugs.webkit.org/show_bug.cgi?id=52697 for details.
base::TimeDelta ConvertSecondsToTimestamp(float seconds) {
  float microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  float integer = ceilf(microseconds);
  float difference = integer - microseconds;

  // Round down if difference is large enough.
  if ((microseconds > 0 && difference > 0.5f) ||
      (microseconds <= 0 && difference >= 0.5f)) {
    integer -= 1.0f;
  }

  // Now we can safely cast to int64 microseconds.
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(integer));
}

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
  base::AutoLock auto_lock(lock_);
  if (outstanding_repaints_ < kMaxOutstandingRepaints) {
    ++outstanding_repaints_;

    render_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebMediaPlayerImpl::Proxy::RepaintTask));
  }
}

void WebMediaPlayerImpl::Proxy::SetVideoRenderer(
    scoped_refptr<WebVideoRenderer> video_renderer) {
  video_renderer_ = video_renderer;
}

WebDataSourceBuildObserverHack* WebMediaPlayerImpl::Proxy::GetBuildObserver() {
  if (!build_observer_.get())
    build_observer_.reset(NewCallback(this, &Proxy::AddDataSource));
  return build_observer_.get();
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

bool WebMediaPlayerImpl::Proxy::HasSingleOrigin() {
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

void WebMediaPlayerImpl::Proxy::AbortDataSources() {
  DCHECK(MessageLoop::current() == render_loop_);
  base::AutoLock auto_lock(data_sources_lock_);

  for (DataSourceList::iterator itr = data_sources_.begin();
       itr != data_sources_.end();
       itr++) {
    (*itr)->Abort();
  }
}

void WebMediaPlayerImpl::Proxy::Detach() {
  DCHECK(MessageLoop::current() == render_loop_);
  webmediaplayer_ = NULL;
  video_renderer_ = NULL;

  {
    base::AutoLock auto_lock(data_sources_lock_);
    data_sources_.clear();
  }
}

void WebMediaPlayerImpl::Proxy::PipelineInitializationCallback(
    PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &WebMediaPlayerImpl::Proxy::PipelineInitializationTask, status));
}

void WebMediaPlayerImpl::Proxy::PipelineSeekCallback(PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &WebMediaPlayerImpl::Proxy::PipelineSeekTask, status));
}

void WebMediaPlayerImpl::Proxy::PipelineEndedCallback(PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &WebMediaPlayerImpl::Proxy::PipelineEndedTask, status));
}

void WebMediaPlayerImpl::Proxy::PipelineErrorCallback(PipelineStatus error) {
  DCHECK_NE(error, media::PIPELINE_OK);
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &WebMediaPlayerImpl::Proxy::PipelineErrorTask, error));
}

void WebMediaPlayerImpl::Proxy::NetworkEventCallback(PipelineStatus status) {
  render_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &WebMediaPlayerImpl::Proxy::NetworkEventTask, status));
}

void WebMediaPlayerImpl::Proxy::AddDataSource(WebDataSource* data_source) {
  base::AutoLock auto_lock(data_sources_lock_);
  data_sources_.push_back(make_scoped_refptr(data_source));
}

void WebMediaPlayerImpl::Proxy::RepaintTask() {
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

void WebMediaPlayerImpl::Proxy::PipelineInitializationTask(
    PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineInitialize(status);
  }
}

void WebMediaPlayerImpl::Proxy::PipelineSeekTask(PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineSeek(status);
  }
}

void WebMediaPlayerImpl::Proxy::PipelineEndedTask(PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineEnded(status);
  }
}

void WebMediaPlayerImpl::Proxy::PipelineErrorTask(PipelineStatus error) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnPipelineError(error);
  }
}

void WebMediaPlayerImpl::Proxy::NetworkEventTask(PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (webmediaplayer_) {
    webmediaplayer_->OnNetworkEvent(status);
  }
}

void WebMediaPlayerImpl::Proxy::GetCurrentFrame(
    scoped_refptr<media::VideoFrame>* frame_out) {
  if (video_renderer_)
    video_renderer_->GetCurrentFrame(frame_out);
}

void WebMediaPlayerImpl::Proxy::PutCurrentFrame(
    scoped_refptr<media::VideoFrame> frame) {
  if (video_renderer_)
    video_renderer_->PutCurrentFrame(frame);
}

/////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerImpl implementation

WebMediaPlayerImpl::WebMediaPlayerImpl(
    WebKit::WebMediaPlayerClient* client,
    media::FilterCollection* collection,
    media::MessageLoopFactory* message_loop_factory)
    : network_state_(WebKit::WebMediaPlayer::Empty),
      ready_state_(WebKit::WebMediaPlayer::HaveNothing),
      main_loop_(NULL),
      filter_collection_(collection),
      pipeline_(NULL),
      message_loop_factory_(message_loop_factory),
      paused_(true),
      seeking_(false),
      playback_rate_(0.0f),
      client_(client),
      proxy_(NULL),
      pipeline_stopped_(false, false) {
  // Saves the current message loop.
  DCHECK(!main_loop_);
  main_loop_ = MessageLoop::current();
}

bool WebMediaPlayerImpl::Initialize(
    WebKit::WebFrame* frame,
    bool use_simple_data_source,
    scoped_refptr<WebVideoRenderer> web_video_renderer) {
  MessageLoop* pipeline_message_loop =
      message_loop_factory_->GetMessageLoop("PipelineThread");
  if (!pipeline_message_loop) {
    NOTREACHED() << "Could not start PipelineThread";
    return false;
  }

  pipeline_ = new media::PipelineImpl(pipeline_message_loop);

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);

  // Creates the proxy.
  proxy_ = new Proxy(main_loop_, this);
  web_video_renderer->SetWebMediaPlayerImplProxy(proxy_);
  proxy_->SetVideoRenderer(web_video_renderer);

  // Set our pipeline callbacks.
  pipeline_->Init(
      NewCallback(proxy_.get(),
                  &WebMediaPlayerImpl::Proxy::PipelineEndedCallback),
      NewCallback(proxy_.get(),
                  &WebMediaPlayerImpl::Proxy::PipelineErrorCallback),
      NewCallback(proxy_.get(),
                  &WebMediaPlayerImpl::Proxy::NetworkEventCallback));

  // A simple data source that keeps all data in memory.
  scoped_ptr<media::DataSourceFactory> simple_data_source_factory(
      SimpleDataSource::CreateFactory(MessageLoop::current(), frame,
                                      proxy_->GetBuildObserver()));

  // A sophisticated data source that does memory caching.
  scoped_ptr<media::DataSourceFactory> buffered_data_source_factory(
      BufferedDataSource::CreateFactory(MessageLoop::current(), frame,
                                        proxy_->GetBuildObserver()));

  scoped_ptr<media::CompositeDataSourceFactory> data_source_factory(
      new media::CompositeDataSourceFactory());

  if (use_simple_data_source) {
    data_source_factory->AddFactory(simple_data_source_factory.release());
    data_source_factory->AddFactory(buffered_data_source_factory.release());
  } else {
    data_source_factory->AddFactory(buffered_data_source_factory.release());
    data_source_factory->AddFactory(simple_data_source_factory.release());
  }

  scoped_ptr<media::DemuxerFactory> demuxer_factory(
      new media::FFmpegDemuxerFactory(data_source_factory.release(),
                                      pipeline_message_loop));
  filter_collection_->SetDemuxerFactory(demuxer_factory.release());

  // Add in the default filter factories.
  filter_collection_->AddAudioDecoder(new media::FFmpegAudioDecoder(
      message_loop_factory_->GetMessageLoop("AudioDecoderThread")));
  filter_collection_->AddVideoDecoder(new media::FFmpegVideoDecoder(
      message_loop_factory_->GetMessageLoop("VideoDecoderThread"), NULL));
  filter_collection_->AddAudioRenderer(new media::NullAudioRenderer());

  return true;
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

  // Handle any volume changes that occured before load().
  setVolume(GetClient()->volume());

  // Initialize the pipeline.
  SetNetworkState(WebKit::WebMediaPlayer::Loading);
  SetReadyState(WebKit::WebMediaPlayer::HaveNothing);
  pipeline_->Start(
      filter_collection_.release(),
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
  paused_time_ = pipeline_->GetCurrentTime();
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

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  // Update our paused time.
  if (paused_) {
    paused_time_ = seek_time;
  }

  seeking_ = true;

  // Kick off the asynchronous seek!
  pipeline_->Seek(
      seek_time,
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

  return pipeline_->HasVideo();
}

bool WebMediaPlayerImpl::hasAudio() const {
  DCHECK(MessageLoop::current() == main_loop_);

  return pipeline_->HasAudio();
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

  return seeking_;
}

float WebMediaPlayerImpl::duration() const {
  DCHECK(MessageLoop::current() == main_loop_);

  base::TimeDelta duration = pipeline_->GetMediaDuration();
  if (duration.InMicroseconds() == media::Limits::kMaxTimeInMicroseconds)
    return std::numeric_limits<float>::infinity();
  return static_cast<float>(duration.InSecondsF());
}

float WebMediaPlayerImpl::currentTime() const {
  DCHECK(MessageLoop::current() == main_loop_);

  if (paused_) {
    return static_cast<float>(paused_time_.InSecondsF());
  }
  return static_cast<float>(pipeline_->GetCurrentTime().InSecondsF());
}

int WebMediaPlayerImpl::dataRate() const {
  DCHECK(MessageLoop::current() == main_loop_);

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

WebKit::WebMediaPlayer::NetworkState WebMediaPlayerImpl::networkState() const {
  return network_state_;
}

WebKit::WebMediaPlayer::ReadyState WebMediaPlayerImpl::readyState() const {
  return ready_state_;
}

const WebKit::WebTimeRanges& WebMediaPlayerImpl::buffered() {
  DCHECK(MessageLoop::current() == main_loop_);

  // Update buffered_ with the most recent buffered time.
  if (buffered_.size() > 0) {
    float buffered_time = static_cast<float>(
        pipeline_->GetBufferedTime().InSecondsF());
    if (buffered_time >= buffered_[0].start)
      buffered_[0].end = buffered_time;
  }

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
  return static_cast<float>(pipeline_->GetMediaDuration().InSecondsF());
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
  // Get the current scaling in X and Y.
  CGAffineTransform mat = CGContextGetCTM(canvas);
  float scale_x = sqrt(mat.a * mat.a + mat.b * mat.b);
  float scale_y = sqrt(mat.c * mat.c + mat.d * mat.d);
  float inverse_scale_x = SkScalarNearlyZero(scale_x) ? 0.0f : 1.0f / scale_x;
  float inverse_scale_y = SkScalarNearlyZero(scale_y) ? 0.0f : 1.0f / scale_y;
  int scaled_width = static_cast<int>(rect.width * fabs(scale_x));
  int scaled_height = static_cast<int>(rect.height * fabs(scale_y));

  // Make sure we don't create a huge canvas.
  // TODO(hclam): Respect the aspect ratio.
  if (scaled_width > static_cast<int>(media::Limits::kMaxCanvas))
    scaled_width = media::Limits::kMaxCanvas;
  if (scaled_height > static_cast<int>(media::Limits::kMaxCanvas))
    scaled_height = media::Limits::kMaxCanvas;

  // If there is no preexisting platform canvas, or if the size has
  // changed, recreate the canvas.  This is to avoid recreating the bitmap
  // buffer over and over for each frame of video.
  if (!skia_canvas_.get() ||
      skia_canvas_->getDevice()->width() != scaled_width ||
      skia_canvas_->getDevice()->height() != scaled_height) {
    skia_canvas_.reset(
        new skia::PlatformCanvas(scaled_width, scaled_height, true));
  }

  // Draw to our temporary skia canvas.
  gfx::Rect normalized_rect(scaled_width, scaled_height);
  proxy_->Paint(skia_canvas_.get(), normalized_rect);

  // The mac coordinate system is flipped vertical from the normal skia
  // coordinates.  During painting of the frame, flip the coordinates
  // system and, for simplicity, also translate the clip rectangle to
  // start at 0,0.
  CGContextSaveGState(canvas);
  CGContextTranslateCTM(canvas, rect.x, rect.height + rect.y);
  CGContextScaleCTM(canvas, inverse_scale_x, -inverse_scale_y);

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
  if (proxy_)
    return proxy_->HasSingleOrigin();
  return true;
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

unsigned WebMediaPlayerImpl::decodedFrameCount() const
{
  DCHECK(MessageLoop::current() == main_loop_);

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::droppedFrameCount() const
{
  DCHECK(MessageLoop::current() == main_loop_);

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_dropped;
}

unsigned WebMediaPlayerImpl::audioDecodedByteCount() const
{
  DCHECK(MessageLoop::current() == main_loop_);

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.audio_bytes_decoded;
}

unsigned WebMediaPlayerImpl::videoDecodedByteCount() const
{
  DCHECK(MessageLoop::current() == main_loop_);

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_bytes_decoded;
}

WebKit::WebVideoFrame* WebMediaPlayerImpl::getCurrentFrame() {
  scoped_refptr<media::VideoFrame> video_frame;
  proxy_->GetCurrentFrame(&video_frame);
  if (video_frame.get())
    return new WebVideoFrameImpl(video_frame);
  return NULL;
}

void WebMediaPlayerImpl::putCurrentFrame(
    WebKit::WebVideoFrame* web_video_frame) {
  if (web_video_frame) {
    scoped_refptr<media::VideoFrame> video_frame(
        WebVideoFrameImpl::toVideoFrame(web_video_frame));
    proxy_->PutCurrentFrame(video_frame);
    delete web_video_frame;
  }
}

void WebMediaPlayerImpl::WillDestroyCurrentMessageLoop() {
  Destroy();
  main_loop_ = NULL;
}

void WebMediaPlayerImpl::Repaint() {
  DCHECK(MessageLoop::current() == main_loop_);
  GetClient()->repaint();
}

void WebMediaPlayerImpl::OnPipelineInitialize(PipelineStatus status) {
  DCHECK(MessageLoop::current() == main_loop_);
  if (status == media::PIPELINE_OK) {
    // Only keep one time range starting from 0.
    WebKit::WebTimeRanges new_buffered(static_cast<size_t>(1));
    new_buffered[0].start = 0.0f;
    new_buffered[0].end =
        static_cast<float>(pipeline_->GetMediaDuration().InSecondsF());
    buffered_.swap(new_buffered);

    // Since we have initialized the pipeline, say we have everything otherwise
    // we'll remain either loading/idle.
    // TODO(hclam): change this to report the correct status.
    SetReadyState(WebKit::WebMediaPlayer::HaveMetadata);
    SetReadyState(WebKit::WebMediaPlayer::HaveEnoughData);
    if (pipeline_->IsLoaded()) {
      SetNetworkState(WebKit::WebMediaPlayer::Loaded);
    }
  } else {
    // TODO(hclam): should use |status| to determine the state
    // properly and reports error using MediaError.
    // WebKit uses FormatError to indicate an error for bogus URL or bad file.
    // Since we are at the initialization stage we can safely treat every error
    // as format error. Should post a task to call to |webmediaplayer_|.
    SetNetworkState(WebKit::WebMediaPlayer::FormatError);
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnPipelineSeek(PipelineStatus status) {
  DCHECK(MessageLoop::current() == main_loop_);
  if (status == media::PIPELINE_OK) {
    // Update our paused time.
    if (paused_) {
      paused_time_ = pipeline_->GetCurrentTime();
    }

    SetReadyState(WebKit::WebMediaPlayer::HaveEnoughData);
    seeking_ = false;
    GetClient()->timeChanged();
  }
}

void WebMediaPlayerImpl::OnPipelineEnded(PipelineStatus status) {
  DCHECK(MessageLoop::current() == main_loop_);
  if (status == media::PIPELINE_OK) {
    GetClient()->timeChanged();
  }
}

void WebMediaPlayerImpl::OnPipelineError(PipelineStatus error) {
  DCHECK(MessageLoop::current() == main_loop_);
  switch (error) {
    case media::PIPELINE_OK:
      LOG(DFATAL) << "PIPELINE_OK isn't an error!";
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
    case media::DATASOURCE_ERROR_URL_NOT_SUPPORTED:
      // Format error.
      SetNetworkState(WebMediaPlayer::FormatError);
      break;

    case media::PIPELINE_ERROR_DECODE:
    case media::PIPELINE_ERROR_ABORT:
    case media::PIPELINE_ERROR_OUT_OF_MEMORY:
    case media::PIPELINE_ERROR_AUDIO_HARDWARE:
    case media::PIPELINE_ERROR_OPERATION_PENDING:
    case media::PIPELINE_ERROR_INVALID_STATE:
      // Decode error.
      SetNetworkState(WebMediaPlayer::DecodeError);
      break;
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnNetworkEvent(PipelineStatus status) {
  DCHECK(MessageLoop::current() == main_loop_);
  if (status == media::PIPELINE_OK) {
    if (pipeline_->IsNetworkActive()) {
      SetNetworkState(WebKit::WebMediaPlayer::Loading);
    } else {
      // If we are inactive because we just finished receiving all the data,
      // do one final repaint to show final progress.
      if (bytesLoaded() == totalBytes() &&
          network_state_ != WebKit::WebMediaPlayer::Idle) {
        Repaint();

        SetNetworkState(WebKit::WebMediaPlayer::Loaded);
      }

      SetNetworkState(WebKit::WebMediaPlayer::Idle);
    }
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

  // Tell the data source to abort any pending reads so that the pipeline is
  // not blocked when issuing stop commands to the other filters.
  if (proxy_)
    proxy_->AbortDataSources();

  // Make sure to kill the pipeline so there's no more media threads running.
  // Note: stopping the pipeline might block for a long time.
  if (pipeline_) {
    pipeline_->Stop(NewCallback(this,
        &WebMediaPlayerImpl::PipelineStoppedCallback));
    pipeline_stopped_.Wait();
  }

  message_loop_factory_.reset();

  // And then detach the proxy, it may live on the render thread for a little
  // longer until all the tasks are finished.
  if (proxy_) {
    proxy_->Detach();
    proxy_ = NULL;
  }
}

void WebMediaPlayerImpl::PipelineStoppedCallback(PipelineStatus status) {
  pipeline_stopped_.Signal();
}

WebKit::WebMediaPlayerClient* WebMediaPlayerImpl::GetClient() {
  DCHECK(MessageLoop::current() == main_loop_);
  DCHECK(client_);
  return client_;
}

}  // namespace webkit_glue
