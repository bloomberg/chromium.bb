// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

// A lower-bound for the refresh interval.
constexpr base::TimeDelta kLowerBoundRefreshInterval =
    base::Hertz(media::limits::kMaxFramesPerSecond);

// This alias mimics the definition of VideoCaptureDeliverFrameCB.
using VideoCaptureDeliverFrameInternalCallback = WTF::CrossThreadFunction<void(
    scoped_refptr<media::VideoFrame> video_frame,
    std::vector<scoped_refptr<media::VideoFrame>> scaled_video_frames,
    base::TimeTicks estimated_capture_time)>;

// Mimics blink::EncodedVideoFrameCB
using EncodedVideoFrameInternalCallback =
    WTF::CrossThreadFunction<void(scoped_refptr<EncodedVideoFrame> frame,
                                  base::TimeTicks estimated_capture_time)>;

base::TimeDelta ComputeRefreshIntervalFromBounds(
    const base::TimeDelta required_min_refresh_interval,
    const absl::optional<double>& min_frame_rate,
    const absl::optional<double>& max_frame_rate) {
  // Start with the default required refresh interval, and refine based on
  // constraints. If a minimum frameRate is provided, use that. Otherwise, use
  // the maximum frameRate if it happens to be less than the default.
  base::TimeDelta refresh_interval = required_min_refresh_interval;
  if (min_frame_rate.has_value())
    refresh_interval = base::Hertz(*min_frame_rate);

  if (max_frame_rate.has_value()) {
    refresh_interval = std::max(refresh_interval, base::Hertz(*max_frame_rate));
  }

  if (refresh_interval < kLowerBoundRefreshInterval)
    refresh_interval = kLowerBoundRefreshInterval;

  return refresh_interval;
}

}  // namespace

// MediaStreamVideoTrack::FrameDeliverer is a helper class used for registering
// VideoCaptureDeliverFrameCB/EncodedVideoFrameCB callbacks on the main render
// thread to receive video frames on the IO-thread. Frames are only delivered to
// the sinks if the track is enabled. If the track is disabled, a black frame is
// instead forwarded to the sinks at the same frame rate. A disabled track does
// not forward data to encoded sinks.
class MediaStreamVideoTrack::FrameDeliverer
    : public WTF::ThreadSafeRefCounted<FrameDeliverer> {
 public:
  using VideoSinkId = WebMediaStreamSink*;

  FrameDeliverer(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                 base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track,
                 bool enabled);

  FrameDeliverer(const FrameDeliverer&) = delete;
  FrameDeliverer& operator=(const FrameDeliverer&) = delete;

  // Sets whether the track is enabled or not. If getting enabled and encoded
  // output is enabled, the deliverer will wait until the next key frame before
  // it resumes producing encoded data.
  void SetEnabled(bool enabled, bool await_key_frame);

  // Add |callback| to receive video frames on the IO-thread.
  // Must be called on the main render thread.
  void AddCallback(VideoSinkId id, VideoCaptureDeliverFrameCB callback);

  // Add |callback| to receive encoded video frames on the IO-thread.
  // Must be called on the main render thread.
  void AddEncodedCallback(VideoSinkId id, EncodedVideoFrameCB callback);

  // Removes |callback| associated with |id| from receiving video frames if |id|
  // has been added. It is ok to call RemoveCallback even if the |id| has not
  // been added. Note that the added callback will be reset on the main thread.
  // Must be called on the main render thread.
  void RemoveCallback(VideoSinkId id);

  // Removes encoded callback associated with |id| from receiving video frames
  // if |id| has been added. It is ok to call RemoveEncodedCallback even if the
  // |id| has not been added. Note that the added callback will be reset on the
  // main thread. Must be called on the main render thread.
  void RemoveEncodedCallback(VideoSinkId id);

  // Triggers all registered callbacks with |frame| and |estimated_capture_time|
  // as parameters. Must be called on the IO-thread.
  void DeliverFrameOnIO(
      scoped_refptr<media::VideoFrame> frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_video_frames,
      base::TimeTicks estimated_capture_time);

  // Triggers all encoded callbacks with |frame| and |estimated_capture_time|.
  // Must be called on the IO-thread.
  void DeliverEncodedVideoFrameOnIO(scoped_refptr<EncodedVideoFrame> frame,
                                    base::TimeTicks estimated_capture_time);

  void SetIsRefreshingForMinFrameRate(bool is_refreshing_for_min_frame_rate);

 private:
  friend class WTF::ThreadSafeRefCounted<FrameDeliverer>;
  virtual ~FrameDeliverer();
  void AddCallbackOnIO(VideoSinkId id,
                       VideoCaptureDeliverFrameInternalCallback callback);
  void RemoveCallbackOnIO(
      VideoSinkId id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void AddEncodedCallbackOnIO(VideoSinkId id,
                              EncodedVideoFrameInternalCallback callback);
  void RemoveEncodedCallbackOnIO(
      VideoSinkId id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void SetEnabledOnIO(bool enabled, bool await_key_frame);

  void SetIsRefreshingForMinFrameRateOnIO(
      bool is_refreshing_for_min_frame_rate);

  // Returns a black frame where the size and time stamp is set to the same as
  // as in |reference_frame|.
  scoped_refptr<media::VideoFrame> GetBlackFrame(
      const media::VideoFrame& reference_frame);

  // Used to DCHECK that AddCallback and RemoveCallback are called on the main
  // Render Thread.
  THREAD_CHECKER(main_render_thread_checker_);
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  // Can be null in testing.
  scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner_;

  base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track_;

  bool enabled_;
  scoped_refptr<media::VideoFrame> black_frame_;
  bool emit_frame_drop_events_;

  using VideoIdCallbackPair =
      std::pair<VideoSinkId, VideoCaptureDeliverFrameInternalCallback>;
  Vector<VideoIdCallbackPair> callbacks_;
  HashMap<VideoSinkId, EncodedVideoFrameInternalCallback> encoded_callbacks_;
  bool await_next_key_frame_;

  // This should only be accessed on the IO thread.
  bool is_refreshing_for_min_frame_rate_ = false;
};

MediaStreamVideoTrack::FrameDeliverer::FrameDeliverer(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track,
    bool enabled)
    : io_task_runner_(std::move(io_task_runner)),
      media_stream_video_track_(media_stream_video_track),
      enabled_(enabled),
      emit_frame_drop_events_(true),
      await_next_key_frame_(false) {
  DCHECK(io_task_runner_.get());

  WebLocalFrame* web_frame = WebLocalFrame::FrameForCurrentContext();
  if (web_frame) {
    main_render_task_runner_ =
        web_frame->GetTaskRunner(TaskType::kInternalMedia);
  }
}

MediaStreamVideoTrack::FrameDeliverer::~FrameDeliverer() {
  DCHECK(callbacks_.IsEmpty());
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallback(
    VideoSinkId id,
    VideoCaptureDeliverFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::AddCallbackOnIO,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          CrossThreadBindRepeating(std::move(callback))));
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallbackOnIO(
    VideoSinkId id,
    VideoCaptureDeliverFrameInternalCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  callbacks_.push_back(std::make_pair(id, std::move(callback)));
}

void MediaStreamVideoTrack::FrameDeliverer::AddEncodedCallback(
    VideoSinkId id,
    EncodedVideoFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::AddEncodedCallbackOnIO,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          CrossThreadBindRepeating(std::move(callback))));
}

void MediaStreamVideoTrack::FrameDeliverer::AddEncodedCallbackOnIO(
    VideoSinkId id,
    EncodedVideoFrameInternalCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  encoded_callbacks_.insert(id, std::move(callback));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallback(VideoSinkId id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::RemoveCallbackOnIO,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          Thread::Current()->GetTaskRunner()));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallbackOnIO(
    VideoSinkId id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto* it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == id) {
      // Callback destruction needs to happen on the specified task runner.
      PostCrossThreadTask(
          *task_runner, FROM_HERE,
          CrossThreadBindOnce(
              [](VideoCaptureDeliverFrameInternalCallback callback) {},
              std::move(it->second)));
      callbacks_.erase(it);
      return;
    }
  }
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveEncodedCallback(
    VideoSinkId id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::RemoveEncodedCallbackOnIO,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          Thread::Current()->GetTaskRunner()));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveEncodedCallbackOnIO(
    VideoSinkId id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Callback destruction needs to happen on the specified task runner.
  auto it = encoded_callbacks_.find(id);
  if (it == encoded_callbacks_.end())
    return;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce([](EncodedVideoFrameInternalCallback callback) {},
                          std::move(it->value)));
  encoded_callbacks_.erase(it);
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabled(bool enabled,
                                                       bool await_key_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::SetEnabledOnIO, WrapRefCounted(this),
                          enabled, await_key_frame));
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabledOnIO(
    bool enabled,
    bool await_key_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (enabled != enabled_) {
    enabled_ = enabled;
    emit_frame_drop_events_ = true;
  }
  if (enabled_) {
    black_frame_ = nullptr;
    await_next_key_frame_ = await_key_frame;
  }
}

void MediaStreamVideoTrack::FrameDeliverer::SetIsRefreshingForMinFrameRate(
    bool is_refreshing_for_min_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::SetIsRefreshingForMinFrameRateOnIO,
                          WrapRefCounted(this),
                          is_refreshing_for_min_frame_rate));
}

void MediaStreamVideoTrack::FrameDeliverer::SetIsRefreshingForMinFrameRateOnIO(
    bool is_refreshing_for_min_frame_rate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  is_refreshing_for_min_frame_rate_ = is_refreshing_for_min_frame_rate;
}

void MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO(
    scoped_refptr<media::VideoFrame> frame,
    std::vector<scoped_refptr<media::VideoFrame>> scaled_video_frames,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!enabled_ && main_render_task_runner_ && emit_frame_drop_events_) {
    emit_frame_drop_events_ = false;

    // TODO(crbug.com/964947): A weak ptr instance of MediaStreamVideoTrack is
    // passed to FrameDeliverer in order to avoid the re-binding the instance of
    // a WTF::CrossThreadFunction.
    PostCrossThreadTask(
        *main_render_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &MediaStreamVideoTrack::OnFrameDropped, media_stream_video_track_,
            media::VideoCaptureFrameDropReason::
                kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame));
  }
  scoped_refptr<media::VideoFrame> video_frame;
  if (enabled_) {
    video_frame = std::move(frame);
  } else {
    // When disabled, a black video frame is passed along instead. The original
    // frames are dropped.
    video_frame = GetBlackFrame(*frame);
    scaled_video_frames.clear();
  }
  for (const auto& entry : callbacks_)
    entry.second.Run(video_frame, scaled_video_frames, estimated_capture_time);

  // The delay on refresh timer is reset each time a frame is received so that
  // it will not fire for at least an additional period. This means refresh
  // frames will only be requested when the source has halted delivery (e.g., a
  // screen capturer stops sending frames because the screen is not being
  // updated).
  if (main_render_task_runner_ && is_refreshing_for_min_frame_rate_) {
    PostCrossThreadTask(
        *main_render_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&MediaStreamVideoTrack::ResetRefreshTimer,
                            media_stream_video_track_));
  }
}

void MediaStreamVideoTrack::FrameDeliverer::DeliverEncodedVideoFrameOnIO(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!enabled_) {
    return;
  }
  if (await_next_key_frame_ && !frame->IsKeyFrame()) {
    return;
  }
  await_next_key_frame_ = false;
  for (const auto& entry : encoded_callbacks_.Values()) {
    entry.Run(frame, estimated_capture_time);
  }
}

scoped_refptr<media::VideoFrame>
MediaStreamVideoTrack::FrameDeliverer::GetBlackFrame(
    const media::VideoFrame& reference_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!black_frame_.get() ||
      black_frame_->natural_size() != reference_frame.natural_size()) {
    black_frame_ =
        media::VideoFrame::CreateBlackFrame(reference_frame.natural_size());
  }

  // Wrap |black_frame_| so we get a fresh timestamp we can modify. Frames
  // returned from this function may still be in use.
  scoped_refptr<media::VideoFrame> wrapped_black_frame =
      media::VideoFrame::WrapVideoFrame(black_frame_, black_frame_->format(),
                                        black_frame_->visible_rect(),
                                        black_frame_->natural_size());
  if (!wrapped_black_frame)
    return nullptr;

  wrapped_black_frame->set_timestamp(reference_frame.timestamp());
  wrapped_black_frame->metadata().reference_time =
      reference_frame.metadata().reference_time;

  return wrapped_black_frame;
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled) {
  auto* component = MakeGarbageCollected<MediaStreamComponent>(source->Owner());
  component->SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      source, std::move(callback), enabled));
  return WebMediaStreamTrack(component);
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const VideoTrackAdapterSettings& adapter_settings,
    const absl::optional<bool>& noise_reduction,
    bool is_screencast,
    const absl::optional<double>& min_frame_rate,
    const absl::optional<double>& pan,
    const absl::optional<double>& tilt,
    const absl::optional<double>& zoom,
    bool pan_tilt_zoom_allowed,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled) {
  WebMediaStreamTrack track;
  auto* component = MakeGarbageCollected<MediaStreamComponent>(source->Owner());
  component->SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      source, adapter_settings, noise_reduction, is_screencast, min_frame_rate,
      pan, tilt, zoom, pan_tilt_zoom_allowed, std::move(callback), enabled));
  return WebMediaStreamTrack(component);
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::From(
    const MediaStreamComponent* component) {
  if (!component ||
      component->Source()->GetType() != MediaStreamSource::kTypeVideo) {
    return nullptr;
  }

  return static_cast<MediaStreamVideoTrack*>(component->GetPlatformTrack());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled)
    : MediaStreamTrackPlatform(true),
      is_screencast_(false),
      source_(source->GetWeakPtr()) {
  frame_deliverer_ =
      base::MakeRefCounted<MediaStreamVideoTrack::FrameDeliverer>(
          source->io_task_runner(), weak_factory_.GetWeakPtr(), enabled);
  source->AddTrack(
      this, VideoTrackAdapterSettings(),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
          frame_deliverer_)),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::DeliverEncodedVideoFrameOnIO,
          frame_deliverer_)),
      media::BindToCurrentLoop(WTF::BindRepeating(
          &MediaStreamVideoTrack::SetSizeAndComputedFrameRate,
          weak_factory_.GetWeakPtr())),
      media::BindToCurrentLoop(
          WTF::BindRepeating(&MediaStreamVideoTrack::set_computed_source_format,
                             weak_factory_.GetWeakPtr())),
      std::move(callback));
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const VideoTrackAdapterSettings& adapter_settings,
    const absl::optional<bool>& noise_reduction,
    bool is_screen_cast,
    const absl::optional<double>& min_frame_rate,
    const absl::optional<double>& pan,
    const absl::optional<double>& tilt,
    const absl::optional<double>& zoom,
    bool pan_tilt_zoom_allowed,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled)
    : MediaStreamTrackPlatform(true),
      adapter_settings_(adapter_settings),
      noise_reduction_(noise_reduction),
      is_screencast_(is_screen_cast),
      min_frame_rate_(min_frame_rate),
      pan_(pan),
      tilt_(tilt),
      zoom_(zoom),
      pan_tilt_zoom_allowed_(pan_tilt_zoom_allowed),
      source_(source->GetWeakPtr()) {
  frame_deliverer_ =
      base::MakeRefCounted<MediaStreamVideoTrack::FrameDeliverer>(
          source->io_task_runner(), weak_factory_.GetWeakPtr(), enabled);
  source->AddTrack(
      this, adapter_settings,
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
          frame_deliverer_)),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &MediaStreamVideoTrack::FrameDeliverer::DeliverEncodedVideoFrameOnIO,
          frame_deliverer_)),
      media::BindToCurrentLoop(WTF::BindRepeating(
          &MediaStreamVideoTrack::SetSizeAndComputedFrameRate,
          weak_factory_.GetWeakPtr())),
      media::BindToCurrentLoop(
          WTF::BindRepeating(&MediaStreamVideoTrack::set_computed_source_format,
                             weak_factory_.GetWeakPtr())),
      std::move(callback));
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(sinks_.IsEmpty());
  DCHECK(encoded_sinks_.IsEmpty());
  Stop();
  DVLOG(3) << "~MediaStreamVideoTrack()";
}

static void AddSinkInternal(Vector<WebMediaStreamSink*>* sinks,
                            WebMediaStreamSink* sink) {
  DCHECK(!base::Contains(*sinks, sink));
  sinks->push_back(sink);
}

static void RemoveSinkInternal(Vector<WebMediaStreamSink*>* sinks,
                               WebMediaStreamSink* sink) {
  auto** it = std::find(sinks->begin(), sinks->end(), sink);
  DCHECK(it != sinks->end());
  sinks->erase(it);
}

void MediaStreamVideoTrack::AddSink(
    WebMediaStreamSink* sink,
    const VideoCaptureDeliverFrameCB& callback,
    MediaStreamVideoSink::IsSecure is_secure,
    MediaStreamVideoSink::UsesAlpha uses_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  AddSinkInternal(&sinks_, sink);
  frame_deliverer_->AddCallback(sink, callback);
  secure_tracker_.Add(sink, is_secure == MediaStreamVideoSink::IsSecure::kYes);
  if (uses_alpha == MediaStreamVideoSink::UsesAlpha::kDefault) {
    alpha_using_sinks_.insert(sink);
  } else if (uses_alpha == MediaStreamVideoSink::UsesAlpha::kNo) {
    alpha_discarding_sinks_.insert(sink);
  }

  // Ensure sink gets told about any constraints set.
  sink->OnVideoConstraintsChanged(min_frame_rate_,
                                  adapter_settings_.max_frame_rate());

  // Request source to deliver a frame because a new sink is added.
  if (!source_)
    return;
  UpdateSourceHasConsumers();
  RequestRefreshFrame();
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());
  // Alpha can't be discarded if any sink uses alpha, or if the only sinks
  // connected are kDependsOnOtherSinks.
  const bool can_discard_alpha =
      alpha_using_sinks_.IsEmpty() && !alpha_discarding_sinks_.IsEmpty();
  source_->SetCanDiscardAlpha(can_discard_alpha);
  if (is_screencast_)
    StartTimerForRequestingFrames();
}

void MediaStreamVideoTrack::AddEncodedSink(WebMediaStreamSink* sink,
                                           EncodedVideoFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  AddSinkInternal(&encoded_sinks_, sink);
  frame_deliverer_->AddEncodedCallback(sink, std::move(callback));
  if (source_)
    source_->UpdateNumEncodedSinks();
  UpdateSourceHasConsumers();
}

void MediaStreamVideoTrack::RemoveSink(WebMediaStreamSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  RemoveSinkInternal(&sinks_, sink);
  alpha_using_sinks_.erase(sink);
  alpha_discarding_sinks_.erase(sink);
  frame_deliverer_->RemoveCallback(sink);
  secure_tracker_.Remove(sink);
  if (!source_)
    return;
  UpdateSourceHasConsumers();
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());
  const bool can_discard_alpha =
      sinks_.IsEmpty() ||
      (alpha_using_sinks_.IsEmpty() && !alpha_discarding_sinks_.IsEmpty());
  source_->SetCanDiscardAlpha(can_discard_alpha);
  // Restart the timer with existing sinks.
  if (is_screencast_)
    StartTimerForRequestingFrames();
}

void MediaStreamVideoTrack::RemoveEncodedSink(WebMediaStreamSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  RemoveSinkInternal(&encoded_sinks_, sink);
  frame_deliverer_->RemoveEncodedCallback(sink);
  if (source_)
    source_->UpdateNumEncodedSinks();
  UpdateSourceHasConsumers();
}

void MediaStreamVideoTrack::UpdateSourceHasConsumers() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;
  bool has_consumers = !sinks_.IsEmpty() || !encoded_sinks_.IsEmpty();
  source_->UpdateHasConsumers(this, has_consumers);
}

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  // If enabled, encoded sinks exist and the source supports encoded output, we
  // need a new keyframe from the source as we may have dropped data making the
  // stream undecodable.
  bool maybe_await_key_frame = false;
  if (enabled && source_ && source_->SupportsEncodedOutput() &&
      !encoded_sinks_.IsEmpty()) {
    RequestRefreshFrame();
    maybe_await_key_frame = true;
  }
  frame_deliverer_->SetEnabled(enabled, maybe_await_key_frame);
  for (auto* sink : sinks_)
    sink->OnEnabledChanged(enabled);
  for (auto* encoded_sink : encoded_sinks_)
    encoded_sink->OnEnabledChanged(enabled);
}

size_t MediaStreamVideoTrack::CountSinks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return sinks_.size();
}

size_t MediaStreamVideoTrack::CountEncodedSinks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return encoded_sinks_.size();
}

void MediaStreamVideoTrack::SetContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  for (auto* sink : sinks_)
    sink->OnContentHintChanged(content_hint);
  for (auto* encoded_sink : encoded_sinks_)
    encoded_sink->OnContentHintChanged(content_hint);
}

void MediaStreamVideoTrack::StopAndNotify(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (source_) {
    source_->RemoveTrack(this, std::move(callback));
    source_ = nullptr;
  } else if (callback) {
    std::move(callback).Run();
  }
  OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
  refresh_timer_.Stop();
}

void MediaStreamVideoTrack::GetSettings(
    MediaStreamTrackPlatform::Settings& settings) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;

  if (width_ && height_) {
    settings.width = width_;
    settings.height = height_;
    settings.aspect_ratio = static_cast<double>(width_) / height_;
  }

  // 0.0 means the track is using the source's frame rate.
  if (frame_rate_ != 0.0) {
    settings.frame_rate = frame_rate_;
  }

  absl::optional<media::VideoCaptureFormat> format =
      source_->GetCurrentFormat();
  if (format) {
    if (frame_rate_ == 0.0)
      settings.frame_rate = format->frame_rate;
    settings.video_kind = GetVideoKindForFormat(*format);
  } else {
    // Format is only set for local tracks. For other tracks, use the frame rate
    // reported through settings callback SetSizeAndComputedFrameRate().
    if (computed_frame_rate_)
      settings.frame_rate = *computed_frame_rate_;
  }

  settings.facing_mode = ToPlatformFacingMode(
      static_cast<mojom::blink::FacingMode>(source_->device().video_facing));
  settings.resize_mode = WebString::FromASCII(std::string(
      adapter_settings().target_size() ? WebMediaStreamTrack::kResizeModeRescale
                                       : WebMediaStreamTrack::kResizeModeNone));
  if (source_->device().display_media_info.has_value()) {
    const auto& info = source_->device().display_media_info.value();
    settings.display_surface = info->display_surface;
    settings.logical_surface = info->logical_surface;
    settings.cursor = info->cursor;
  }
}

MediaStreamTrackPlatform::CaptureHandle
MediaStreamVideoTrack::GetCaptureHandle() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  MediaStreamTrackPlatform::CaptureHandle capture_handle;

  if (!source_) {
    return capture_handle;
  }

  const MediaStreamDevice& device = source_->device();
  if (!device.display_media_info.has_value()) {
    return capture_handle;
  }
  const media::mojom::DisplayMediaInformationPtr& info =
      device.display_media_info.value();

  if (!info->capture_handle) {
    return capture_handle;
  }

  if (!info->capture_handle->origin.opaque()) {
    capture_handle.origin =
        String::FromUTF8(info->capture_handle->origin.Serialize());
  }
  capture_handle.handle =
      WebString::FromUTF16(info->capture_handle->capture_handle);

  return capture_handle;
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  // Copy the vectors first, since sinks might DisconnectFromTrack() and
  // invalidate iterators.

  Vector<WebMediaStreamSink*> sinks_copy(sinks_);
  for (auto* sink : sinks_copy)
    sink->OnReadyStateChanged(state);

  Vector<WebMediaStreamSink*> encoded_sinks_copy(encoded_sinks_);
  for (auto* encoded_sink : encoded_sinks_copy)
    encoded_sink->OnReadyStateChanged(state);
}

void MediaStreamVideoTrack::SetMinimumFrameRate(double min_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  min_frame_rate_ = min_frame_rate;
}

void MediaStreamVideoTrack::SetTrackAdapterSettings(
    const VideoTrackAdapterSettings& settings) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  adapter_settings_ = settings;
}

void MediaStreamVideoTrack::NotifyConstraintsConfigurationComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  for (auto* sink : sinks_) {
    sink->OnVideoConstraintsChanged(min_frame_rate_,
                                    adapter_settings_.max_frame_rate());
  }
}

media::VideoCaptureFormat MediaStreamVideoTrack::GetComputedSourceFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return computed_source_format_;
}

void MediaStreamVideoTrack::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;
  source_->OnFrameDropped(reason);
}

void MediaStreamVideoTrack::StartTimerForRequestingFrames() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  // Find the maximum of all the required min frames per second in the attached
  // sinks.
  double required_min_fps = 0;
  for (auto* web_sink : sinks_) {
    auto* sink = static_cast<MediaStreamVideoSink*>(web_sink);
    required_min_fps =
        std::max(required_min_fps, sink->GetRequiredMinFramesPerSec());
  }

  base::TimeDelta refresh_interval = ComputeRefreshIntervalFromBounds(
      base::Hertz(required_min_fps), min_frame_rate_, max_frame_rate_);

  if (refresh_interval.is_max()) {
    refresh_timer_.Stop();
    frame_deliverer_->SetIsRefreshingForMinFrameRate(false);
  } else {
    DVLOG(1) << "Starting frame refresh timer with interval "
             << refresh_interval.InMillisecondsF() << " ms.";
    refresh_timer_.Start(FROM_HERE, refresh_interval, this,
                         &MediaStreamVideoTrack::RequestRefreshFrame);
    frame_deliverer_->SetIsRefreshingForMinFrameRate(true);
  }
}

void MediaStreamVideoTrack::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (source_)
    source_->RequestRefreshFrame();
}

void MediaStreamVideoTrack::ResetRefreshTimer() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (refresh_timer_.IsRunning())
    refresh_timer_.Reset();
}

}  // namespace blink
