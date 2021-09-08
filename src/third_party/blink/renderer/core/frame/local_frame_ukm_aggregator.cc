// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include <memory>

#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    scoped_refptr<LocalFrameUkmAggregator> aggregator,
    size_t metric_index,
    const base::TickClock* clock)
    : aggregator_(aggregator),
      metric_index_(metric_index),
      clock_(clock),
      start_time_(clock_->NowTicks()) {}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    ScopedUkmHierarchicalTimer&& other)
    : aggregator_(other.aggregator_),
      metric_index_(other.metric_index_),
      clock_(other.clock_),
      start_time_(other.start_time_) {
  other.aggregator_ = nullptr;
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::
    ~ScopedUkmHierarchicalTimer() {
  if (aggregator_ && base::TimeTicks::IsHighResolution()) {
    aggregator_->RecordSample(metric_index_, start_time_, clock_->NowTicks());
  }
}

void LocalFrameUkmAggregator::AbsoluteMetricRecord::reset() {
  interval_duration = base::TimeDelta();
  main_frame_duration = base::TimeDelta();
}

LocalFrameUkmAggregator::LocalFrameUkmAggregator(int64_t source_id,
                                                 ukm::UkmRecorder* recorder)
    : source_id_(source_id),
      recorder_(recorder),
      clock_(base::DefaultTickClock::GetInstance()),
      event_name_("Blink.UpdateTime") {
  // All of these are assumed to have one entry per sub-metric.
  DCHECK_EQ(base::size(absolute_metric_records_), metrics_data().size());
  DCHECK_EQ(base::size(current_sample_.sub_metrics_durations),
            metrics_data().size());
  DCHECK_EQ(base::size(current_sample_.sub_main_frame_durations),
            metrics_data().size());

  // Record average and worst case for the primary metric.
  primary_metric_.reset();

  // Define the UMA for the primary metric.
  primary_metric_.pre_fcp_uma_counter = std::make_unique<CustomCountHistogram>(
      "Blink.MainFrame.UpdateTime.PreFCP", 0, 10000000, 50);
  primary_metric_.post_fcp_uma_counter = std::make_unique<CustomCountHistogram>(
      "Blink.MainFrame.UpdateTime.PostFCP", 0, 10000000, 50);
  primary_metric_.uma_aggregate_counter =
      std::make_unique<CustomCountHistogram>(
          "Blink.MainFrame.UpdateTime.AggregatedPreFCP", 0, 10000000, 50);

  // Set up the substrings to create the UMA names
  const char* const uma_preamble = "Blink.";
  const char* const uma_postscript = ".UpdateTime";
  const char* const uma_prefcp_postscript = ".PreFCP";
  const char* const uma_postfcp_postscript = ".PostFCP";
  const char* const uma_pre_fcp_aggregated_postscript = ".AggregatedPreFCP";

  // Populate all the sub-metrics.
  size_t metric_index = 0;
  for (const MetricInitializationData& metric_data : metrics_data()) {
    // Absolute records report the absolute time for each metric per frame.
    // They also aggregate the time spent in each stage between navigation
    // (LocalFrameView resets) and First Contentful Paint.
    // They have an associated UMA too that we own and allocate here.
    auto& absolute_record = absolute_metric_records_[metric_index];
    absolute_record.reset();
    absolute_record.pre_fcp_aggregate = base::TimeDelta();
    if (metric_data.has_uma) {
      StringBuilder uma_name;
      uma_name.Append(uma_preamble);
      uma_name.Append(metric_data.name);
      uma_name.Append(uma_postscript);
      StringBuilder pre_fcp_uma_name;
      pre_fcp_uma_name.Append(uma_name);
      pre_fcp_uma_name.Append(uma_prefcp_postscript);
      absolute_record.pre_fcp_uma_counter =
          std::make_unique<CustomCountHistogram>(
              pre_fcp_uma_name.ToString().Utf8().c_str(), 0, 10000000, 50);
      StringBuilder post_fcp_uma_name;
      post_fcp_uma_name.Append(uma_name);
      post_fcp_uma_name.Append(uma_postfcp_postscript);
      absolute_record.post_fcp_uma_counter =
          std::make_unique<CustomCountHistogram>(
              post_fcp_uma_name.ToString().Utf8().c_str(), 0, 10000000, 50);
      StringBuilder aggregated_uma_name;
      aggregated_uma_name.Append(uma_name);
      aggregated_uma_name.Append(uma_pre_fcp_aggregated_postscript);
      absolute_record.uma_aggregate_counter =
          std::make_unique<CustomCountHistogram>(
              aggregated_uma_name.ToString().Utf8().c_str(), 0, 10000000, 50);
    }

    metric_index++;
  }
}

LocalFrameUkmAggregator::~LocalFrameUkmAggregator() {
  ReportUpdateTimeEvent();
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer
LocalFrameUkmAggregator::GetScopedTimer(size_t metric_index) {
  return ScopedUkmHierarchicalTimer(this, metric_index, clock_);
}

void LocalFrameUkmAggregator::BeginMainFrame() {
  DCHECK(!in_main_frame_update_);
  in_main_frame_update_ = true;
}

std::unique_ptr<cc::BeginMainFrameMetrics>
LocalFrameUkmAggregator::GetBeginMainFrameMetrics() {
  DCHECK(InMainFrameUpdate());

  // Use the main_frame_percentage_records_ because they are the ones that
  // only count time between the Begin and End of a main frame update.
  // Do not report hit testing because it is a sub-portion of the other
  // metrics and would result in double counting.
  std::unique_ptr<cc::BeginMainFrameMetrics> metrics_data =
      std::make_unique<cc::BeginMainFrameMetrics>();
  metrics_data->handle_input_events =
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kHandleInputEvents)]
          .main_frame_duration;
  metrics_data->animate =
      absolute_metric_records_[static_cast<unsigned>(MetricId::kAnimate)]
          .main_frame_duration;
  metrics_data->style_update =
      absolute_metric_records_[static_cast<unsigned>(MetricId::kStyle)]
          .main_frame_duration;
  metrics_data->layout_update =
      absolute_metric_records_[static_cast<unsigned>(MetricId::kLayout)]
          .main_frame_duration;
  metrics_data->prepaint =
      absolute_metric_records_[static_cast<unsigned>(MetricId::kPrePaint)]
          .main_frame_duration;
  metrics_data->compositing_assignments =
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingAssignments)]
          .main_frame_duration;
  metrics_data->compositing_inputs =
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingInputs)]
          .main_frame_duration;
  metrics_data->paint =
      absolute_metric_records_[static_cast<unsigned>(MetricId::kPaint)]
          .main_frame_duration;
  metrics_data->composite_commit =
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingCommit)]
          .main_frame_duration;
  metrics_data->should_measure_smoothness =
      (fcp_state_ >= kThisFrameReachedFCP);
  return metrics_data;
}

void LocalFrameUkmAggregator::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

void LocalFrameUkmAggregator::DidReachFirstContentfulPaint(
    bool are_painting_main_frame) {
  DCHECK(fcp_state_ != kHavePassedFCP);

  if (!are_painting_main_frame) {
    DCHECK(AllMetricsAreZero());
    return;
  }

  fcp_state_ = kThisFrameReachedFCP;
}

void LocalFrameUkmAggregator::RecordSample(size_t metric_index,
                                           base::TimeTicks start,
                                           base::TimeTicks end) {
  // Always use RecordForcedLayoutSample for the kForcedStyleAndLayout
  // metric id.
  DCHECK_NE(metric_index, static_cast<size_t>(kForcedStyleAndLayout));

  base::TimeDelta duration = end - start;
  bool is_pre_fcp = (fcp_state_ != kHavePassedFCP);

  // Accumulate for UKM and record the UMA
  DCHECK_LT(metric_index, base::size(absolute_metric_records_));
  auto& record = absolute_metric_records_[metric_index];
  record.interval_duration += duration;
  if (in_main_frame_update_)
    record.main_frame_duration += duration;
  if (is_pre_fcp)
    record.pre_fcp_aggregate += duration;
  // Record the UMA
  // ForcedStyleAndLayout happen so frequently on some pages that we overflow
  // the signed 32 counter for number of events in a 30 minute period. So
  // randomly record with probability 1/100.
  if (record.pre_fcp_uma_counter) {
    if (is_pre_fcp) {
      record.pre_fcp_uma_counter->CountMicroseconds(duration);
    } else {
      record.post_fcp_uma_counter->CountMicroseconds(duration);
    }
  }
}

void LocalFrameUkmAggregator::RecordForcedLayoutSample(
    DocumentUpdateReason reason,
    base::TimeTicks start,
    base::TimeTicks end) {
  base::TimeDelta duration = end - start;
  bool is_pre_fcp = (fcp_state_ != kHavePassedFCP);

  // Accumulate for UKM always, but only record the UMA for a subset of cases to
  // avoid overflowing the counters.
  bool should_report_uma_this_frame = !calls_to_next_forced_style_layout_uma_;
  if (should_report_uma_this_frame) {
    calls_to_next_forced_style_layout_uma_ =
        base::RandInt(0, mean_calls_between_forced_style_layout_uma_ * 2);
  } else {
    DCHECK_GT(calls_to_next_forced_style_layout_uma_, 0u);
    --calls_to_next_forced_style_layout_uma_;
  }

  auto& record =
      absolute_metric_records_[static_cast<size_t>(kForcedStyleAndLayout)];
  record.interval_duration += duration;
  if (in_main_frame_update_)
    record.main_frame_duration += duration;
  if (is_pre_fcp)
    record.pre_fcp_aggregate += duration;

  if (should_report_uma_this_frame) {
    if (is_pre_fcp)
      record.pre_fcp_uma_counter->CountMicroseconds(duration);
    else
      record.post_fcp_uma_counter->CountMicroseconds(duration);
  }

  // Record a variety of DocumentUpdateReasons as distinct metrics
  // Figure out which sub-metric, if any, we wish to report for UKM.
  MetricId sub_metric = kCount;
  switch (reason) {
    case DocumentUpdateReason::kContextMenu:
    case DocumentUpdateReason::kDragImage:
    case DocumentUpdateReason::kEditing:
    case DocumentUpdateReason::kFindInPage:
    case DocumentUpdateReason::kFocus:
    case DocumentUpdateReason::kForm:
    case DocumentUpdateReason::kInput:
    case DocumentUpdateReason::kInspector:
    case DocumentUpdateReason::kPrinting:
    case DocumentUpdateReason::kSelection:
    case DocumentUpdateReason::kSpatialNavigation:
    case DocumentUpdateReason::kTapHighlight:
      sub_metric = kUserDrivenDocumentUpdate;
      break;

    case DocumentUpdateReason::kAccessibility:
    case DocumentUpdateReason::kBaseColor:
    case DocumentUpdateReason::kDisplayLock:
    case DocumentUpdateReason::kIntersectionObservation:
    case DocumentUpdateReason::kOverlay:
    case DocumentUpdateReason::kPagePopup:
    case DocumentUpdateReason::kSizeChange:
    case DocumentUpdateReason::kSpellCheck:
      sub_metric = kServiceDocumentUpdate;
      break;

    case DocumentUpdateReason::kCanvas:
    case DocumentUpdateReason::kPlugin:
    case DocumentUpdateReason::kSVGImage:
      sub_metric = kContentDocumentUpdate;
      break;

    case DocumentUpdateReason::kScroll:
      sub_metric = kScrollDocumentUpdate;
      break;

    case DocumentUpdateReason::kHitTest:
      sub_metric = kHitTestDocumentUpdate;
      break;

    case DocumentUpdateReason::kJavaScript:
      sub_metric = kJavascriptDocumentUpdate;
      break;

    // Do not report main frame because we have it already from
    // in_main_frame_update_ above.
    case DocumentUpdateReason::kBeginMainFrame:
    // No metrics from testing.
    case DocumentUpdateReason::kTest:
    // Don't report if we don't know why.
    case DocumentUpdateReason::kUnknown:
      break;
  }

  if (sub_metric != kCount) {
    auto& sub_record =
        absolute_metric_records_[static_cast<size_t>(sub_metric)];
    sub_record.interval_duration += duration;
    if (in_main_frame_update_)
      sub_record.main_frame_duration += duration;
    if (is_pre_fcp)
      sub_record.pre_fcp_aggregate += duration;
    if (should_report_uma_this_frame) {
      if (is_pre_fcp)
        sub_record.pre_fcp_uma_counter->CountMicroseconds(duration);
      else
        sub_record.post_fcp_uma_counter->CountMicroseconds(duration);
    }
  }
}

void LocalFrameUkmAggregator::RecordImplCompositorSample(
    base::TimeTicks requested,
    base::TimeTicks started,
    base::TimeTicks completed) {
  // Record the time spent waiting for the commit based on requested
  // (which came from ProxyImpl::BeginMainFrame) and started as reported by
  // the impl thread. If started is zero, no time was spent
  // processing. This can only happen if the commit was aborted because there
  // was no change and we did not wait for the impl thread at all. Attribute
  // all time to the compositor commit so as to not imply that wait time was
  // consumed.
  if (started == base::TimeTicks()) {
    RecordSample(kImplCompositorCommit, requested, completed);
  } else {
    RecordSample(kWaitForCommit, requested, started);
    RecordSample(kImplCompositorCommit, started, completed);
  }
}

void LocalFrameUkmAggregator::RecordEndOfFrameMetrics(
    base::TimeTicks start,
    base::TimeTicks end,
    cc::ActiveFrameSequenceTrackers trackers) {
  const base::TimeDelta duration = end - start;
  const bool have_valid_metrics =
      // Any of the early outs in LocalFrameView::UpdateLifecyclePhases() will
      // mean we are not in a main frame update. Recording is triggered higher
      // in the stack, so we cannot know to avoid calling this method.
      in_main_frame_update_ &&
      // In tests it's possible to reach here with zero duration.
      (duration > base::TimeDelta());

  in_main_frame_update_ = false;
  if (!have_valid_metrics) {
    // Reset for the next frame to start the next recording period with
    // clear counters, even when we did not record anything this frame.
    ResetAllMetrics();
    return;
  }

  bool report_as_pre_fcp = (fcp_state_ != kHavePassedFCP);
  bool report_fcp_metrics = (fcp_state_ == kThisFrameReachedFCP);

  // Record UMA
  if (report_as_pre_fcp)
    primary_metric_.pre_fcp_uma_counter->CountMicroseconds(duration);
  else
    primary_metric_.post_fcp_uma_counter->CountMicroseconds(duration);

  // Record primary time information
  primary_metric_.interval_duration = duration;
  if (report_as_pre_fcp)
    primary_metric_.pre_fcp_aggregate += duration;

  UpdateEventTimeAndUpdateSampleIfNeeded(trackers);

  // Report the FCP metrics, if necessary, after updating the sample so that
  // the sample has been recorded for the frame that produced FCP.
  if (report_fcp_metrics) {
    ReportPreFCPEvent();
    ReportUpdateTimeEvent();
    // Update the state to prevent future reporting.
    fcp_state_ = kHavePassedFCP;
  }

  // Reset for the next frame.
  ResetAllMetrics();
}

void LocalFrameUkmAggregator::UpdateEventTimeAndUpdateSampleIfNeeded(
    cc::ActiveFrameSequenceTrackers trackers) {
  // Update the frame count first, because it must include this frame
  frames_since_last_report_++;

  // Regardless of test requests, always capture the first frame.
  if (frames_since_last_report_ == 1) {
    UpdateSample(trackers);
    return;
  }

  // Exit if in testing and we do not want to update this frame
  if (next_frame_sample_control_for_test_ == kMustNotChooseNextFrame)
    return;

  // Update the sample with probability 1/frames_since_last_report_, or if
  // testing demand is.
  if ((next_frame_sample_control_for_test_ == kMustChooseNextFrame) ||
      base::RandDouble() < 1 / static_cast<double>(frames_since_last_report_)) {
    UpdateSample(trackers);
  }
}

void LocalFrameUkmAggregator::UpdateSample(
    cc::ActiveFrameSequenceTrackers trackers) {
  current_sample_.primary_metric_duration = primary_metric_.interval_duration;
  for (size_t i = 0; i < metrics_data().size(); ++i) {
    current_sample_.sub_metrics_durations[i] =
        absolute_metric_records_[i].interval_duration;
    current_sample_.sub_main_frame_durations[i] =
        absolute_metric_records_[i].main_frame_duration;
  }
  current_sample_.trackers = trackers;
}

void LocalFrameUkmAggregator::ReportPreFCPEvent() {
#define CASE_FOR_ID(name)                                                  \
  case k##name:                                                            \
    builder.Set##name(absolute_record.pre_fcp_aggregate.InMicroseconds()); \
    break

  ukm::builders::Blink_PageLoad builder(source_id_);
  builder.SetMainFrame(primary_metric_.pre_fcp_aggregate.InMicroseconds());
  primary_metric_.uma_aggregate_counter->CountMicroseconds(
      primary_metric_.pre_fcp_aggregate);
  for (size_t i = 0; i < metrics_data().size(); ++i) {
    auto& absolute_record = absolute_metric_records_[i];
    if (absolute_record.uma_aggregate_counter) {
      absolute_record.uma_aggregate_counter->CountMicroseconds(
          absolute_record.pre_fcp_aggregate);
    }

    switch (static_cast<MetricId>(i)) {
      CASE_FOR_ID(CompositingAssignments);
      CASE_FOR_ID(CompositingCommit);
      CASE_FOR_ID(CompositingInputs);
      CASE_FOR_ID(ImplCompositorCommit);
      CASE_FOR_ID(IntersectionObservation);
      CASE_FOR_ID(Paint);
      CASE_FOR_ID(PrePaint);
      CASE_FOR_ID(Style);
      CASE_FOR_ID(Layout);
      CASE_FOR_ID(ForcedStyleAndLayout);
      CASE_FOR_ID(HandleInputEvents);
      CASE_FOR_ID(Animate);
      CASE_FOR_ID(UpdateLayers);
      CASE_FOR_ID(WaitForCommit);
      CASE_FOR_ID(DisplayLockIntersectionObserver);
      CASE_FOR_ID(JavascriptIntersectionObserver);
      CASE_FOR_ID(LazyLoadIntersectionObserver);
      CASE_FOR_ID(MediaIntersectionObserver);
      CASE_FOR_ID(AnchorElementMetricsIntersectionObserver);
      CASE_FOR_ID(UpdateViewportIntersection);
      CASE_FOR_ID(UserDrivenDocumentUpdate);
      CASE_FOR_ID(ServiceDocumentUpdate);
      CASE_FOR_ID(ContentDocumentUpdate);
      CASE_FOR_ID(ScrollDocumentUpdate);
      CASE_FOR_ID(HitTestDocumentUpdate);
      CASE_FOR_ID(JavascriptDocumentUpdate);
      case kCount:
      case kMainFrame:
        NOTREACHED();
        break;
    }
  }
  builder.Record(recorder_);
#undef CASE_FOR_ID
}

void LocalFrameUkmAggregator::ReportUpdateTimeEvent() {
  // Don't report if we haven't generated any samples.
  if (!frames_since_last_report_)
    return;

#define CASE_FOR_ID(name, index)                                               \
  case k##name:                                                                \
    builder                                                                    \
        .Set##name(                                                            \
            current_sample_.sub_metrics_durations[index].InMicroseconds())     \
        .Set##name##BeginMainFrame(                                            \
            current_sample_.sub_main_frame_durations[index].InMicroseconds()); \
    break

  ukm::builders::Blink_UpdateTime builder(source_id_);
  builder.SetMainFrame(
      current_sample_.primary_metric_duration.InMicroseconds());
  builder.SetMainFrameIsBeforeFCP(fcp_state_ != kHavePassedFCP);
  builder.SetMainFrameReasons(current_sample_.trackers);
  for (size_t i = 0; i < metrics_data().size(); ++i) {
    switch (static_cast<MetricId>(i)) {
      CASE_FOR_ID(CompositingAssignments, i);
      CASE_FOR_ID(CompositingCommit, i);
      CASE_FOR_ID(CompositingInputs, i);
      CASE_FOR_ID(ImplCompositorCommit, i);
      CASE_FOR_ID(IntersectionObservation, i);
      CASE_FOR_ID(Paint, i);
      CASE_FOR_ID(PrePaint, i);
      CASE_FOR_ID(Style, i);
      CASE_FOR_ID(Layout, i);
      CASE_FOR_ID(ForcedStyleAndLayout, i);
      CASE_FOR_ID(HandleInputEvents, i);
      CASE_FOR_ID(Animate, i);
      CASE_FOR_ID(UpdateLayers, i);
      CASE_FOR_ID(WaitForCommit, i);
      CASE_FOR_ID(DisplayLockIntersectionObserver, i);
      CASE_FOR_ID(JavascriptIntersectionObserver, i);
      CASE_FOR_ID(LazyLoadIntersectionObserver, i);
      CASE_FOR_ID(MediaIntersectionObserver, i);
      CASE_FOR_ID(AnchorElementMetricsIntersectionObserver, i);
      CASE_FOR_ID(UpdateViewportIntersection, i);
      CASE_FOR_ID(UserDrivenDocumentUpdate, i);
      CASE_FOR_ID(ServiceDocumentUpdate, i);
      CASE_FOR_ID(ContentDocumentUpdate, i);
      CASE_FOR_ID(ScrollDocumentUpdate, i);
      CASE_FOR_ID(HitTestDocumentUpdate, i);
      CASE_FOR_ID(JavascriptDocumentUpdate, i);
      case kCount:
      case kMainFrame:
        NOTREACHED();
        break;
    }
  }
  builder.Record(recorder_);
#undef CASE_FOR_ID

  // Reset the frames since last report to ensure correct sampling.
  frames_since_last_report_ = 0;
}

void LocalFrameUkmAggregator::ResetAllMetrics() {
  primary_metric_.reset();
  for (auto& record : absolute_metric_records_)
    record.reset();
}

bool LocalFrameUkmAggregator::AllMetricsAreZero() {
  if (!primary_metric_.interval_duration.is_zero())
    return false;
  for (auto& record : absolute_metric_records_) {
    if (!record.interval_duration.is_zero()) {
      return false;
    }
    if (!record.main_frame_duration.is_zero()) {
      return false;
    }
  }
  return true;
}

void LocalFrameUkmAggregator::ChooseNextFrameForTest() {
  next_frame_sample_control_for_test_ = kMustChooseNextFrame;
}

void LocalFrameUkmAggregator::DoNotChooseNextFrameForTest() {
  next_frame_sample_control_for_test_ = kMustNotChooseNextFrame;
}

}  // namespace blink
