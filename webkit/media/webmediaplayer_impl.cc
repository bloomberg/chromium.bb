// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_impl.h"

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/filter_collection.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/video_renderer_base.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVideoFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "v8/include/v8.h"
#include "webkit/media/buffered_data_source.h"
#include "webkit/media/filter_helpers.h"
#include "webkit/media/key_systems.h"
#include "webkit/media/webmediaplayer_delegate.h"
#include "webkit/media/webmediaplayer_proxy.h"
#include "webkit/media/webmediaplayer_util.h"
#include "webkit/media/webvideoframe_impl.h"

using WebKit::WebCanvas;
using WebKit::WebMediaPlayer;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using media::NetworkEvent;
using media::PipelineStatus;

namespace {

// Amount of extra memory used by each player instance reported to V8.
// It is not exact number -- first, it differs on different platforms,
// and second, it is very hard to calculate. Instead, use some arbitrary
// value that will cause garbage collection from time to time. We don't want
// it to happen on every allocation, but don't want 5k players to sit in memory
// either. Looks that chosen constant achieves both goals, at least for audio
// objects. (Do not worry about video objects yet, JS programs do not create
// thousands of them...)
const int kPlayerExtraMemory = 1024 * 1024;

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

namespace webkit_media {

WebMediaPlayerImpl::WebMediaPlayerImpl(
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    media::FilterCollection* collection,
    WebKit::WebAudioSourceProvider* audio_source_provider,
    media::MessageLoopFactory* message_loop_factory,
    MediaStreamClient* media_stream_client,
    media::MediaLog* media_log)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      main_loop_(MessageLoop::current()),
      filter_collection_(collection),
      started_(false),
      message_loop_factory_(message_loop_factory),
      paused_(true),
      seeking_(false),
      playback_rate_(0.0f),
      pending_seek_(false),
      pending_seek_seconds_(0.0f),
      client_(client),
      proxy_(new WebMediaPlayerProxy(main_loop_->message_loop_proxy(), this)),
      delegate_(delegate),
      media_stream_client_(media_stream_client),
      media_log_(media_log),
      accelerated_compositing_reported_(false),
      incremented_externally_allocated_memory_(false),
      audio_source_provider_(audio_source_provider) {
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  MessageLoop* pipeline_message_loop =
      message_loop_factory_->GetMessageLoop("PipelineThread");
  pipeline_ = new media::Pipeline(pipeline_message_loop, media_log_);

  // Let V8 know we started new thread if we did not did it yet.
  // Made separate task to avoid deletion of player currently being created.
  // Also, delaying GC until after player starts gets rid of starting lag --
  // collection happens in parallel with playing.
  //
  // TODO(enal): remove when we get rid of per-audio-stream thread.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerImpl::IncrementExternallyAllocatedMemory,
                 AsWeakPtr()));

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);

  // Create default video renderer.
  scoped_refptr<media::VideoRendererBase> video_renderer =
      new media::VideoRendererBase(
          base::Bind(&WebMediaPlayerProxy::Repaint, proxy_),
          base::Bind(&WebMediaPlayerProxy::SetOpaque, proxy_.get()),
          true);
  filter_collection_->AddVideoRenderer(video_renderer);
  proxy_->set_frame_provider(video_renderer);

  // Create default audio renderer.
  filter_collection_->AddAudioRenderer(
      new media::AudioRendererImpl(new media::NullAudioSink()));
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  Destroy();
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_)
    delegate_->PlayerGone(this);

  // Finally tell the |main_loop_| we don't want to be notified of destruction
  // event.
  if (main_loop_) {
    main_loop_->RemoveDestructionObserver(this);
  }
}

namespace {

// Helper enum for reporting scheme histograms.
enum URLSchemeForHistogram {
  kUnknownURLScheme,
  kMissingURLScheme,
  kHttpURLScheme,
  kHttpsURLScheme,
  kFtpURLScheme,
  kChromeExtensionURLScheme,
  kJavascriptURLScheme,
  kFileURLScheme,
  kBlobURLScheme,
  kDataURLScheme,
  kFileSystemScheme,
  kMaxURLScheme = kFileSystemScheme  // Must be equal to highest enum value.
};

URLSchemeForHistogram URLScheme(const GURL& url) {
  if (!url.has_scheme()) return kMissingURLScheme;
  if (url.SchemeIs("http")) return kHttpURLScheme;
  if (url.SchemeIs("https")) return kHttpsURLScheme;
  if (url.SchemeIs("ftp")) return kFtpURLScheme;
  if (url.SchemeIs("chrome-extension")) return kChromeExtensionURLScheme;
  if (url.SchemeIs("javascript")) return kJavascriptURLScheme;
  if (url.SchemeIs("file")) return kFileURLScheme;
  if (url.SchemeIs("blob")) return kBlobURLScheme;
  if (url.SchemeIs("data")) return kDataURLScheme;
  if (url.SchemeIs("filesystem")) return kFileSystemScheme;
  return kUnknownURLScheme;
}

}  // anonymous namespace

void WebMediaPlayerImpl::load(const WebKit::WebURL& url) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GURL gurl(url);
  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(gurl), kMaxURLScheme);

  // Handle any volume/preload changes that occured before load().
  setVolume(GetClient()->volume());
  setPreload(GetClient()->preload());

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  // Media streams pipelines can start immediately.
  if (BuildMediaStreamCollection(url, media_stream_client_,
                                 message_loop_factory_.get(),
                                 filter_collection_.get())) {
    StartPipeline();
    return;
  }

  // Media source pipelines can start immediately.
  scoped_refptr<media::FFmpegVideoDecoder> video_decoder;
  if (BuildMediaSourceCollection(url, GetClient()->sourceURL(), proxy_,
                                 message_loop_factory_.get(),
                                 filter_collection_.get(),
                                 &video_decoder)) {
    proxy_->set_video_decoder(video_decoder);
    StartPipeline();
    return;
  }

  // Otherwise it's a regular request which requires resolving the URL first.
  proxy_->set_data_source(
      new BufferedDataSource(main_loop_, frame_, media_log_));
  proxy_->data_source()->Initialize(url, base::Bind(
      &WebMediaPlayerImpl::DataSourceInitialized,
      base::Unretained(this), gurl));

  // TODO(scherkus): this is leftover from removing DemuxerFactory -- instead
  // our DataSource should report this information. See http://crbug.com/120426
  bool local_source = !gurl.SchemeIs("http") && !gurl.SchemeIs("https");

  BuildDefaultCollection(proxy_->data_source(),
                         local_source,
                         message_loop_factory_.get(),
                         filter_collection_.get(),
                         &video_decoder);
  proxy_->set_video_decoder(video_decoder);
}

void WebMediaPlayerImpl::cancelLoad() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
}

void WebMediaPlayerImpl::play() {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  paused_ = false;
  pipeline_->SetPlaybackRate(playback_rate_);

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));

  if (delegate_)
    delegate_->DidPlay(this);
}

void WebMediaPlayerImpl::pause() {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  paused_ = true;
  pipeline_->SetPlaybackRate(0.0f);
  paused_time_ = pipeline_->GetCurrentTime();

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));

  if (delegate_)
    delegate_->DidPause(this);
}

bool WebMediaPlayerImpl::supportsFullscreen() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return true;
}

bool WebMediaPlayerImpl::supportsSave() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return true;
}

void WebMediaPlayerImpl::seek(float seconds) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (seeking_) {
    pending_seek_ = true;
    pending_seek_seconds_ = seconds;
    return;
  }

  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  // Update our paused time.
  if (paused_)
    paused_time_ = seek_time;

  seeking_ = true;

  proxy_->DemuxerFlush();

  // Kick off the asynchronous seek!
  pipeline_->Seek(
      seek_time,
      base::Bind(&WebMediaPlayerProxy::PipelineSeekCallback,
                 proxy_.get()));
}

void WebMediaPlayerImpl::setEndTime(float seconds) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): add method call when it has been implemented.
  return;
}

void WebMediaPlayerImpl::setRate(float rate) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

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
  DCHECK_EQ(main_loop_, MessageLoop::current());

  pipeline_->SetVolume(volume);
}

void WebMediaPlayerImpl::setVisible(bool visible) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): add appropriate method call when pipeline has it implemented.
  return;
}

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, chromium_name) \
    COMPILE_ASSERT(static_cast<int>(WebMediaPlayer::webkit_name) == \
                   static_cast<int>(webkit_media::chromium_name), \
                   mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(PreloadNone, NONE);
COMPILE_ASSERT_MATCHING_ENUM(PreloadMetaData, METADATA);
COMPILE_ASSERT_MATCHING_ENUM(PreloadAuto, AUTO);

void WebMediaPlayerImpl::setPreload(WebMediaPlayer::Preload preload) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (proxy_ && proxy_->data_source()) {
    // XXX: Why do I need to use webkit_media:: prefix? clang complains!
    proxy_->data_source()->SetPreload(
        static_cast<webkit_media::Preload>(preload));
  }
}

bool WebMediaPlayerImpl::totalBytesKnown() {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetTotalBytes() != 0;
}

bool WebMediaPlayerImpl::hasVideo() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->HasVideo();
}

bool WebMediaPlayerImpl::hasAudio() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->HasAudio();
}

WebKit::WebSize WebMediaPlayerImpl::naturalSize() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  gfx::Size size;
  pipeline_->GetNaturalVideoSize(&size);
  return WebKit::WebSize(size);
}

bool WebMediaPlayerImpl::paused() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::seeking() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return false;

  return seeking_;
}

float WebMediaPlayerImpl::duration() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  base::TimeDelta duration = pipeline_->GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == media::kInfiniteDuration())
    return std::numeric_limits<float>::infinity();

  return static_cast<float>(duration.InSecondsF());
}

float WebMediaPlayerImpl::currentTime() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (paused_)
    return static_cast<float>(paused_time_.InSecondsF());
  return static_cast<float>(pipeline_->GetCurrentTime().InSecondsF());
}

int WebMediaPlayerImpl::dataRate() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::networkState() const {
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::readyState() const {
  return ready_state_;
}

const WebKit::WebTimeRanges& WebMediaPlayerImpl::buffered() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  media::Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_->GetBufferedTimeRanges();
  WebKit::WebTimeRanges web_ranges(buffered_time_ranges.size());
  for (size_t i = 0; i < buffered_time_ranges.size(); ++i) {
    web_ranges[i].start = buffered_time_ranges.start(i).InSecondsF();
    web_ranges[i].end = buffered_time_ranges.end(i).InSecondsF();
  }
  buffered_.swap(web_ranges);
  return buffered_;
}

float WebMediaPlayerImpl::maxTimeSeekable() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // If we are performing streaming, we report that we cannot seek at all.
  // We are using this flag to indicate if the data source supports seeking
  // or not. We should be able to seek even if we are performing streaming.
  // TODO(hclam): We need to update this when we have better caching.
  if (pipeline_->IsStreaming())
    return 0.0f;
  return static_cast<float>(pipeline_->GetMediaDuration().InSecondsF());
}

unsigned long long WebMediaPlayerImpl::bytesLoaded() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetBufferedBytes();
}

unsigned long long WebMediaPlayerImpl::totalBytes() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetTotalBytes();
}

void WebMediaPlayerImpl::setSize(const WebSize& size) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // Don't need to do anything as we use the dimensions passed in via paint().
}

// This variant (without alpha) is just present during staging of this API
// change. Later we will again only have one virtual paint().
void WebMediaPlayerImpl::paint(WebKit::WebCanvas* canvas,
                               const WebKit::WebRect& rect) {
  paint(canvas, rect, 0xFF);
}

void WebMediaPlayerImpl::paint(WebCanvas* canvas,
                               const WebRect& rect,
                               uint8_t alpha) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(proxy_);

#if WEBKIT_USING_SKIA
  proxy_->Paint(canvas, rect, alpha);
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
  if (scaled_width > static_cast<int>(media::limits::kMaxCanvas))
    scaled_width = media::limits::kMaxCanvas;
  if (scaled_height > static_cast<int>(media::limits::kMaxCanvas))
    scaled_height = media::limits::kMaxCanvas;

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
  skia::DrawToNativeContext(skia_canvas_.get(), canvas, 0, 0,
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

WebMediaPlayer::MovieLoadType WebMediaPlayerImpl::movieLoadType() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): If the pipeline is performing streaming, we say that this is
  // a live stream. But instead it should be a StoredStream if we have proper
  // caching.
  if (pipeline_->IsStreaming())
    return WebMediaPlayer::MovieLoadTypeLiveStream;
  return WebMediaPlayer::MovieLoadTypeUnknown;
}

float WebMediaPlayerImpl::mediaTimeForTimeValue(float timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::decodedFrameCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::droppedFrameCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_dropped;
}

unsigned WebMediaPlayerImpl::audioDecodedByteCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.audio_bytes_decoded;
}

unsigned WebMediaPlayerImpl::videoDecodedByteCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

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
  if (!accelerated_compositing_reported_) {
    accelerated_compositing_reported_ = true;
    UMA_HISTOGRAM_BOOLEAN("Media.AcceleratedCompositingActive",
                          frame_->view()->isAcceleratedCompositingActive());
  }
  if (web_video_frame) {
    scoped_refptr<media::VideoFrame> video_frame(
        WebVideoFrameImpl::toVideoFrame(web_video_frame));
    proxy_->PutCurrentFrame(video_frame);
    delete web_video_frame;
  } else {
    proxy_->PutCurrentFrame(NULL);
  }
}

#define COMPILE_ASSERT_MATCHING_STATUS_ENUM(webkit_name, chromium_name) \
    COMPILE_ASSERT(static_cast<int>(WebKit::WebMediaPlayer::webkit_name) == \
                   static_cast<int>(media::ChunkDemuxer::chromium_name), \
                   mismatching_status_enums)
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddIdStatusOk, kOk);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddIdStatusNotSupported, kNotSupported);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddIdStatusReachedIdLimit, kReachedIdLimit);

WebKit::WebMediaPlayer::AddIdStatus WebMediaPlayerImpl::sourceAddId(
    const WebKit::WebString& id,
    const WebKit::WebString& type) {
    DCHECK_EQ(main_loop_, MessageLoop::current());

    WebKit::WebString kDefaultSourceType("video/webm; codecs=\"vp8, vorbis\"");

    if (type != kDefaultSourceType)
      return WebKit::WebMediaPlayer::AddIdStatusNotSupported;

    WebKit::WebVector<WebKit::WebString> codecs(static_cast<size_t>(2));
    codecs[0] = "vp8";
    codecs[1] = "vorbis";
    return sourceAddId(id, "video/webm", codecs);
}

WebKit::WebMediaPlayer::AddIdStatus WebMediaPlayerImpl::sourceAddId(
    const WebKit::WebString& id,
    const WebKit::WebString& type,
    const WebKit::WebVector<WebKit::WebString>& codecs) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  std::vector<std::string> new_codecs(codecs.size());
  for (size_t i = 0; i < codecs.size(); ++i)
    new_codecs[i] = codecs[i].utf8().data();

  return static_cast<WebKit::WebMediaPlayer::AddIdStatus>(
      proxy_->DemuxerAddId(id.utf8().data(), type.utf8().data(),
                           new_codecs));
}

bool WebMediaPlayerImpl::sourceRemoveId(const WebKit::WebString& id) {
  DCHECK(!id.isEmpty());
  proxy_->DemuxerRemoveId(id.utf8().data());
  return true;
}

WebKit::WebTimeRanges WebMediaPlayerImpl::sourceBuffered(
    const WebKit::WebString& id) {
  media::ChunkDemuxer::Ranges buffered_ranges;
  if (!proxy_->DemuxerBufferedRange(id.utf8().data(), &buffered_ranges))
    return WebKit::WebTimeRanges();

  WebKit::WebTimeRanges ranges(buffered_ranges.size());
  for (size_t i = 0; i < buffered_ranges.size(); i++) {
    ranges[i].start = buffered_ranges[i].first.InSecondsF();
    ranges[i].end = buffered_ranges[i].second.InSecondsF();
  }
  return ranges;
}

bool WebMediaPlayerImpl::sourceAppend(const unsigned char* data,
                                      unsigned length) {
  return sourceAppend(WebKit::WebString::fromUTF8("DefaultSourceId"),
                      data, length);
}

bool WebMediaPlayerImpl::sourceAppend(const WebKit::WebString& id,
                                      const unsigned char* data,
                                      unsigned length) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return proxy_->DemuxerAppend(id.utf8().data(), data, length);
}

bool WebMediaPlayerImpl::sourceAbort(const WebKit::WebString& id) {
  proxy_->DemuxerAbort(id.utf8().data());
  return true;
}

void WebMediaPlayerImpl::sourceEndOfStream(
    WebMediaPlayer::EndOfStreamStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  media::PipelineStatus pipeline_status = media::PIPELINE_OK;

  switch(status) {
    case WebMediaPlayer::EndOfStreamStatusNoError:
      break;
    case WebMediaPlayer::EndOfStreamStatusNetworkError:
      pipeline_status = media::PIPELINE_ERROR_NETWORK;
      break;
    case WebMediaPlayer::EndOfStreamStatusDecodeError:
      pipeline_status = media::PIPELINE_ERROR_DECODE;
      break;
    default:
      NOTIMPLEMENTED();
  }

  proxy_->DemuxerEndOfStream(pipeline_status);
}

WebKit::WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::generateKeyRequest(const WebString& key_system,
                                       const unsigned char* init_data,
                                       unsigned init_data_length) {
  if (!IsSupportedKeySystem(key_system))
    return WebKit::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // Every request call creates a unique ID.
  // TODO(ddorwin): Move this to the CDM implementations since the CDMs may
  // create their own IDs and since CDMs supporting multiple renderer processes
  // need globally unique IDs.
  // Everything from here until the return should probably be handled by
  // the decryptor - see http://crbug.com/123260.
  static uint32_t next_available_session_id = 1;
  uint32_t session_id = next_available_session_id++;

  WebString session_id_string(base::UintToString16(session_id));

  DVLOG(1) << "generateKeyRequest: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length))
           << " [" << session_id_string.utf8().data() << "]";

  // TODO(ddorwin): Generate a key request in the decryptor and fire
  // keyMessage when it completes.
  // For now, just fire the event with the init_data as the request.
  const unsigned char* message = init_data;
  unsigned message_length = init_data_length;

  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &WebKit::WebMediaPlayerClient::keyMessage,
      base::Unretained(GetClient()),
      key_system,
      session_id_string,
      message,
      message_length));

  return WebKit::WebMediaPlayer::MediaKeyExceptionNoError;
}

WebKit::WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);

  if (!IsSupportedKeySystem(key_system))
    return WebKit::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;


  DVLOG(1) << "addKey: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length))
           << " [" << session_id.utf8().data() << "]";

  // TODO(ddorwin): Everything from here until the return should probably be
  // handled by the decryptor - see http://crbug.com/123260.
  // Temporarily, fire an error for invalid key length so we can test the error
  // event and fire the keyAdded event in all other cases.
  const unsigned kSupportedKeyLength = 16;  // 128-bit key.
  if (key_length != kSupportedKeyLength) {
    DLOG(ERROR) << "addKey: invalid key length: " << key_length;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &WebKit::WebMediaPlayerClient::keyError,
        base::Unretained(GetClient()),
        key_system,
        session_id,
        WebKit::WebMediaPlayerClient::MediaKeyErrorCodeUnknown,
        0));
  } else {
    // TODO(ddorwin): Fix the decryptor to accept no |init_data|. See
    // http://crbug.com/123265. Until then, ensure a non-empty value is passed.
    static const unsigned char kDummyInitData[1] = {0};
    if (!init_data) {
      init_data = kDummyInitData;
      init_data_length = arraysize(kDummyInitData);
    }

    proxy_->video_decoder()->decryptor()->AddKey(init_data, init_data_length,
                                                 key, key_length);

    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &WebKit::WebMediaPlayerClient::keyAdded,
        base::Unretained(GetClient()),
        key_system,
        session_id));
  }

  return WebKit::WebMediaPlayer::MediaKeyExceptionNoError;
}

WebKit::WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  if (!IsSupportedKeySystem(key_system))
    return WebKit::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // TODO(ddorwin): Cancel the key request in the decryptor.

  return WebKit::WebMediaPlayer::MediaKeyExceptionNoError;
}

void WebMediaPlayerImpl::WillDestroyCurrentMessageLoop() {
  Destroy();
  main_loop_ = NULL;
}

void WebMediaPlayerImpl::Repaint() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  GetClient()->repaint();
}

void WebMediaPlayerImpl::OnPipelineInitialize(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (status != media::PIPELINE_OK) {
    OnPipelineError(status);
    // Repaint to trigger UI update.
    Repaint();
    return;
  }

  if (!hasVideo())
    GetClient()->disableAcceleratedCompositing();

  if (pipeline_->IsLocalSource())
    SetNetworkState(WebMediaPlayer::NetworkStateLoaded);

  SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
  // Fire canplaythrough immediately after playback begins because of
  // crbug.com/106480.
  // TODO(vrk): set ready state to HaveFutureData when bug above is fixed.
  SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnPipelineSeek(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  seeking_ = false;
  if (pending_seek_) {
    pending_seek_ = false;
    seek(pending_seek_seconds_);
    return;
  }

  if (status != media::PIPELINE_OK) {
    OnPipelineError(status);
    return;
  }

  // Update our paused time.
  if (paused_)
    paused_time_ = pipeline_->GetCurrentTime();

  GetClient()->timeChanged();
}

void WebMediaPlayerImpl::OnPipelineEnded(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (status != media::PIPELINE_OK) {
    OnPipelineError(status);
    return;
  }
  GetClient()->timeChanged();
}

void WebMediaPlayerImpl::OnPipelineError(PipelineStatus error) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  switch (error) {
    case media::PIPELINE_OK:
      LOG(DFATAL) << "PIPELINE_OK isn't an error!";
      break;

    case media::PIPELINE_ERROR_NETWORK:
    case media::PIPELINE_ERROR_READ:
      SetNetworkState(WebMediaPlayer::NetworkStateNetworkError);
      break;

    case media::PIPELINE_ERROR_INITIALIZATION_FAILED:
    case media::PIPELINE_ERROR_REQUIRED_FILTER_MISSING:
    case media::PIPELINE_ERROR_COULD_NOT_RENDER:
    case media::PIPELINE_ERROR_URL_NOT_FOUND:
    case media::DEMUXER_ERROR_COULD_NOT_OPEN:
    case media::DEMUXER_ERROR_COULD_NOT_PARSE:
    case media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case media::DECODER_ERROR_NOT_SUPPORTED:
      SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;

    case media::PIPELINE_ERROR_DECODE:
    case media::PIPELINE_ERROR_ABORT:
    case media::PIPELINE_ERROR_OPERATION_PENDING:
    case media::PIPELINE_ERROR_INVALID_STATE:
      SetNetworkState(WebMediaPlayer::NetworkStateDecodeError);
      break;

    case media::PIPELINE_ERROR_DECRYPT:
      // Decrypt error.
      // TODO(xhwang): Change to use NetworkStateDecryptError once it's added in
      // Webkit (see http://crbug.com/124486).
      SetNetworkState(WebMediaPlayer::NetworkStateDecodeError);
      break;
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnNetworkEvent(NetworkEvent type) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  switch(type) {
    case media::DOWNLOAD_CONTINUED:
      SetNetworkState(WebMediaPlayer::NetworkStateLoading);
      break;
    case media::DOWNLOAD_PAUSED:
      SetNetworkState(WebMediaPlayer::NetworkStateIdle);
      break;
    case media::CAN_PLAY_THROUGH:
      // Temporarily disable delayed firing of CAN_PLAY_THROUGH due to
      // crbug.com/106480.
      // TODO(vrk): uncomment code below when bug above is fixed.
      // SetReadyState(WebMediaPlayer::NetworkStateHaveEnoughData);
      break;
    default:
      NOTREACHED();
  }
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->sourceOpened();
}

void WebMediaPlayerImpl::OnKeyNeeded(scoped_array<uint8> init_data,
                                     int init_data_size) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->keyNeeded("", "", init_data.get(), init_data_size);
}

void WebMediaPlayerImpl::SetOpaque(bool opaque) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->setOpaque(opaque);
}

void WebMediaPlayerImpl::DataSourceInitialized(
    const GURL& gurl,
    media::PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (status != media::PIPELINE_OK) {
    DVLOG(1) << "DataSourceInitialized status: " << status;
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
    Repaint();
    return;
  }

  StartPipeline();
}

void WebMediaPlayerImpl::StartPipeline() {
  started_ = true;
  pipeline_->Start(
      filter_collection_.Pass(),
      base::Bind(&WebMediaPlayerProxy::PipelineEndedCallback, proxy_.get()),
      base::Bind(&WebMediaPlayerProxy::PipelineErrorCallback, proxy_.get()),
      base::Bind(&WebMediaPlayerProxy::NetworkEventCallback, proxy_.get()),
      base::Bind(&WebMediaPlayerProxy::PipelineInitializationCallback,
                 proxy_.get()));
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DVLOG(1) << "SetNetworkState: " << state;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->networkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DVLOG(1) << "SetReadyState: " << state;
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->readyStateChanged();
}

void WebMediaPlayerImpl::Destroy() {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // Tell the data source to abort any pending reads so that the pipeline is
  // not blocked when issuing stop commands to the other filters.
  if (proxy_) {
    proxy_->AbortDataSource();
    proxy_->DemuxerShutdown();
  }

  // Make sure to kill the pipeline so there's no more media threads running.
  // Note: stopping the pipeline might block for a long time.
  if (started_) {
    base::WaitableEvent waiter(false, false);
    pipeline_->Stop(base::Bind(
        &base::WaitableEvent::Signal, base::Unretained(&waiter)));
    waiter.Wait();
    started_ = false;
  }

  // Let V8 know we are not using extra resources anymore.
  if (incremented_externally_allocated_memory_) {
    v8::V8::AdjustAmountOfExternalAllocatedMemory(-kPlayerExtraMemory);
    incremented_externally_allocated_memory_ = false;
  }

  message_loop_factory_.reset();

  // And then detach the proxy, it may live on the render thread for a little
  // longer until all the tasks are finished.
  if (proxy_) {
    proxy_->Detach();
    proxy_ = NULL;
  }
}

WebKit::WebMediaPlayerClient* WebMediaPlayerImpl::GetClient() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(client_);
  return client_;
}

WebKit::WebAudioSourceProvider* WebMediaPlayerImpl::audioSourceProvider() {
  return audio_source_provider_;
}

void WebMediaPlayerImpl::IncrementExternallyAllocatedMemory() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  incremented_externally_allocated_memory_ = true;
  v8::V8::AdjustAmountOfExternalAllocatedMemory(kPlayerExtraMemory);
}

}  // namespace webkit_media
