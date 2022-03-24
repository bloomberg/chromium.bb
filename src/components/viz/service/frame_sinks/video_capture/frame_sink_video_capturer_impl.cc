// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "components/viz/service/frame_sinks/video_capture/shared_memory_video_frame_pool.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureOracle;
using media::VideoFrame;
using media::VideoFrameMetadata;

#define UMA_HISTOGRAM_CAPTURE_DURATION_CUSTOM_TIMES(name, sample)         \
  UMA_HISTOGRAM_CUSTOM_TIMES(                                             \
      base::StringPrintf("Viz.FrameSinkVideoCapturer.%s.CaptureDuration", \
                         name),                                           \
      sample, base::Milliseconds(1), base::Seconds(1), 50)

namespace viz {

namespace {

// The largest Rect possible.
constexpr gfx::Rect kMaxRect = gfx::Rect(0,
                                         0,
                                         std::numeric_limits<int>::max(),
                                         std::numeric_limits<int>::max());

}  // namespace

// static
constexpr media::VideoPixelFormat
    FrameSinkVideoCapturerImpl::kDefaultPixelFormat;
// static
constexpr gfx::ColorSpace FrameSinkVideoCapturerImpl::kDefaultColorSpace;
// static
constexpr int FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames;
// static
constexpr int FrameSinkVideoCapturerImpl::kFramePoolCapacity;
// static
constexpr float FrameSinkVideoCapturerImpl::kTargetPipelineUtilization;

FrameSinkVideoCapturerImpl::FrameSinkVideoCapturerImpl(
    FrameSinkVideoCapturerManager* frame_sink_manager,
    mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver,
    std::unique_ptr<media::VideoCaptureOracle> oracle,
    bool log_to_webrtc)
    : frame_sink_manager_(frame_sink_manager),
      copy_request_source_(base::UnguessableToken::Create()),
      clock_(base::DefaultTickClock::GetInstance()),
      oracle_(std::move(oracle)),
      frame_pool_(
          std::make_unique<SharedMemoryVideoFramePool>(kFramePoolCapacity)),
      feedback_weak_factory_(oracle_.get()),
      log_to_webrtc_(log_to_webrtc) {
  DCHECK(frame_sink_manager_);
  DCHECK(oracle_);
  if (log_to_webrtc_) {
    oracle_->SetLogCallback(base::BindRepeating(
        &FrameSinkVideoCapturerImpl::OnLog, base::Unretained(this)));
  }
  // Instantiate a default base::OneShotTimer instance.
  refresh_frame_retry_timer_.emplace();

  if (receiver.is_valid()) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&FrameSinkVideoCapturerManager::OnCapturerConnectionLost,
                       base::Unretained(frame_sink_manager_), this));
  }
}

FrameSinkVideoCapturerImpl::~FrameSinkVideoCapturerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
  SetResolvedTarget(nullptr);
}

void FrameSinkVideoCapturerImpl::SetResolvedTarget(
    CapturableFrameSink* target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (resolved_target_ == target) {
    return;
  }

  TRACE_EVENT_INSTANT2(
      "gpu.capture", "SetResolvedTarget", TRACE_EVENT_SCOPE_THREAD, "current",
      resolved_target_ ? resolved_target_->GetFrameSinkId().ToString() : "None",
      "new", target ? target->GetFrameSinkId().ToString() : "None");

  if (resolved_target_) {
    resolved_target_->DetachCaptureClient(this);
  }
  resolved_target_ = target;
  if (resolved_target_) {
    resolved_target_->AttachCaptureClient(this);
    RefreshEntireSourceSoon();
  } else {
    // The capturer will remain idle until either: 1) the requested target is
    // re-resolved by the |frame_sink_manager_|, or 2) a new target is set via a
    // call to ChangeTarget().
  }
}

void FrameSinkVideoCapturerImpl::OnTargetWillGoAway() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetResolvedTarget(nullptr);
}

void FrameSinkVideoCapturerImpl::SetFormat(media::VideoPixelFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool format_changed = false;

  if (format != media::PIXEL_FORMAT_I420 &&
      format != media::PIXEL_FORMAT_ARGB) {
    LOG(DFATAL) << "Invalid pixel format: Only I420 and ARGB are supported.";
  } else {
    format_changed |= (pixel_format_ != format);
    pixel_format_ = format;
  }

  if (format_changed) {
    TRACE_EVENT_INSTANT1("gpu.capture", "SetFormat", TRACE_EVENT_SCOPE_THREAD,
                         "format", format);

    MarkFrame(nullptr);
    RefreshEntireSourceSoon();
  }
}

void FrameSinkVideoCapturerImpl::SetMinCapturePeriod(
    base::TimeDelta min_capture_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr base::TimeDelta kMinMinCapturePeriod = base::Microseconds(
      base::Time::kMicrosecondsPerSecond / media::limits::kMaxFramesPerSecond);
  if (min_capture_period < kMinMinCapturePeriod) {
    min_capture_period = kMinMinCapturePeriod;
  }

  // On machines without high-resolution clocks, limit the maximum frame rate to
  // 30 FPS. This avoids a potential issue where the system clock may not
  // advance between two successive frames.
  if (!base::TimeTicks::IsHighResolution()) {
    constexpr base::TimeDelta kMinLowResCapturePeriod =
        base::Microseconds(base::Time::kMicrosecondsPerSecond / 30);
    if (min_capture_period < kMinLowResCapturePeriod) {
      min_capture_period = kMinLowResCapturePeriod;
    }
  }

  TRACE_EVENT_INSTANT1("gpu.capture", "SetMinCapturePeriod",
                       TRACE_EVENT_SCOPE_THREAD, "min_capture_period",
                       min_capture_period);

  oracle_->SetMinCapturePeriod(min_capture_period);
  if (refresh_frame_retry_timer_->IsRunning()) {
    // With the change in the minimum capture period, a pending refresh might
    // be ready to execute now (or sooner than it would have been).
    RefreshSoon();
  }
}

void FrameSinkVideoCapturerImpl::SetMinSizeChangePeriod(
    base::TimeDelta min_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_INSTANT1("gpu.capture", "SetMinSizeChangePeriod",
                       TRACE_EVENT_SCOPE_THREAD, "min_size_change_period",
                       min_period);

  oracle_->SetMinSizeChangePeriod(min_period);
}

void FrameSinkVideoCapturerImpl::SetResolutionConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_INSTANT2("gpu.capture", "SetResolutionConstraints",
                       TRACE_EVENT_SCOPE_THREAD, "min_size.width",
                       min_size.width(), "min_size.height", min_size.height());
  TRACE_EVENT_INSTANT2("gpu.capture", "SetResolutionConstraints",
                       TRACE_EVENT_SCOPE_THREAD, "max_size.width",
                       max_size.width(), "max_size.height", max_size.height());

  if (min_size.width() <= 0 || min_size.height() <= 0 ||
      max_size.width() > media::limits::kMaxDimension ||
      max_size.height() > media::limits::kMaxDimension ||
      min_size.width() > max_size.width() ||
      min_size.height() > max_size.height()) {
    LOG(DFATAL) << "Invalid resolutions constraints: " << min_size.ToString()
                << " must not be greater than " << max_size.ToString()
                << "; and also within media::limits.";
    return;
  }

  oracle_->SetCaptureSizeConstraints(min_size, max_size,
                                     use_fixed_aspect_ratio);
  RefreshEntireSourceSoon();
}

void FrameSinkVideoCapturerImpl::SetAutoThrottlingEnabled(bool enabled) {
  TRACE_EVENT_INSTANT1("gpu.capture", "SetAutoThrottlingEnabled",
                       TRACE_EVENT_SCOPE_THREAD, "autothrottling_enabled",
                       enabled);

  oracle_->SetAutoThrottlingEnabled(enabled);
}

void FrameSinkVideoCapturerImpl::ChangeTarget(
    const absl::optional<VideoCaptureTarget>& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  target_ = target;

  CapturableFrameSink* resolved_target = nullptr;
  if (target_) {
    resolved_target =
        frame_sink_manager_->FindCapturableFrameSink(target_->frame_sink_id);
  }
  SetResolvedTarget(resolved_target);
}

void FrameSinkVideoCapturerImpl::Start(
    mojo::PendingRemote<mojom::FrameSinkVideoConsumer> consumer,
    mojom::BufferFormatPreference buffer_format_preference) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(consumer);

  if (video_capture_started_)
    Stop();

  video_capture_started_ = true;
  buffer_format_preference_ = buffer_format_preference;

  if (resolved_target_)
    resolved_target_->OnClientCaptureStarted();

  consumer_.Bind(std::move(consumer));
  // In the future, if the connection to the consumer is lost before a call to
  // Stop(), make that call on its behalf.
  consumer_.set_disconnect_handler(base::BindOnce(
      &FrameSinkVideoCapturerImpl::Stop, base::Unretained(this)));
  RefreshEntireSourceSoon();
}

void FrameSinkVideoCapturerImpl::Stop() {
  if (!video_capture_started_)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  refresh_frame_retry_timer_->Stop();

  // Cancel any captures in-flight and any captured frames pending delivery.
  capture_weak_factory_.InvalidateWeakPtrs();
  oracle_->CancelAllCaptures();
  while (!delivery_queue_.empty()) {
    delivery_queue_.pop();
  }
  next_delivery_frame_number_ = next_capture_frame_number_;

  if (consumer_) {
    consumer_->OnStopped();
    consumer_.reset();
  }

  if (resolved_target_)
    resolved_target_->OnClientCaptureStopped();

  video_capture_started_ = false;
  buffer_format_preference_ = mojom::BufferFormatPreference::kDefault;
}

void FrameSinkVideoCapturerImpl::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!refresh_frame_retry_timer_->IsRunning()) {
    RefreshSoon();
  }
}

void FrameSinkVideoCapturerImpl::CreateOverlay(
    int32_t stacking_index,
    mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This will cause an existing overlay with the same stacking index to be
  // dropped, per mojom-documented behavior.
  overlays_.emplace(stacking_index, std::make_unique<VideoCaptureOverlay>(
                                        this, std::move(receiver)));
}

void FrameSinkVideoCapturerImpl::ScheduleRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  refresh_frame_retry_timer_->Start(
      FROM_HERE, GetDelayBeforeNextRefreshAttempt(),
      base::BindRepeating(&FrameSinkVideoCapturerImpl::RefreshSoon,
                          base::Unretained(this)));
}

base::TimeDelta FrameSinkVideoCapturerImpl::GetDelayBeforeNextRefreshAttempt()
    const {
  // The delay should be long enough to prevent interrupting the smooth timing
  // of frame captures that are expected to take place due to compositor update
  // events. However, the delay should not be excessively long either. Two frame
  // periods should be "just right."
  return 2 * std::max(oracle_->estimated_frame_duration(),
                      oracle_->min_capture_period());
}

void FrameSinkVideoCapturerImpl::RefreshEntireSourceSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InvalidateEntireSource();
  RefreshSoon();
}

void FrameSinkVideoCapturerImpl::RefreshSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If consumption is stopped, cancel the refresh.
  if (!consumer_) {
    return;
  }

  // If the capture target has not yet been resolved, the refresh must be
  // attempted later.
  if (!resolved_target_) {
    ScheduleRefreshFrame();
    return;
  }

  // Detect whether the source size changed before attempting capture.
  DCHECK(target_);
  const gfx::Rect capture_region =
      resolved_target_->GetCopyOutputRequestRegion(target_->sub_target);
  if (capture_region.IsEmpty()) {
    // If the capture region is empty, it means one of two things: the first
    // frame has not been composited yet or the current region selected for
    // capture has a current size of zero. We schedule a frame refresh here,
    // although its not useful in all circumstances.
    ScheduleRefreshFrame();
    return;
  }

  if (capture_region.size() != oracle_->source_size()) {
    oracle_->SetSourceSize(capture_region.size());
    InvalidateEntireSource();
    if (log_to_webrtc_) {
      consumer_->OnLog(
          base::StringPrintf("FrameSinkVideoCapturerImpl::RefreshSoon() "
                             "changed active frame size: %s",
                             capture_region.size().ToString().c_str()));
    }
  }

  MaybeCaptureFrame(VideoCaptureOracle::kRefreshRequest, gfx::Rect(),
                    clock_->NowTicks(),
                    *resolved_target_->GetLastActivatedFrameMetadata());
}

void FrameSinkVideoCapturerImpl::OnFrameDamaged(
    const gfx::Size& frame_size,
    const gfx::Rect& damage_rect,
    base::TimeTicks expected_display_time,
    const CompositorFrameMetadata& frame_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!frame_size.IsEmpty());
  DCHECK(!damage_rect.IsEmpty());
  DCHECK(!expected_display_time.is_null());
  DCHECK(resolved_target_);
  DCHECK(target_);

  const gfx::Rect capture_region =
      resolved_target_->GetCopyOutputRequestRegion(target_->sub_target);
  if (capture_region.IsEmpty()) {
    return;
  }

  if (capture_region.size() == oracle_->source_size()) {
    if (!absl::holds_alternative<absl::monostate>(target_->sub_target)) {
      // The damage_rect may not be in the same coordinate space when we have
      // a valid request subtree identifier, so to be safe we just invalidate
      // the entire source.
      InvalidateEntireSource();
    } else {
      InvalidateRect(damage_rect);
    }
  } else {
    oracle_->SetSourceSize(capture_region.size());
    InvalidateEntireSource();
    if (log_to_webrtc_ && consumer_) {
      consumer_->OnLog(base::StringPrintf(
          "FrameSinkVideoCapturerImpl::OnFrameDamaged() changed frame size: %s",
          capture_region.size().ToString().c_str()));
    }
  }

  MaybeCaptureFrame(VideoCaptureOracle::kCompositorUpdate, damage_rect,
                    expected_display_time, frame_metadata);
}

bool FrameSinkVideoCapturerImpl::IsVideoCaptureStarted() {
  return video_capture_started_;
}

gfx::Size FrameSinkVideoCapturerImpl::GetSourceSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return oracle_->source_size();
}

void FrameSinkVideoCapturerImpl::InvalidateRect(const gfx::Rect& rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Rect positive_rect = rect;
  positive_rect.Intersect(kMaxRect);
  dirty_rect_.Union(positive_rect);
  content_version_++;
}

void FrameSinkVideoCapturerImpl::InvalidateEntireSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dirty_rect_ = kMaxRect;
  content_version_++;
}

void FrameSinkVideoCapturerImpl::OnOverlayConnectionLost(
    VideoCaptureOverlay* overlay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it =
      std::find_if(overlays_.begin(), overlays_.end(),
                   [&overlay](const decltype(overlays_)::value_type& entry) {
                     return entry.second.get() == overlay;
                   });
  DCHECK(it != overlays_.end());
  overlays_.erase(it);
}

std::vector<VideoCaptureOverlay*>
FrameSinkVideoCapturerImpl::GetOverlaysInOrder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<VideoCaptureOverlay*> list;
  list.reserve(overlays_.size());
  for (const auto& entry : overlays_) {
    list.push_back(entry.second.get());
  }
  return list;
}

FrameSinkVideoCapturerImpl::CaptureRequestProperties::CaptureRequestProperties(
    int64_t capture_frame_number,
    OracleFrameNumber oracle_frame_number,
    int64_t content_version,
    gfx::Rect content_rect,
    gfx::Rect capture_rect,
    gfx::Rect active_frame_rect,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks request_time)
    : capture_frame_number(capture_frame_number),
      oracle_frame_number(oracle_frame_number),
      content_version(content_version),
      content_rect(content_rect),
      capture_rect(capture_rect),
      active_frame_rect(active_frame_rect),
      frame(std::move(frame)),
      request_time(request_time) {}

FrameSinkVideoCapturerImpl::CaptureRequestProperties::
    CaptureRequestProperties() = default;
FrameSinkVideoCapturerImpl::CaptureRequestProperties::CaptureRequestProperties(
    const FrameSinkVideoCapturerImpl::CaptureRequestProperties&) = default;
FrameSinkVideoCapturerImpl::CaptureRequestProperties::CaptureRequestProperties(
    FrameSinkVideoCapturerImpl::CaptureRequestProperties&&) = default;
FrameSinkVideoCapturerImpl::CaptureRequestProperties&
FrameSinkVideoCapturerImpl::CaptureRequestProperties::operator=(
    const FrameSinkVideoCapturerImpl::CaptureRequestProperties&) = default;
FrameSinkVideoCapturerImpl::CaptureRequestProperties&
FrameSinkVideoCapturerImpl::CaptureRequestProperties::operator=(
    FrameSinkVideoCapturerImpl::CaptureRequestProperties&&) = default;
FrameSinkVideoCapturerImpl::CaptureRequestProperties::
    ~CaptureRequestProperties() = default;

void FrameSinkVideoCapturerImpl::MaybeCaptureFrame(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time,
    const CompositorFrameMetadata& frame_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(resolved_target_);
  DCHECK(target_);

  // Consult the oracle to determine whether this frame should be captured.
  if (oracle_->ObserveEventAndDecideCapture(event, damage_rect, event_time)) {
    // Regardless of the type of |event|, there is no longer a need for the
    // refresh frame retry timer to fire. The following is a no-op, if the timer
    // was not running.
    refresh_frame_retry_timer_->Stop();
  } else {
    TRACE_EVENT_INSTANT1("gpu.capture", "FpsRateLimited",
                         TRACE_EVENT_SCOPE_THREAD, "trigger",
                         VideoCaptureOracle::EventAsString(event));

    // Whether the oracle rejected a compositor update or a refresh event,
    // the consumer needs to be provided an update in the near future.
    ScheduleRefreshFrame();
    return;
  }

  // If there is no |consumer_| present, punt. This check is being done after
  // consulting the oracle because it helps to "prime" the oracle in the short
  // period of time where the capture target is known but the |consumer_| has
  // not yet been provided in the call to Start().
  if (!consumer_) {
    TRACE_EVENT_INSTANT1("gpu.capture", "NoConsumer", TRACE_EVENT_SCOPE_THREAD,
                         "trigger", VideoCaptureOracle::EventAsString(event));
    return;
  }

  // Reserve a buffer from the pool for the next frame.
  const OracleFrameNumber oracle_frame_number = oracle_->next_frame_number();
  const gfx::Size capture_size =
      AdjustSizeForPixelFormat(oracle_->capture_size());

  const bool can_resurrect_content = CanResurrectFrame(capture_size);
  scoped_refptr<VideoFrame> frame;
  if (can_resurrect_content) {
    TRACE_EVENT_INSTANT0("gpu.capture", "UsingResurrectedFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    frame = ResurrectFrame();
  } else {
    frame = frame_pool_->ReserveVideoFrame(pixel_format_, capture_size);
  }

  // Compute the current in-flight utilization and attenuate it: The utilization
  // reported to the oracle is in terms of a maximum sustainable amount (not the
  // absolute maximum).
  const float utilization =
      GetPipelineUtilization() / kTargetPipelineUtilization;

  // Do not proceed if the pool did not provide a frame: This indicates the
  // pipeline is full.
  if (!frame) {
    TRACE_EVENT_INSTANT2(
        "gpu.capture", "PipelineLimited", TRACE_EVENT_SCOPE_THREAD, "trigger",
        VideoCaptureOracle::EventAsString(event), "atten_util_percent",
        base::saturated_cast<int>(utilization * 100.0f + 0.5f));
    oracle_->RecordWillNotCapture(utilization);
    if (next_capture_frame_number_ == 0) {
      // The pool was unable to provide a buffer for the very first capture, and
      // so there is no expectation of recovery. Thus, treat this as a fatal
      // memory allocation issue instead of a transient one.
      LOG(ERROR) << "Unable to allocate shmem for first frame capture: OOM?";
      Stop();
    } else {
      ScheduleRefreshFrame();
    }
    return;
  }

  // Record a trace event if the capture pipeline is redlining, but capture will
  // still proceed.
  if (utilization >= 1.0) {
    TRACE_EVENT_INSTANT2(
        "gpu.capture", "NearlyPipelineLimited", TRACE_EVENT_SCOPE_THREAD,
        "trigger", VideoCaptureOracle::EventAsString(event),
        "atten_util_percent",
        base::saturated_cast<int>(utilization * 100.0f + 0.5f));
  }

  // At this point, the capture is going to proceed. Populate the VideoFrame's
  // metadata, and notify the oracle.
  const int64_t capture_frame_number = next_capture_frame_number_++;
  VideoFrameMetadata& metadata = frame->metadata();
  metadata.capture_begin_time = clock_->NowTicks();
  metadata.capture_counter = capture_frame_number;
  metadata.frame_duration = oracle_->estimated_frame_duration();
  metadata.frame_rate = 1.0 / oracle_->min_capture_period().InSecondsF();
  metadata.reference_time = event_time;
  metadata.device_scale_factor = frame_metadata.device_scale_factor;
  metadata.page_scale_factor = frame_metadata.page_scale_factor;
  metadata.root_scroll_offset_x = frame_metadata.root_scroll_offset.x();
  metadata.root_scroll_offset_y = frame_metadata.root_scroll_offset.y();
  if (frame_metadata.top_controls_visible_height.has_value()) {
    last_top_controls_visible_height_ =
        *frame_metadata.top_controls_visible_height;
  }
  metadata.top_controls_visible_height = last_top_controls_visible_height_;

  oracle_->RecordCapture(utilization);
  TRACE_EVENT_ASYNC_BEGIN2("gpu.capture", "Capture", oracle_frame_number,
                           "frame_number", capture_frame_number, "trigger",
                           VideoCaptureOracle::EventAsString(event));

  const gfx::Size& source_size = oracle_->source_size();
  DCHECK(!source_size.IsEmpty());
  gfx::Rect content_rect;
  if (pixel_format_ == media::PIXEL_FORMAT_I420) {
    content_rect = media::ComputeLetterboxRegionForI420(frame->visible_rect(),
                                                        source_size);
  } else {
    DCHECK_EQ(media::PIXEL_FORMAT_ARGB, pixel_format_);
    content_rect =
        media::ComputeLetterboxRegion(frame->visible_rect(), source_size);
    // The media letterboxing computation explicitly allows for off-by-one
    // errors due to computation, so we address those here.
    if (content_rect.ApproximatelyEqual(frame->visible_rect(), 1)) {
      content_rect = frame->visible_rect();
    }
  }

  // Determine what rectangular region has changed since the last captured
  // frame.
  gfx::Rect update_rect;
  if (dirty_rect_ == kMaxRect ||
      frame->visible_rect() != last_frame_visible_rect_) {
    // Source or VideoFrame size change: Assume entire frame (including
    // letterboxed regions) have changed.
    update_rect = last_frame_visible_rect_ = frame->visible_rect();
  } else {
    update_rect = copy_output::ComputeResultRect(
        dirty_rect_, gfx::Vector2d(source_size.width(), source_size.height()),
        gfx::Vector2d(content_rect.width(), content_rect.height()));
    update_rect.Offset(content_rect.OffsetFromOrigin());
    if (pixel_format_ == media::PIXEL_FORMAT_I420)
      update_rect = ExpandRectToI420SubsampleBoundaries(update_rect);
  }
  metadata.capture_update_rect = update_rect;

  // If the frame is a resurrected one, just deliver it since it already
  // contains the most up-to-date capture of the source content.
  if (can_resurrect_content) {
    if (log_to_webrtc_) {
      std::string strides = "";
      switch (frame->format()) {
        case media::PIXEL_FORMAT_I420:
          strides = base::StringPrintf("strideY:%d StrideU:%d StrideV:%d",
                                       frame->stride(VideoFrame::kYPlane),
                                       frame->stride(VideoFrame::kUPlane),
                                       frame->stride(VideoFrame::kVPlane));
          break;
        case media::PIXEL_FORMAT_ARGB:
          strides = base::StringPrintf("strideRGBA:%d",
                                       frame->stride(VideoFrame::kARGBPlane));
          break;
        default:
          strides = "strides:???";
      }
      consumer_->OnLog(base::StringPrintf(
          "FrameSinkVideoCapturerImpl: Resurrecting frame format=%s "
          "frame_coded_size: %s "
          "frame_visible_rect: %s frame_natural_size: %s %s",
          VideoPixelFormatToString(frame->format()).c_str(),
          frame->coded_size().ToString().c_str(),
          frame->visible_rect().ToString().c_str(),
          frame->natural_size().ToString().c_str(), strides.c_str()));
    }
    OnFrameReadyForDelivery(capture_frame_number, oracle_frame_number,
                            content_rect, std::move(frame));
    return;
  }

  // At this point, we know the frame is not resurrected, so there will be only
  // one reference to it (held by us). It means that we are free to mutate the
  // frame's pixel content however we want.
  DCHECK(frame->HasOneRef());

  // Extreme edge-case: If somehow the source size is so tiny that the content
  // region becomes empty, just deliver a frame filled with black.
  if (content_rect.IsEmpty()) {
    if (pixel_format_ == media::PIXEL_FORMAT_I420) {
      media::FillYUV(frame.get(), 0x00, 0x80, 0x80);
      frame->set_color_space(gfx::ColorSpace::CreateREC709());
    } else {
      DCHECK_EQ(pixel_format_, media::PIXEL_FORMAT_ARGB);
      media::LetterboxVideoFrame(frame.get(), gfx::Rect());
      frame->set_color_space(gfx::ColorSpace::CreateSRGB());
    }
    dirty_rect_ = gfx::Rect();
    OnFrameReadyForDelivery(capture_frame_number, oracle_frame_number,
                            gfx::Rect(), std::move(frame));
    return;
  }

  // The oracle only keeps track of the source size, which should be the
  // size of the capture region. If the capture region is empty, we shouldn't
  // capture.
  const gfx::Rect capture_region =
      resolved_target_->GetCopyOutputRequestRegion(target_->sub_target);
  if (capture_region.IsEmpty()) {
    return;
  }
  DCHECK(capture_region.size() == source_size);
  CaptureRequestProperties request_properties(
      capture_frame_number, oracle_frame_number, content_version_, content_rect,
      capture_region,
      resolved_target_->GetCopyOutputRequestRegion(VideoCaptureSubTarget{}),
      std::move(frame), base::TimeTicks::Now());

  // Request a copy of the next frame from the frame sink.
  auto request = std::make_unique<CopyOutputRequest>(
      pixel_format_ == media::PIXEL_FORMAT_I420
          ? CopyOutputRequest::ResultFormat::I420_PLANES
          : CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&FrameSinkVideoCapturerImpl::DidCopyFrame,
                     capture_weak_factory_.GetWeakPtr(),
                     std::move(request_properties)));
  request->set_result_task_runner(base::SequencedTaskRunnerHandle::Get());
  request->set_source(copy_request_source_);
  request->set_area(capture_region);
  request->SetScaleRatio(
      gfx::Vector2d(source_size.width(), source_size.height()),
      gfx::Vector2d(content_rect.width(), content_rect.height()));
  // TODO(crbug.com/775740): As an optimization, set the result selection to
  // just the part of the result that would have changed due to aggregated
  // damage over all the frames that weren't captured.
  request->set_result_selection(gfx::Rect(content_rect.size()));

  // Clear the |dirty_rect_|, to indicate all changes at the source are now
  // being captured.
  dirty_rect_ = gfx::Rect();

  if (log_to_webrtc_) {
    std::string format =
        pixel_format_ == media::PIXEL_FORMAT_I420 ? "I420" : "RGBA_bitmap";
    consumer_->OnLog(base::StringPrintf(
        "FrameSinkVideoCapturerImpl: Sending CopyRequest: "
        "format=%s area:%s "
        "scale_from: %s "
        "scale_to: %s "
        "frame pool utilization: %f",
        format.c_str(), request->area().ToString().c_str(),
        request->scale_from().ToString().c_str(),
        request->scale_to().ToString().c_str(), utilization));
  }

  const SubtreeCaptureId subtree_id =
      absl::holds_alternative<SubtreeCaptureId>(target_->sub_target)
          ? absl::get<SubtreeCaptureId>(target_->sub_target)
          : SubtreeCaptureId();

  resolved_target_->RequestCopyOfOutput(
      {LocalSurfaceId(), subtree_id, std::move(request)});
}

void FrameSinkVideoCapturerImpl::DidCopyFrame(
    CaptureRequestProperties properties,
    std::unique_ptr<CopyOutputResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(properties.capture_frame_number, next_delivery_frame_number_);
  DCHECK(properties.frame);
  DCHECK(result);

  scoped_refptr<media::VideoFrame>& frame = properties.frame;
  const gfx::Rect& content_rect = properties.content_rect;
  if (log_to_webrtc_ && consumer_) {
    std::string format = "";
    std::string strides = "";
    switch (result->format()) {
      case CopyOutputResult::Format::I420_PLANES:
        format = "I420";
        strides = base::StringPrintf("strideY:%d StrideU:%d StrideV:%d",
                                     frame->stride(VideoFrame::kYPlane),
                                     frame->stride(VideoFrame::kUPlane),
                                     frame->stride(VideoFrame::kVPlane));
        break;
      case CopyOutputResult::Format::NV12_PLANES:
        format = "NV12";
        strides = base::StringPrintf("strideY:%d StrideUV:%d",
                                     frame->stride(VideoFrame::kYPlane),
                                     frame->stride(VideoFrame::kUVPlane));
        break;
      case CopyOutputResult::Format::RGBA:
        strides = base::StringPrintf("strideRGBA:%d",
                                     frame->stride(VideoFrame::kARGBPlane));

        switch (result->destination()) {
          case CopyOutputResult::Destination::kSystemMemory:
            format = "RGBA_Bitmap";
            break;
          case CopyOutputResult::Destination::kNativeTextures:
            format = "RGBA_Texture";
            break;
        }
        break;
    }
    consumer_->OnLog(base::StringPrintf(
        "FrameSinkVideoCapturerImpl: got CopyOutputResult: format=%s size:%s "
        "frame_coded_size: %s frame_visible_rect: %s frame_natural_size: %s "
        "content_rect: %s %s",
        format.c_str(), result->size().ToString().c_str(),
        frame->coded_size().ToString().c_str(),
        frame->visible_rect().ToString().c_str(),
        frame->natural_size().ToString().c_str(),
        content_rect.ToString().c_str(), strides.c_str()));
  }

  // Stop() should have canceled any outstanding copy requests. So, by reaching
  // this point, |consumer_| should be bound.
  DCHECK(consumer_);

  if (pixel_format_ == media::PIXEL_FORMAT_I420) {
    DCHECK_EQ(content_rect.x() % 2, 0);
    DCHECK_EQ(content_rect.y() % 2, 0);
    DCHECK_EQ(content_rect.width() % 2, 0);
    DCHECK_EQ(content_rect.height() % 2, 0);
    // Populate the VideoFrame from the CopyOutputResult.
    const int y_stride = frame->stride(VideoFrame::kYPlane);
    uint8_t* const y = frame->visible_data(VideoFrame::kYPlane) +
                       content_rect.y() * y_stride + content_rect.x();
    const int u_stride = frame->stride(VideoFrame::kUPlane);
    uint8_t* const u = frame->visible_data(VideoFrame::kUPlane) +
                       (content_rect.y() / 2) * u_stride +
                       (content_rect.x() / 2);
    const int v_stride = frame->stride(VideoFrame::kVPlane);
    uint8_t* const v = frame->visible_data(VideoFrame::kVPlane) +
                       (content_rect.y() / 2) * v_stride +
                       (content_rect.x() / 2);
    bool success =
        result->ReadI420Planes(y, y_stride, u, u_stride, v, v_stride);
    if (success) {
      // Per CopyOutputResult header comments, I420_PLANES results are always in
      // the Rec.709 color space.
      frame->set_color_space(gfx::ColorSpace::CreateREC709());
      UMA_HISTOGRAM_CAPTURE_DURATION_CUSTOM_TIMES(
          "I420", base::TimeTicks::Now() - properties.request_time);
    } else {
      frame = nullptr;
    }
    UMA_HISTOGRAM_BOOLEAN("Viz.FrameSinkVideoCapturer.I420.CaptureSucceeded",
                          success);
  } else {
    int stride = frame->stride(VideoFrame::kARGBPlane);
    DCHECK_EQ(media::PIXEL_FORMAT_ARGB, pixel_format_);
    uint8_t* const pixels = frame->visible_data(VideoFrame::kARGBPlane) +
                            content_rect.y() * stride + content_rect.x() * 4;
    bool success = result->ReadRGBAPlane(pixels, stride);
    if (success) {
      frame->set_color_space(result->GetRGBAColorSpace());
      UMA_HISTOGRAM_CAPTURE_DURATION_CUSTOM_TIMES(
          "RGBA", base::TimeTicks::Now() - properties.request_time);
    } else {
      frame = nullptr;
    }
    UMA_HISTOGRAM_BOOLEAN("Viz.FrameSinkVideoCapturer.RGBA.CaptureSucceeded",
                          success);
  }

  if (frame) {
    gfx::Rect sub_region = properties.capture_rect;
    // In some cases, the content_rect is smaller than the capture_rect.
    sub_region.ClampToCenteredSize(content_rect.size());

    auto overlay_renderer = VideoCaptureOverlay::MakeCombinedRenderer(
        GetOverlaysInOrder(),
        VideoCaptureOverlay::CapturedFrameProperties{
            properties.active_frame_rect, sub_region, frame->format()});
    if (overlay_renderer) {
      std::move(overlay_renderer).Run(frame.get());
    }

    // The result may be smaller than what was requested, if unforeseen
    // clamping to the source boundaries occurred by the executor of the
    // CopyOutputRequest. However, the result should never contain more than
    // what was requested.
    DCHECK_LE(result->size().width(), content_rect.width());
    DCHECK_LE(result->size().height(), content_rect.height());
    media::LetterboxVideoFrame(
        frame.get(), gfx::Rect(content_rect.origin(),
                               AdjustSizeForPixelFormat(result->size())));

    if (ShouldMark(*frame, properties.content_version)) {
      MarkFrame(frame, properties.content_version);
    }
  }

  OnFrameReadyForDelivery(properties.capture_frame_number,
                          properties.oracle_frame_number, content_rect,
                          std::move(frame));
}

void FrameSinkVideoCapturerImpl::OnFrameReadyForDelivery(
    int64_t capture_frame_number,
    OracleFrameNumber oracle_frame_number,
    const gfx::Rect& content_rect,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(capture_frame_number, next_delivery_frame_number_);

  // From this point onward, we're not allowed to mutate |frame|'s pixels as we
  // may be operating on a resurrected frame.

  if (frame)
    frame->metadata().capture_end_time = clock_->NowTicks();

  // Ensure frames are delivered in-order by using a min-heap, and only
  // deliver the next frame(s) in-sequence when they are found at the top.
  delivery_queue_.emplace(capture_frame_number, oracle_frame_number,
                          content_rect, std::move(frame));
  while (delivery_queue_.top().capture_frame_number ==
         next_delivery_frame_number_) {
    auto& next = delivery_queue_.top();
    MaybeDeliverFrame(next.oracle_frame_number, next.content_rect,
                      std::move(next.frame));
    ++next_delivery_frame_number_;
    delivery_queue_.pop();
    if (delivery_queue_.empty()) {
      break;
    }
  }
}

void FrameSinkVideoCapturerImpl::MaybeDeliverFrame(
    OracleFrameNumber oracle_frame_number,
    const gfx::Rect& content_rect,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The Oracle has the final say in whether frame delivery will proceed. It
  // also rewrites the media timestamp in terms of the smooth flow of the
  // original source content.
  base::TimeTicks media_ticks;
  if (!oracle_->CompleteCapture(oracle_frame_number, !!frame, &media_ticks)) {
    // The following is used by
    // chrome/browser/extension/api/cast_streaming/performance_test.cc, in
    // addition to the usual runtime tracing.
    TRACE_EVENT_ASYNC_END1("gpu.capture", "Capture", oracle_frame_number,
                           "success", false);

    ScheduleRefreshFrame();
    return;
  }

  // Set media timestamp in terms of the time offset since the first frame.
  if (!first_frame_media_ticks_) {
    first_frame_media_ticks_ = media_ticks;
  }
  frame->set_timestamp(media_ticks - *first_frame_media_ticks_);

  // The following is used by
  // chrome/browser/extension/api/cast_streaming/performance_test.cc, in
  // addition to the usual runtime tracing.
  TRACE_EVENT_ASYNC_END2("gpu.capture", "Capture", oracle_frame_number,
                         "success", true, "time_delta",
                         frame->timestamp().InMicroseconds());

  // Clone a handle to the shared memory backing the populated video frame, to
  // send to the consumer.
  auto handle = frame_pool_->CloneHandleForDelivery(*frame);
  DCHECK(handle);
  DCHECK(handle->is_read_only_shmem_region());
  DCHECK(handle->get_read_only_shmem_region().IsValid());

  // Assemble frame layout, format, and metadata into a mojo struct to send to
  // the consumer.
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
  info->timestamp = frame->timestamp();
  info->metadata = frame->metadata();
  info->pixel_format = frame->format();
  info->coded_size = frame->coded_size();
  info->visible_rect = frame->visible_rect();
  DCHECK(frame->ColorSpace().IsValid());  // Ensure it was set by this point.
  info->color_space = frame->ColorSpace();

  // Create an InFlightFrameDelivery for this frame, owned by its mojo receiver.
  // It responds to the consumer's Done() notification by returning the video
  // frame to the |frame_pool_|. It responds to the optional ProvideFeedback()
  // by forwarding the measurement to the |oracle_|.
  mojo::PendingRemote<mojom::FrameSinkVideoConsumerFrameCallbacks> callbacks;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<InFlightFrameDelivery>(
          base::BindOnce(&FrameSinkVideoCapturerImpl::NotifyFrameReleased,
                         capture_weak_factory_.GetWeakPtr(), std::move(frame)),
          base::BindOnce(&VideoCaptureOracle::RecordConsumerFeedback,
                         feedback_weak_factory_.GetWeakPtr(),
                         oracle_frame_number)),
      callbacks.InitWithNewPipeAndPassReceiver());

  num_frames_in_flight_++;
  TRACE_COUNTER_ID1("gpu.capture",
                    "FrameSinkVideoCapturerImpl::num_frames_in_flight_", this,
                    num_frames_in_flight_);

  // Send the frame to the consumer.
  consumer_->OnFrameCaptured(std::move(handle), std::move(info), content_rect,
                             std::move(callbacks));
}

gfx::Size FrameSinkVideoCapturerImpl::AdjustSizeForPixelFormat(
    const gfx::Size& raw_size) {
  if (pixel_format_ == media::PIXEL_FORMAT_ARGB) {
    gfx::Size result(raw_size);
    if (result.width() <= 0)
      result.set_width(1);
    if (result.height() <= 0)
      result.set_height(1);
    return result;
  }
  DCHECK_EQ(media::PIXEL_FORMAT_I420, pixel_format_);
  gfx::Size result(raw_size.width() & ~1, raw_size.height() & ~1);
  if (result.width() <= 0)
    result.set_width(2);
  if (result.height() <= 0)
    result.set_height(2);
  return result;
}

// static
gfx::Rect FrameSinkVideoCapturerImpl::ExpandRectToI420SubsampleBoundaries(
    const gfx::Rect& rect) {
  const int x = rect.x() & ~1;
  const int y = rect.y() & ~1;
  const int r = rect.right() + (rect.right() & 1);
  const int b = rect.bottom() + (rect.bottom() & 1);
  return gfx::Rect(x, y, r - x, b - y);
}

void FrameSinkVideoCapturerImpl::OnLog(const std::string& message) {
  if (log_to_webrtc_ && consumer_) {
    consumer_->OnLog(message);
  }
}

bool FrameSinkVideoCapturerImpl::ShouldMark(const media::VideoFrame& frame,
                                            int64_t content_version) const {
  return !marked_frame_ || content_version > content_version_in_marked_frame_ ||
         frame.coded_size() != marked_frame_->coded_size();
}

void FrameSinkVideoCapturerImpl::MarkFrame(
    scoped_refptr<media::VideoFrame> frame,
    int64_t content_version) {
  marked_frame_ = frame;
  content_version_in_marked_frame_ = marked_frame_ ? content_version : -1;
}

bool FrameSinkVideoCapturerImpl::CanResurrectFrame(
    const gfx::Size& size) const {
  return content_version_ == content_version_in_marked_frame_ &&
         marked_frame_ && marked_frame_->coded_size() == size;
}

void FrameSinkVideoCapturerImpl::NotifyFrameReleased(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  num_frames_in_flight_--;

  TRACE_COUNTER_ID1("gpu.capture",
                    "FrameSinkVideoCapturerImpl::num_frames_in_flight_", this,
                    num_frames_in_flight_);
}

float FrameSinkVideoCapturerImpl::GetPipelineUtilization() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return static_cast<float>(num_frames_in_flight_) / kDesignLimitMaxFrames;
}

FrameSinkVideoCapturerImpl::CapturedFrame::CapturedFrame(
    int64_t capture_frame_number,
    OracleFrameNumber oracle_frame_number,
    const gfx::Rect& content_rect,
    scoped_refptr<media::VideoFrame> frame)
    : capture_frame_number(capture_frame_number),
      oracle_frame_number(oracle_frame_number),
      content_rect(content_rect),
      frame(std::move(frame)) {}

FrameSinkVideoCapturerImpl::CapturedFrame::CapturedFrame(
    const CapturedFrame& other) = default;

FrameSinkVideoCapturerImpl::CapturedFrame::~CapturedFrame() = default;

bool FrameSinkVideoCapturerImpl::CapturedFrame::operator<(
    const FrameSinkVideoCapturerImpl::CapturedFrame& other) const {
  // Reverse the sort order; so std::priority_queue<CapturedFrame> becomes a
  // min-heap instead of a max-heap.
  return other.capture_frame_number < capture_frame_number;
}

}  // namespace viz
