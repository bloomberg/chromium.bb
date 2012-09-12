// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_impl.h"

#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_loop.h"
#include "media/base/filter_collection.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/chunk_demuxer.h"
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
#include "webkit/media/webmediaplayer_delegate.h"
#include "webkit/media/webmediaplayer_proxy.h"
#include "webkit/media/webmediaplayer_util.h"
#include "webkit/media/webvideoframe_impl.h"
#include "webkit/plugins/ppapi/ppapi_webplugin_impl.h"

using WebKit::WebCanvas;
using WebKit::WebMediaPlayer;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
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

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(WebKit::WebMediaPlayer::CORSMode ## name) == \
                 static_cast<int>(BufferedResourceLoader::k ## name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(Unspecified);
COMPILE_ASSERT_MATCHING_ENUM(Anonymous);
COMPILE_ASSERT_MATCHING_ENUM(UseCredentials);
#undef COMPILE_ASSERT_MATCHING_ENUM

#define BIND_TO_RENDER_LOOP(function) \
  media::BindToLoop(main_loop_->message_loop_proxy(), base::Bind( \
      function, AsWeakPtr()))

static WebKit::WebTimeRanges ConvertToWebTimeRanges(
    const media::Ranges<base::TimeDelta>& ranges) {
  WebKit::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

// TODO(acolwell): Investigate whether the key_system & session_id parameters
// are really necessary.
typedef base::Callback<void(const std::string&,
                            const std::string&,
                            scoped_array<uint8>,
                            int)> OnNeedKeyCB;

static void OnDemuxerNeedKeyTrampoline(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const OnNeedKeyCB& need_key_cb,
    scoped_array<uint8> init_data,
    int init_data_size) {
  message_loop->PostTask(FROM_HERE, base::Bind(
      need_key_cb, "", "", base::Passed(&init_data), init_data_size));
}

WebMediaPlayerImpl::WebMediaPlayerImpl(
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    media::FilterCollection* collection,
    WebKit::WebAudioSourceProvider* audio_source_provider,
    media::AudioRendererSink* audio_renderer_sink,
    media::MessageLoopFactory* message_loop_factory,
    MediaStreamClient* media_stream_client,
    media::MediaLog* media_log)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      main_loop_(MessageLoop::current()),
      filter_collection_(collection),
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
      audio_source_provider_(audio_source_provider),
      audio_renderer_sink_(audio_renderer_sink),
      is_local_source_(false),
      supports_save_(true),
      decryptor_(proxy_.get(), client, frame),
      starting_(false) {
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  scoped_refptr<base::MessageLoopProxy> pipeline_message_loop =
      message_loop_factory_->GetMessageLoop(
          media::MessageLoopFactory::kPipeline);
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
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetOpaque),
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

void WebMediaPlayerImpl::load(const WebKit::WebURL& url, CORSMode cors_mode) {
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
    supports_save_ = false;
    StartPipeline();
    return;
  }

  // Media source pipelines can start immediately.
  if (!url.isEmpty() && url == GetClient()->sourceURL()) {
    chunk_demuxer_ = new media::ChunkDemuxer(
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
        base::Bind(&OnDemuxerNeedKeyTrampoline,
                   main_loop_->message_loop_proxy(),
                   base::Bind(&WebMediaPlayerImpl::OnNeedKey, AsWeakPtr())));

    BuildMediaSourceCollection(chunk_demuxer_,
                               message_loop_factory_.get(),
                               filter_collection_.get(),
                               &decryptor_);
    supports_save_ = false;
    StartPipeline();
    return;
  }

  // Otherwise it's a regular request which requires resolving the URL first.
  proxy_->set_data_source(
      new BufferedDataSource(main_loop_, frame_, media_log_,
                             base::Bind(&WebMediaPlayerImpl::NotifyDownloading,
                                        AsWeakPtr())));
  proxy_->data_source()->Initialize(
      url, static_cast<BufferedResourceLoader::CORSMode>(cors_mode),
      base::Bind(
          &WebMediaPlayerImpl::DataSourceInitialized,
          AsWeakPtr(), gurl));

  is_local_source_ = !gurl.SchemeIs("http") && !gurl.SchemeIs("https");

  BuildDefaultCollection(proxy_->data_source(),
                         message_loop_factory_.get(),
                         filter_collection_.get(),
                         &decryptor_);
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
  paused_time_ = pipeline_->GetMediaTime();

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
  return supports_save_;
}

void WebMediaPlayerImpl::seek(float seconds) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (starting_ || seeking_) {
    pending_seek_ = true;
    pending_seek_seconds_ = seconds;
    if (chunk_demuxer_)
      chunk_demuxer_->CancelPendingSeek();
    return;
  }

  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  // Update our paused time.
  if (paused_)
    paused_time_ = seek_time;

  seeking_ = true;

  if (chunk_demuxer_)
    chunk_demuxer_->StartWaitingForSeek();

  // Kick off the asynchronous seek!
  pipeline_->Seek(
      seek_time,
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek));
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
#undef COMPILE_ASSERT_MATCHING_ENUM

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
  return static_cast<float>(pipeline_->GetMediaTime().InSecondsF());
}

int WebMediaPlayerImpl::dataRate() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::networkState() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::readyState() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return ready_state_;
}

const WebKit::WebTimeRanges& WebMediaPlayerImpl::buffered() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  WebKit::WebTimeRanges web_ranges(
      ConvertToWebTimeRanges(pipeline_->GetBufferedTimeRanges()));
  buffered_.swap(web_ranges);
  return buffered_;
}

float WebMediaPlayerImpl::maxTimeSeekable() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // We don't support seeking in streaming media.
  if (proxy_ && proxy_->data_source() && proxy_->data_source()->IsStreaming())
    return 0.0f;
  return static_cast<float>(pipeline_->GetMediaDuration().InSecondsF());
}

bool WebMediaPlayerImpl::didLoadingProgress() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return pipeline_->DidLoadingProgress();
}

unsigned long long WebMediaPlayerImpl::totalBytes() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetTotalBytes();
}

void WebMediaPlayerImpl::setSize(const WebSize& size) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // Don't need to do anything as we use the dimensions passed in via paint().
}

void WebMediaPlayerImpl::paint(WebCanvas* canvas,
                               const WebRect& rect,
                               uint8_t alpha) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(proxy_);

  if (!accelerated_compositing_reported_) {
    accelerated_compositing_reported_ = true;
    // Normally paint() is only called in non-accelerated rendering, but there
    // are exceptions such as webgl where compositing is used in the WebView but
    // video frames are still rendered to a canvas.
    UMA_HISTOGRAM_BOOLEAN(
        "Media.AcceleratedCompositingActive",
        frame_->view()->isAcceleratedCompositingActive());
  }

  proxy_->Paint(canvas, rect, alpha);
}

bool WebMediaPlayerImpl::hasSingleSecurityOrigin() const {
  if (proxy_)
    return proxy_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerImpl::didPassCORSAccessCheck() const {
  return proxy_ && proxy_->DidPassCORSAccessCheck();
}

WebMediaPlayer::MovieLoadType WebMediaPlayerImpl::movieLoadType() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // Disable seeking while streaming.
  if (proxy_ && proxy_->data_source() && proxy_->data_source()->IsStreaming())
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
    DCHECK(frame_->view()->isAcceleratedCompositingActive());
    UMA_HISTOGRAM_BOOLEAN("Media.AcceleratedCompositingActive", true);
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
    const WebKit::WebString& type,
    const WebKit::WebVector<WebKit::WebString>& codecs) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  std::vector<std::string> new_codecs(codecs.size());
  for (size_t i = 0; i < codecs.size(); ++i)
    new_codecs[i] = codecs[i].utf8().data();

  return static_cast<WebKit::WebMediaPlayer::AddIdStatus>(
      chunk_demuxer_->AddId(id.utf8().data(), type.utf8().data(), new_codecs));
}

bool WebMediaPlayerImpl::sourceRemoveId(const WebKit::WebString& id) {
  DCHECK(!id.isEmpty());
  chunk_demuxer_->RemoveId(id.utf8().data());
  return true;
}

WebKit::WebTimeRanges WebMediaPlayerImpl::sourceBuffered(
    const WebKit::WebString& id) {
  return ConvertToWebTimeRanges(
      chunk_demuxer_->GetBufferedRanges(id.utf8().data()));
}

bool WebMediaPlayerImpl::sourceAppend(const WebKit::WebString& id,
                                      const unsigned char* data,
                                      unsigned length) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  float old_duration = duration();
  if (!chunk_demuxer_->AppendData(id.utf8().data(), data, length))
    return false;

  if (old_duration != duration())
    GetClient()->durationChanged();

  return true;
}

bool WebMediaPlayerImpl::sourceAbort(const WebKit::WebString& id) {
  chunk_demuxer_->Abort(id.utf8().data());
  return true;
}

void WebMediaPlayerImpl::sourceSetDuration(double new_duration) {
  if (static_cast<double>(duration()) == new_duration)
    return;

  chunk_demuxer_->SetDuration(
      base::TimeDelta::FromMicroseconds(
          new_duration * base::Time::kMicrosecondsPerSecond));
  GetClient()->durationChanged();
}

void WebMediaPlayerImpl::sourceEndOfStream(
    WebMediaPlayer::EndOfStreamStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  media::PipelineStatus pipeline_status = media::PIPELINE_OK;

  switch (status) {
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

  float old_duration = duration();
  chunk_demuxer_->EndOfStream(pipeline_status);

  if (old_duration != duration())
    GetClient()->durationChanged();
}

bool WebMediaPlayerImpl::sourceSetTimestampOffset(const WebKit::WebString& id,
                                                  double offset) {
  base::TimeDelta time_offset = base::TimeDelta::FromMicroseconds(
      offset * base::Time::kMicrosecondsPerSecond);
  return chunk_demuxer_->SetTimestampOffset(id.utf8().data(), time_offset);
}

WebKit::WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::generateKeyRequest(const WebString& key_system,
                                       const unsigned char* init_data,
                                       unsigned init_data_length) {
  if (!IsSupportedKeySystem(key_system))
    return WebKit::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.isEmpty())
    current_key_system_ = key_system;
  else if (key_system != current_key_system_)
    return WebKit::WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  DVLOG(1) << "generateKeyRequest: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  if (!decryptor_.GenerateKeyRequest(key_system.utf8(),
                                     init_data, init_data_length)) {
    current_key_system_.reset();
    return WebKit::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

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

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebKit::WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  DVLOG(1) << "addKey: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length))
           << " [" << session_id.utf8().data() << "]";

  decryptor_.AddKey(key_system.utf8(), key, key_length,
                    init_data, init_data_length, session_id.utf8());
  return WebKit::WebMediaPlayer::MediaKeyExceptionNoError;
}

WebKit::WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  if (!IsSupportedKeySystem(key_system))
    return WebKit::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebKit::WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  decryptor_.CancelKeyRequest(key_system.utf8(), session_id.utf8());
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

void WebMediaPlayerImpl::OnPipelineSeek(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  starting_ = false;
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
    paused_time_ = pipeline_->GetMediaTime();

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

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
    Repaint();
    return;
  }

  switch (error) {
    case media::PIPELINE_OK:
      NOTREACHED() << "PIPELINE_OK isn't an error!";
      break;

    case media::PIPELINE_ERROR_NETWORK:
    case media::PIPELINE_ERROR_READ:
      SetNetworkState(WebMediaPlayer::NetworkStateNetworkError);
      break;

    // TODO(vrk): Because OnPipelineInitialize() directly reports the
    // NetworkStateFormatError instead of calling OnPipelineError(), I believe
    // this block can be deleted. Should look into it! (crbug.com/126070)
    case media::PIPELINE_ERROR_INITIALIZATION_FAILED:
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

    case media::PIPELINE_STATUS_MAX:
      NOTREACHED() << "PIPELINE_STATUS_MAX isn't a real error!";
      break;
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnPipelineBufferingState(
    media::Pipeline::BufferingState buffering_state) {
  DVLOG(1) << "OnPipelineBufferingState(" << buffering_state << ")";

  switch (buffering_state) {
    case media::Pipeline::kHaveMetadata:
      SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
      break;
    case media::Pipeline::kPrerollCompleted:
      SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
      break;
  }

  // Repaint to trigger UI update.
  Repaint();
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  GetClient()->sourceOpened();
}

void WebMediaPlayerImpl::OnKeyAdded(const std::string& key_system,
                                    const std::string& session_id) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->keyAdded(WebString::fromUTF8(key_system),
                        WebString::fromUTF8(session_id));
}

void WebMediaPlayerImpl::OnNeedKey(const std::string& key_system,
                                   const std::string& session_id,
                                   scoped_array<uint8> init_data,
                                   int init_data_size) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->keyNeeded(WebString::fromUTF8(key_system),
                         WebString::fromUTF8(session_id),
                         init_data.get(),
                         init_data_size);
}

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(WebKit::WebMediaPlayerClient::name) == \
                 static_cast<int>(media::Decryptor::k ## name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(UnknownError);
COMPILE_ASSERT_MATCHING_ENUM(ClientError);
COMPILE_ASSERT_MATCHING_ENUM(ServiceError);
COMPILE_ASSERT_MATCHING_ENUM(OutputError);
COMPILE_ASSERT_MATCHING_ENUM(HardwareChangeError);
COMPILE_ASSERT_MATCHING_ENUM(DomainError);
#undef COMPILE_ASSERT_MATCHING_ENUM

void WebMediaPlayerImpl::OnKeyError(const std::string& key_system,
                                    const std::string& session_id,
                                    media::Decryptor::KeyError error_code,
                                    int system_code) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->keyError(
      WebString::fromUTF8(key_system),
      WebString::fromUTF8(session_id),
      static_cast<WebKit::WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      system_code);
}

void WebMediaPlayerImpl::OnKeyMessage(const std::string& key_system,
                                      const std::string& session_id,
                                      scoped_array<uint8> message,
                                      int message_length,
                                      const std::string& /* default_url */) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->keyMessage(WebString::fromUTF8(key_system),
                          WebString::fromUTF8(session_id),
                          message.get(),
                          message_length);
}

void WebMediaPlayerImpl::SetOpaque(bool opaque) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->setOpaque(opaque);
}

void WebMediaPlayerImpl::DataSourceInitialized(const GURL& gurl, bool success) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (!success) {
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
    Repaint();
    return;
  }

  StartPipeline();
}

void WebMediaPlayerImpl::NotifyDownloading(bool is_downloading) {
  if (!is_downloading && network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateIdle);
  else if (is_downloading && network_state_ == WebMediaPlayer::NetworkStateIdle)
    SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  media_log_->AddEvent(
      media_log_->CreateBooleanEvent(
          media::MediaLogEvent::NETWORK_ACTIVITY_SET,
          "is_downloading_data", is_downloading));
}

void WebMediaPlayerImpl::StartPipeline() {
  starting_ = true;
  pipeline_->Start(
      filter_collection_.Pass(),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingState));
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

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing &&
      state >= WebMediaPlayer::ReadyStateHaveMetadata) {
    if (!hasVideo())
      GetClient()->disableAcceleratedCompositing();
  } else if (state == WebMediaPlayer::ReadyStateHaveEnoughData) {
    if (is_local_source_ &&
        network_state_ == WebMediaPlayer::NetworkStateLoading) {
      SetNetworkState(WebMediaPlayer::NetworkStateLoaded);
    }
  }

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
    if (chunk_demuxer_) {
      chunk_demuxer_->Shutdown();
      chunk_demuxer_ = NULL;
    }
  }

  // Make sure to kill the pipeline so there's no more media threads running.
  // Note: stopping the pipeline might block for a long time.
  base::WaitableEvent waiter(false, false);
  pipeline_->Stop(base::Bind(
      &base::WaitableEvent::Signal, base::Unretained(&waiter)));
  waiter.Wait();

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
