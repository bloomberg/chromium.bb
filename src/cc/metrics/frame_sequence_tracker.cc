// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/gfx/presentation_feedback.h"

// This macro is used with DCHECK to provide addition debug info.
#if DCHECK_IS_ON()
#define TRACKER_TRACE_STREAM frame_sequence_trace_
#define TRACKER_DCHECK_MSG                                      \
  " in " << GetFrameSequenceTrackerTypeName(this->type())       \
         << " tracker: " << frame_sequence_trace_.str() << " (" \
         << frame_sequence_trace_.str().size() << ")";
#else
#define TRACKER_TRACE_STREAM EAT_STREAM_PARAMETERS
#define TRACKER_DCHECK_MSG ""
#endif

namespace cc {

// In the |TRACKER_TRACE_STREAM|, we mod the numbers such as frame sequence
// number, or frame token, such that the debug string is not too long.
constexpr int kDebugStrMod = 1000;

const char* FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
    FrameSequenceTrackerType type) {
  switch (type) {
    case FrameSequenceTrackerType::kCompositorAnimation:
      return "CompositorAnimation";
    case FrameSequenceTrackerType::kMainThreadAnimation:
      return "MainThreadAnimation";
    case FrameSequenceTrackerType::kPinchZoom:
      return "PinchZoom";
    case FrameSequenceTrackerType::kRAF:
      return "RAF";
    case FrameSequenceTrackerType::kTouchScroll:
      return "TouchScroll";
    case FrameSequenceTrackerType::kUniversal:
      return "Universal";
    case FrameSequenceTrackerType::kVideo:
      return "Video";
    case FrameSequenceTrackerType::kWheelScroll:
      return "WheelScroll";
    case FrameSequenceTrackerType::kScrollbarScroll:
      return "ScrollbarScroll";
    case FrameSequenceTrackerType::kCustom:
      return "Custom";
    case FrameSequenceTrackerType::kMaxType:
      return "";
  }
}

FrameSequenceTracker::FrameSequenceTracker(
    FrameSequenceTrackerType type,
    ThroughputUkmReporter* throughput_ukm_reporter)
    : custom_sequence_id_(-1),
      metrics_(std::make_unique<FrameSequenceMetrics>(type,
                                                      throughput_ukm_reporter)),
      trace_data_(metrics_.get()) {
  DCHECK_LT(type, FrameSequenceTrackerType::kMaxType);
  DCHECK(type != FrameSequenceTrackerType::kCustom);
}

FrameSequenceTracker::FrameSequenceTracker(
    int custom_sequence_id,
    FrameSequenceMetrics::CustomReporter custom_reporter)
    : custom_sequence_id_(custom_sequence_id),
      metrics_(std::make_unique<FrameSequenceMetrics>(
          FrameSequenceTrackerType::kCustom,
          /*ukm_reporter=*/nullptr)),
      trace_data_(metrics_.get()) {
  DCHECK_GT(custom_sequence_id_, 0);
  metrics_->SetCustomReporter(std::move(custom_reporter));
}

FrameSequenceTracker::~FrameSequenceTracker() = default;

void FrameSequenceTracker::ScheduleTerminate() {
  // If the last frame has ended and there is no frame awaiting presentation,
  // then it is ready to terminate.
  if (!is_inside_frame_ && last_submitted_frame_ == 0)
    termination_status_ = TerminationStatus::kReadyForTermination;
  else
    termination_status_ = TerminationStatus::kScheduledForTermination;
}

void FrameSequenceTracker::ReportBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "b(" << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  DCHECK(!is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = true;
#if DCHECK_IS_ON()
  if (args.type == viz::BeginFrameArgs::NORMAL)
    impl_frames_.insert(args.frame_id);
#endif

  DCHECK_EQ(last_started_impl_sequence_, 0u) << TRACKER_DCHECK_MSG;
  last_started_impl_sequence_ = args.frame_id.sequence_number;
  if (reset_all_state_) {
    begin_impl_frame_data_ = {};
    begin_main_frame_data_ = {};
    reset_all_state_ = false;
  }

  DCHECK(!frame_had_no_compositor_damage_) << TRACKER_DCHECK_MSG;
  DCHECK(!compositor_frame_submitted_) << TRACKER_DCHECK_MSG;

  UpdateTrackedFrameData(&begin_impl_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number);
  impl_throughput().frames_expected +=
      begin_impl_frame_data_.previous_sequence_delta;
#if DCHECK_IS_ON()
  ++impl_throughput().frames_received;
#endif

  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = args.frame_time;
}

void FrameSequenceTracker::ReportBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "B("
                       << begin_main_frame_data_.previous_sequence %
                              kDebugStrMod
                       << "," << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  if (first_received_main_sequence_ &&
      first_received_main_sequence_ > args.frame_id.sequence_number) {
    return;
  }

  if (!first_received_main_sequence_ &&
      ShouldIgnoreSequence(args.frame_id.sequence_number)) {
    return;
  }

#if DCHECK_IS_ON()
  if (args.type == viz::BeginFrameArgs::NORMAL) {
    DCHECK(impl_frames_.contains(args.frame_id)) << TRACKER_DCHECK_MSG;
  }
#endif

  DCHECK_EQ(awaiting_main_response_sequence_, 0u) << TRACKER_DCHECK_MSG;
  last_processed_main_sequence_latency_ = 0;
  awaiting_main_response_sequence_ = args.frame_id.sequence_number;

  UpdateTrackedFrameData(&begin_main_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number);
  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ <= last_no_main_damage_sequence_) {
    first_received_main_sequence_ = args.frame_id.sequence_number;
  }
  main_throughput().frames_expected +=
      begin_main_frame_data_.previous_sequence_delta;
  previous_begin_main_sequence_ = current_begin_main_sequence_;
  current_begin_main_sequence_ = args.frame_id.sequence_number;
}

void FrameSequenceTracker::ReportMainFrameProcessed(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "E(" << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  const bool previous_main_frame_submitted_or_no_damage =
      previous_begin_main_sequence_ != 0 &&
      (last_submitted_main_sequence_ == previous_begin_main_sequence_ ||
       last_no_main_damage_sequence_ == previous_begin_main_sequence_);
  if (last_processed_main_sequence_ != 0 &&
      !had_impl_frame_submitted_between_commits_ &&
      !previous_main_frame_submitted_or_no_damage) {
    DCHECK_GE(main_throughput().frames_expected,
              begin_main_frame_data_.previous_sequence_delta)
        << TRACKER_DCHECK_MSG;
    main_throughput().frames_expected -=
        begin_main_frame_data_.previous_sequence_delta;
    last_no_main_damage_sequence_ = previous_begin_main_sequence_;
  }
  had_impl_frame_submitted_between_commits_ = false;

  if (first_received_main_sequence_ &&
      args.frame_id.sequence_number >= first_received_main_sequence_) {
    if (awaiting_main_response_sequence_) {
      DCHECK_EQ(awaiting_main_response_sequence_, args.frame_id.sequence_number)
          << TRACKER_DCHECK_MSG;
    }
    DCHECK_EQ(last_processed_main_sequence_latency_, 0u) << TRACKER_DCHECK_MSG;
    last_processed_main_sequence_ = args.frame_id.sequence_number;
    last_processed_main_sequence_latency_ =
        std::max(last_started_impl_sequence_, last_processed_impl_sequence_) -
        args.frame_id.sequence_number;
    awaiting_main_response_sequence_ = 0;
  }
}

void FrameSequenceTracker::ReportSubmitFrame(
    uint32_t frame_token,
    bool has_missing_content,
    const viz::BeginFrameAck& ack,
    const viz::BeginFrameArgs& origin_args) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);

  // TODO(crbug.com/1072482): find a proper way to terminate a tracker.
  // Right now, we define a magical number |frames_to_terminate_tracker| = 3,
  // which means that if this frame_token is more than 3 frames compared with
  // the last submitted frame, then we assume that the last submitted frame is
  // not going to be presented, and thus terminate this tracker.
  const uint32_t frames_to_terminate_tracker = 3;
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      last_submitted_frame_ != 0 &&
      viz::FrameTokenGT(frame_token,
                        last_submitted_frame_ + frames_to_terminate_tracker)) {
    termination_status_ = TerminationStatus::kReadyForTermination;
    return;
  }

  if (ShouldIgnoreBeginFrameSource(ack.frame_id.source_id) ||
      ShouldIgnoreSequence(ack.frame_id.sequence_number)) {
    ignored_frame_tokens_.insert(frame_token);
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  DCHECK_LT(impl_throughput().frames_processed,
            impl_throughput().frames_received)
      << TRACKER_DCHECK_MSG;
  ++impl_throughput().frames_processed;
#endif

  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
  if (first_submitted_frame_ == 0)
    first_submitted_frame_ = frame_token;
  last_submitted_frame_ = frame_token;
  compositor_frame_submitted_ = true;

  TRACKER_TRACE_STREAM << "s(" << frame_token % kDebugStrMod << ")";
  had_impl_frame_submitted_between_commits_ = true;

  const bool main_changes_after_sequence_started =
      first_received_main_sequence_ &&
      origin_args.frame_id.sequence_number >= first_received_main_sequence_;
  const bool main_changes_include_new_changes =
      last_submitted_main_sequence_ == 0 ||
      origin_args.frame_id.sequence_number > last_submitted_main_sequence_;
  const bool main_change_had_no_damage =
      last_no_main_damage_sequence_ != 0 &&
      origin_args.frame_id.sequence_number == last_no_main_damage_sequence_;
  const bool origin_args_is_valid = origin_args.frame_id.sequence_number <=
                                    begin_main_frame_data_.previous_sequence;

  if (!ShouldIgnoreBeginFrameSource(origin_args.frame_id.source_id) &&
      origin_args_is_valid) {
    if (main_changes_after_sequence_started &&
        main_changes_include_new_changes && !main_change_had_no_damage) {
      submitted_frame_had_new_main_content_ = true;
      TRACKER_TRACE_STREAM << "S("
                           << origin_args.frame_id.sequence_number %
                                  kDebugStrMod
                           << ")";

      last_submitted_main_sequence_ = origin_args.frame_id.sequence_number;
      main_frames_.push_back(frame_token);
      DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
          << TRACKER_DCHECK_MSG;
    } else {
      // If we have sent a BeginMainFrame which hasn't yet been submitted, or
      // confirmed that it has no damage (previous_sequence is set to 0), then
      // we are currently expecting a main frame.
      const bool expecting_main = begin_main_frame_data_.previous_sequence >
                                  last_submitted_main_sequence_;
      if (expecting_main)
        expecting_main_when_submit_impl_.push_back(frame_token);
    }
  }

  if (has_missing_content) {
    checkerboarding_.frames.push_back(frame_token);
  }
}

void FrameSequenceTracker::ReportFrameEnd(
    const viz::BeginFrameArgs& args,
    const viz::BeginFrameArgs& main_args) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "e(" << args.frame_id.sequence_number % kDebugStrMod
                       << ","
                       << main_args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  bool should_ignore_sequence =
      ShouldIgnoreSequence(args.frame_id.sequence_number);
  if (reset_all_state_) {
    begin_impl_frame_data_ = {};
    begin_main_frame_data_ = {};
    reset_all_state_ = false;
  }

  if (should_ignore_sequence) {
    is_inside_frame_ = false;
    return;
  }

  if (compositor_frame_submitted_ && submitted_frame_had_new_main_content_ &&
      last_processed_main_sequence_latency_) {
    // If a compositor frame was submitted with new content from the
    // main-thread, then make sure the latency gets accounted for.
    main_throughput().frames_expected += last_processed_main_sequence_latency_;
  }

  // It is possible that the compositor claims there was no damage from the
  // compositor, but before the frame ends, it submits a compositor frame (e.g.
  // with some damage from main). In such cases, the compositor is still
  // responsible for processing the update, and therefore the 'no damage' claim
  // is ignored.
  if (frame_had_no_compositor_damage_ && !compositor_frame_submitted_) {
    DCHECK_GT(impl_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
    DCHECK_GT(impl_throughput().frames_expected,
              impl_throughput().frames_produced)
        << TRACKER_DCHECK_MSG;
    --impl_throughput().frames_expected;
#if DCHECK_IS_ON()
    ++impl_throughput().frames_processed;
    // If these two are the same, it means that each impl frame is either
    // no-damage or submitted. That's expected, so we don't need those in the
    // output of DCHECK.
    if (impl_throughput().frames_processed == impl_throughput().frames_received)
      ignored_trace_char_count_ = frame_sequence_trace_.str().size();
    else
      NOTREACHED() << TRACKER_DCHECK_MSG;
#endif
    begin_impl_frame_data_.previous_sequence = 0;
  }
  // last_submitted_frame_ == 0 means the last impl frame has been presented.
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      last_submitted_frame_ == 0)
    termination_status_ = TerminationStatus::kReadyForTermination;

  frame_had_no_compositor_damage_ = false;
  compositor_frame_submitted_ = false;
  submitted_frame_had_new_main_content_ = false;
  last_processed_main_sequence_latency_ = 0;

  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = false;

  DCHECK_EQ(last_started_impl_sequence_, last_processed_impl_sequence_)
      << TRACKER_DCHECK_MSG;
  last_started_impl_sequence_ = 0;
}

void FrameSequenceTracker::ReportFramePresented(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  // TODO(xidachen): We should early exit if |last_submitted_frame_| = 0, as it
  // means that we are presenting the same frame_token again.
  const bool submitted_frame_since_last_presentation = !!last_submitted_frame_;
  // !viz::FrameTokenGT(a, b) is equivalent to b >= a.
  const bool frame_token_acks_last_frame =
      !viz::FrameTokenGT(last_submitted_frame_, frame_token);

  // Even if the presentation timestamp is null, we set last_submitted_frame_ to
  // 0 such that the tracker can be terminated.
  if (last_submitted_frame_ && frame_token_acks_last_frame)
    last_submitted_frame_ = 0;
  // Update termination status if this is scheduled for termination, and it is
  // not waiting for any frames, or it has received the presentation-feedback
  // for the latest frame it is tracking.
  //
  // We should always wait for an impl frame to end, that is, ReportFrameEnd.
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      last_submitted_frame_ == 0 && !is_inside_frame_) {
    termination_status_ = TerminationStatus::kReadyForTermination;
  }

  if (first_submitted_frame_ == 0 ||
      viz::FrameTokenGT(first_submitted_frame_, frame_token)) {
    // We are getting presentation feedback for frames that were submitted
    // before this sequence started. So ignore these.
    return;
  }

  TRACKER_TRACE_STREAM << "P(" << frame_token % kDebugStrMod << ")";

  base::EraseIf(ignored_frame_tokens_, [frame_token](const uint32_t& token) {
    return viz::FrameTokenGT(frame_token, token);
  });
  if (ignored_frame_tokens_.contains(frame_token))
    return;

  uint32_t impl_frames_produced = 0;
  uint32_t main_frames_produced = 0;
  trace_data_.Advance(feedback.timestamp);

  const bool was_presented = !feedback.timestamp.is_null();
  if (was_presented && submitted_frame_since_last_presentation) {
    DCHECK_LT(impl_throughput().frames_produced,
              impl_throughput().frames_expected)
        << TRACKER_DCHECK_MSG;
    ++impl_throughput().frames_produced;
    ++impl_frames_produced;
  }

  if (was_presented) {
    // This presentation includes the visual update from all main frame tokens
    // <= |frame_token|.
    const unsigned size_before_erase = main_frames_.size();
    while (!main_frames_.empty() &&
           !viz::FrameTokenGT(main_frames_.front(), frame_token)) {
      main_frames_.pop_front();
    }
    if (main_frames_.size() < size_before_erase) {
      DCHECK_LT(main_throughput().frames_produced,
                main_throughput().frames_expected)
          << TRACKER_DCHECK_MSG;
      ++main_throughput().frames_produced;
      ++main_frames_produced;
    }

    if (impl_frames_produced > 0) {
      // If there is no main frame presented, then we need to see whether or not
      // we are expecting main frames to be presented or not.
      if (main_frames_produced == 0) {
        // Only need to check the first element in the deque because the
        // elements are in order.
        bool expecting_main_frames =
            !expecting_main_when_submit_impl_.empty() &&
            !viz::FrameTokenGT(expecting_main_when_submit_impl_[0],
                               frame_token);
        if (expecting_main_frames) {
          // We are expecting a main frame to be processed, the main frame
          // should either report no-damage or be submitted to GPU. Since we
          // don't know which case it would be, we accumulate the number of impl
          // frames produced so that we can apply that to aggregated throughput
          // if the main frame reports no-damage later on.
          impl_frames_produced_while_expecting_main_ += impl_frames_produced;
        } else {
          // TODO(https://crbug.com/1066455): Determine why this DCHECK is
          // causing PageLoadMetricsBrowserTests to flake, and re-enable.
          // DCHECK_EQ(impl_frames_produced_while_expecting_main_, 0u)
          //    << TRACKER_DCHECK_MSG;
          aggregated_throughput().frames_produced += impl_frames_produced;
          impl_frames_produced_while_expecting_main_ = 0;
        }
      } else {
        aggregated_throughput().frames_produced += main_frames_produced;
        impl_frames_produced_while_expecting_main_ = 0;
        while (!expecting_main_when_submit_impl_.empty() &&
               !viz::FrameTokenGT(expecting_main_when_submit_impl_.front(),
                                  frame_token)) {
          expecting_main_when_submit_impl_.pop_front();
        }
      }
    }

    if (checkerboarding_.last_frame_had_checkerboarding) {
      DCHECK(!checkerboarding_.last_frame_timestamp.is_null())
          << TRACKER_DCHECK_MSG;
      DCHECK(!feedback.timestamp.is_null()) << TRACKER_DCHECK_MSG;

      // |feedback.timestamp| is the timestamp when the latest frame was
      // presented. |checkerboarding_.last_frame_timestamp| is the timestamp
      // when the previous frame (which had checkerboarding) was presented. Use
      // |feedback.interval| to compute the number of vsyncs that have passed
      // between the two frames (since that is how many times the user saw that
      // checkerboarded frame).
      base::TimeDelta difference =
          feedback.timestamp - checkerboarding_.last_frame_timestamp;
      const auto& interval = feedback.interval.is_zero()
                                 ? viz::BeginFrameArgs::DefaultInterval()
                                 : feedback.interval;
      DCHECK(!interval.is_zero()) << TRACKER_DCHECK_MSG;
      constexpr base::TimeDelta kEpsilon = base::TimeDelta::FromMilliseconds(1);
      int64_t frames = (difference + kEpsilon) / interval;
      metrics_->add_checkerboarded_frames(frames);
    }

    const bool frame_had_checkerboarding =
        base::Contains(checkerboarding_.frames, frame_token);
    checkerboarding_.last_frame_had_checkerboarding = frame_had_checkerboarding;
    checkerboarding_.last_frame_timestamp = feedback.timestamp;
  }

  while (!checkerboarding_.frames.empty() &&
         !viz::FrameTokenGT(checkerboarding_.frames.front(), frame_token)) {
    checkerboarding_.frames.pop_front();
  }
}

void FrameSequenceTracker::ReportImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  DCHECK_NE(termination_status_, TerminationStatus::kReadyForTermination);

  if (ShouldIgnoreBeginFrameSource(ack.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "n(" << ack.frame_id.sequence_number % kDebugStrMod
                       << ")";

  // This tracker would be scheduled to terminate, and this frame doesn't belong
  // to that tracker.
  if (ShouldIgnoreSequence(ack.frame_id.sequence_number))
    return;

  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
  // If there is no damage for this frame (and no frame is submitted), then the
  // impl-sequence needs to be reset. However, this should be done after the
  // processing the frame is complete (i.e. in ReportFrameEnd()), so that other
  // notifications (e.g. 'no main damage' etc.) can be handled correctly.
  DCHECK_EQ(begin_impl_frame_data_.previous_sequence,
            ack.frame_id.sequence_number);
  frame_had_no_compositor_damage_ = true;
}

void FrameSequenceTracker::ReportMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "N("
                       << begin_main_frame_data_.previous_sequence %
                              kDebugStrMod
                       << "," << args.frame_id.sequence_number % kDebugStrMod
                       << ")";

  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ > args.frame_id.sequence_number) {
    return;
  }

  if (last_no_main_damage_sequence_ == args.frame_id.sequence_number)
    return;

  // It is possible for |awaiting_main_response_sequence_| to be zero here if a
  // commit had already happened before (e.g. B(x)E(x)N(x)). So check that case
  // here.
  if (awaiting_main_response_sequence_) {
    DCHECK_EQ(awaiting_main_response_sequence_, args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  } else {
    DCHECK_EQ(last_processed_main_sequence_, args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  }
  awaiting_main_response_sequence_ = 0;

  DCHECK_GT(main_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
  DCHECK_GT(main_throughput().frames_expected,
            main_throughput().frames_produced)
      << TRACKER_DCHECK_MSG;
  last_no_main_damage_sequence_ = args.frame_id.sequence_number;
  --main_throughput().frames_expected;
  DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
      << TRACKER_DCHECK_MSG;

  // Could be 0 if there were a pause frame production.
  if (begin_main_frame_data_.previous_sequence != 0) {
    DCHECK_EQ(begin_main_frame_data_.previous_sequence,
              args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  }
  begin_main_frame_data_.previous_sequence = 0;

  aggregated_throughput().frames_produced +=
      impl_frames_produced_while_expecting_main_;
  impl_frames_produced_while_expecting_main_ = 0;
  expecting_main_when_submit_impl_.clear();
}

void FrameSequenceTracker::PauseFrameProduction() {
  // The states need to be reset, so that the tracker ignores the vsyncs until
  // the next received begin-frame. However, defer doing that until the frame
  // ends (or a new frame starts), so that in case a frame is in-progress,
  // subsequent notifications for that frame can be handled correctly.
  TRACKER_TRACE_STREAM << 'R';
  reset_all_state_ = true;
}

void FrameSequenceTracker::UpdateTrackedFrameData(TrackedFrameData* frame_data,
                                                  uint64_t source_id,
                                                  uint64_t sequence_number) {
  if (frame_data->previous_sequence &&
      frame_data->previous_source == source_id) {
    uint32_t current_latency = sequence_number - frame_data->previous_sequence;
    DCHECK_GT(current_latency, 0u) << TRACKER_DCHECK_MSG;
    frame_data->previous_sequence_delta = current_latency;
  } else {
    frame_data->previous_sequence_delta = 1;
  }
  frame_data->previous_source = source_id;
  frame_data->previous_sequence = sequence_number;
}

bool FrameSequenceTracker::ShouldIgnoreBeginFrameSource(
    uint64_t source_id) const {
  if (begin_impl_frame_data_.previous_source == 0)
    return source_id == viz::BeginFrameArgs::kManualSourceId;
  return source_id != begin_impl_frame_data_.previous_source;
}

// This check handles two cases:
// 1. When there is a call to ReportBeginMainFrame, or ReportSubmitFrame, or
// ReportFramePresented, there must be a ReportBeginImplFrame for that sequence.
// Otherwise, the begin_impl_frame_data_.previous_sequence would be 0.
// 2. A tracker is scheduled to terminate, then any new request to handle a new
// impl frame whose sequence_number > begin_impl_frame_data_.previous_sequence
// should be ignored.
// Note that sequence_number < begin_impl_frame_data_.previous_sequence cannot
// happen.
bool FrameSequenceTracker::ShouldIgnoreSequence(
    uint64_t sequence_number) const {
  return sequence_number != begin_impl_frame_data_.previous_sequence;
}

std::unique_ptr<base::trace_event::TracedValue>
FrameSequenceMetrics::ThroughputData::ToTracedValue(
    const ThroughputData& impl,
    const ThroughputData& main) {
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->SetInteger("impl-frames-produced", impl.frames_produced);
  dict->SetInteger("impl-frames-expected", impl.frames_expected);
  dict->SetInteger("main-frames-produced", main.frames_produced);
  dict->SetInteger("main-frames-expected", main.frames_expected);
  return dict;
}

bool FrameSequenceTracker::ShouldReportMetricsNow(
    const viz::BeginFrameArgs& args) const {
  return metrics_->HasEnoughDataForReporting() &&
         !first_frame_timestamp_.is_null() &&
         args.frame_time - first_frame_timestamp_ >= time_delta_to_report_;
}

std::unique_ptr<FrameSequenceMetrics> FrameSequenceTracker::TakeMetrics() {
#if DCHECK_IS_ON()
  DCHECK_EQ(impl_throughput().frames_received,
            impl_throughput().frames_processed)
      << frame_sequence_trace_.str().substr(ignored_trace_char_count_);
#endif
  return std::move(metrics_);
}

FrameSequenceTracker::CheckerboardingData::CheckerboardingData() = default;
FrameSequenceTracker::CheckerboardingData::~CheckerboardingData() = default;

FrameSequenceTracker::TraceData::TraceData(const void* id) : trace_id(id) {}
void FrameSequenceTracker::TraceData::Advance(base::TimeTicks new_timestamp) {
  // Use different names, because otherwise the trace-viewer shows the slices in
  // the same color, and that makes it difficult to tell the traces apart from
  // each other.
  const char* trace_names[] = {"Frame", "Frame ", "Frame   "};
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "cc,benchmark", trace_names[++this->frame_count % 3],
      TRACE_ID_LOCAL(this->trace_id), this->last_timestamp);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "cc,benchmark", trace_names[this->frame_count % 3],
      TRACE_ID_LOCAL(this->trace_id), new_timestamp);
  this->last_timestamp = new_timestamp;
}

}  // namespace cc
