// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/compositor_timing_history.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.pbzero.h"

namespace cc {

namespace {
// This is a fudge factor we subtract from the deadline to account
// for message latency and kernel scheduling variability.
const base::TimeDelta kDeadlineFudgeFactor =
    base::TimeDelta::FromMicroseconds(1000);
}  // namespace

Scheduler::Scheduler(
    SchedulerClient* client,
    const SchedulerSettings& settings,
    int layer_tree_host_id,
    base::SingleThreadTaskRunner* task_runner,
    std::unique_ptr<CompositorTimingHistory> compositor_timing_history)
    : settings_(settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      task_runner_(task_runner),
      compositor_timing_history_(std::move(compositor_timing_history)),
      begin_impl_frame_tracker_(FROM_HERE),
      state_machine_(settings) {
  TRACE_EVENT1("cc", "Scheduler::Scheduler", "settings", settings_.AsValue());
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeeded());

  // We want to handle animate_only BeginFrames.
  wants_animate_only_begin_frames_ = true;

  ProcessScheduledActions();
}

Scheduler::~Scheduler() {
  SetBeginFrameSource(nullptr);
}

void Scheduler::Stop() {
  stopped_ = true;
}

void Scheduler::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
               "Scheduler::SetNeedsImplSideInvalidation",
               "needs_first_draw_on_activation",
               needs_first_draw_on_activation);
  state_machine_.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  ProcessScheduledActions();
}

base::TimeTicks Scheduler::Now() const {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.now"),
               "Scheduler::Now", "now", now);
  return now;
}

void Scheduler::SetVisible(bool visible) {
  state_machine_.SetVisible(visible);
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::SetCanDraw(bool can_draw) {
  state_machine_.SetCanDraw(can_draw);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToActivate() {
  if (state_machine_.NotifyReadyToActivate())
    compositor_timing_history_->ReadyToActivate();

  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToDraw() {
  // Future work might still needed for crbug.com/352894.
  state_machine_.NotifyReadyToDraw();
  ProcessScheduledActions();
}

void Scheduler::SetBeginFrameSource(viz::BeginFrameSource* source) {
  if (source == begin_frame_source_)
    return;
  if (begin_frame_source_ && observing_begin_frame_source_)
    begin_frame_source_->RemoveObserver(this);
  begin_frame_source_ = source;
  if (!begin_frame_source_)
    return;
  if (observing_begin_frame_source_)
    begin_frame_source_->AddObserver(this);
}

void Scheduler::NotifyAnimationWorkletStateChange(AnimationWorkletState state,
                                                  TreeType tree) {
  state_machine_.NotifyAnimationWorkletStateChange(state, tree);
  ProcessScheduledActions();
}

void Scheduler::NotifyPaintWorkletStateChange(PaintWorkletState state) {
  state_machine_.NotifyPaintWorkletStateChange(state);
  ProcessScheduledActions();
}

void Scheduler::SetNeedsBeginMainFrame() {
  state_machine_.SetNeedsBeginMainFrame();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsOneBeginImplFrame() {
  state_machine_.SetNeedsOneBeginImplFrame();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsRedraw() {
  state_machine_.SetNeedsRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsPrepareTiles() {
  DCHECK(!IsInsideAction(SchedulerStateMachine::Action::PREPARE_TILES));
  state_machine_.SetNeedsPrepareTiles();
  ProcessScheduledActions();
}

void Scheduler::DidSubmitCompositorFrame(uint32_t frame_token,
                                         EventMetricsSet events_metrics) {
  compositor_timing_history_->DidSubmitCompositorFrame(
      frame_token, begin_main_frame_args_.frame_id,
      last_activate_origin_frame_args_.frame_id, std::move(events_metrics));
  state_machine_.DidSubmitCompositorFrame();

  // There is no need to call ProcessScheduledActions here because
  // submitting a CompositorFrame should not trigger any new actions.
  if (!inside_process_scheduled_actions_) {
    DCHECK_EQ(state_machine_.NextAction(), SchedulerStateMachine::Action::NONE);
  }
}

void Scheduler::DidReceiveCompositorFrameAck() {
  DCHECK_GT(state_machine_.pending_submit_frames(), 0);
  compositor_timing_history_->DidReceiveCompositorFrameAck();
  state_machine_.DidReceiveCompositorFrameAck();
  ProcessScheduledActions();
}

void Scheduler::SetTreePrioritiesAndScrollState(
    TreePriority tree_priority,
    ScrollHandlerState scroll_handler_state) {
  compositor_timing_history_->SetTreePriority(tree_priority);
  state_machine_.SetTreePrioritiesAndScrollState(tree_priority,
                                                 scroll_handler_state);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToCommit(
    std::unique_ptr<BeginMainFrameMetrics> details) {
  TRACE_EVENT0("cc", "Scheduler::NotifyReadyToCommit");
  compositor_timing_history_->NotifyReadyToCommit(std::move(details));
  state_machine_.NotifyReadyToCommit();
  ProcessScheduledActions();
}

void Scheduler::DidCommit() {
  compositor_timing_history_->DidCommit();
}

void Scheduler::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  TRACE_EVENT1("cc", "Scheduler::BeginMainFrameAborted", "reason",
               CommitEarlyOutReasonToString(reason));
  compositor_timing_history_->BeginMainFrameAborted(
      last_dispatched_begin_main_frame_args_.frame_id);
  state_machine_.BeginMainFrameAborted(reason);
  ProcessScheduledActions();
}

void Scheduler::WillPrepareTiles() {
  compositor_timing_history_->WillPrepareTiles();
}

void Scheduler::DidPrepareTiles() {
  compositor_timing_history_->DidPrepareTiles();
  state_machine_.DidPrepareTiles();
}

void Scheduler::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  compositor_timing_history_->DidPresentCompositorFrame(frame_token, details);
}

void Scheduler::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseLayerTreeFrameSink");
  state_machine_.DidLoseLayerTreeFrameSink();
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeLayerTreeFrameSink");
  DCHECK(!observing_begin_frame_source_);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
  state_machine_.DidCreateAndInitializeLayerTreeFrameSink();
  compositor_timing_history_->DidCreateAndInitializeLayerTreeFrameSink();
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::NotifyBeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  TRACE_EVENT0("cc", "Scheduler::NotifyBeginMainFrameStarted");
  compositor_timing_history_->BeginMainFrameStarted(main_thread_start_time);
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return begin_impl_frame_tracker_.Current().frame_time;
}

void Scheduler::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  TRACE_EVENT1("cc", "Scheduler::BeginMainFrameNotExpectedUntil",
               "remaining_time", (time - Now()).InMillisecondsF());

  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  client_->ScheduledActionBeginMainFrameNotExpectedUntil(time);
}

void Scheduler::BeginMainFrameNotExpectedSoon() {
  TRACE_EVENT0("cc", "Scheduler::BeginMainFrameNotExpectedSoon");

  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  client_->SendBeginMainFrameNotExpectedSoon();
}

void Scheduler::StartOrStopBeginFrames() {
  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BeginImplFrameState::IDLE) {
    return;
  }

  bool needs_begin_frames = state_machine_.BeginFrameNeeded();
  if (needs_begin_frames == observing_begin_frame_source_)
    return;

  if (needs_begin_frames) {
    observing_begin_frame_source_ = true;
    if (begin_frame_source_)
      begin_frame_source_->AddObserver(this);
    devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_, true);
  } else {
    observing_begin_frame_source_ = false;
    if (begin_frame_source_)
      begin_frame_source_->RemoveObserver(this);
    // We're going idle so drop pending begin frame.
    CancelPendingBeginFrameTask();
    compositor_timing_history_->BeginImplFrameNotExpectedSoon();
    devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_,
                                                     false);
    client_->WillNotReceiveBeginFrame();
  }
}

void Scheduler::CancelPendingBeginFrameTask() {
  if (pending_begin_frame_args_.IsValid()) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    SendDidNotProduceFrame(pending_begin_frame_args_,
                           FrameSkippedReason::kNoDamage);
    // Make pending begin frame invalid so that we don't accidentally use it.
    pending_begin_frame_args_ = viz::BeginFrameArgs();
  }
  pending_begin_frame_task_.Cancel();
}

void Scheduler::PostPendingBeginFrameTask() {
  bool is_idle = state_machine_.begin_impl_frame_state() ==
                 SchedulerStateMachine::BeginImplFrameState::IDLE;
  bool needs_begin_frames = state_machine_.BeginFrameNeeded();
  // We only post one pending begin frame task at a time, but we update the args
  // whenever we get a new begin frame.
  bool has_pending_begin_frame_args = pending_begin_frame_args_.IsValid();
  bool has_no_pending_begin_frame_task =
      pending_begin_frame_task_.IsCancelled();

  if (is_idle && needs_begin_frames && has_pending_begin_frame_args &&
      has_no_pending_begin_frame_task) {
    pending_begin_frame_task_.Reset(base::BindOnce(
        &Scheduler::HandlePendingBeginFrame, base::Unretained(this)));
    task_runner_->PostTask(FROM_HERE, pending_begin_frame_task_.callback());
  }
}

void Scheduler::OnBeginFrameSourcePausedChanged(bool paused) {
  if (state_machine_.begin_frame_source_paused() == paused)
    return;
  TRACE_EVENT_INSTANT1("cc", "Scheduler::SetBeginFrameSourcePaused",
                       TRACE_EVENT_SCOPE_THREAD, "paused", paused);
  state_machine_.SetBeginFrameSourcePaused(paused);
  ProcessScheduledActions();
}

// BeginFrame is the mechanism that tells us that now is a good time to start
// making a frame. Usually this means that user input for the frame is complete.
// If the scheduler is busy, we queue the BeginFrame to be handled later as
// a BeginRetroFrame.
bool Scheduler::OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginFrame", "args", args.AsValue());

  // If the begin frame interval is different than last frame and bigger than
  // zero then let |client_| know about the new interval for animations. In
  // theory the interval should always be bigger than zero but the value is
  // provided by APIs outside our control.
  if (args.interval != last_frame_interval_ &&
      args.interval > base::TimeDelta()) {
    last_frame_interval_ = args.interval;
    client_->FrameIntervalUpdated(last_frame_interval_);
  }

  // Drop the BeginFrame if we don't need one.
  if (!state_machine_.BeginFrameNeeded()) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    // Since we don't use the BeginFrame, we may later receive the same
    // BeginFrame again. Thus, we can't confirm it at this point, even though we
    // don't have any updates right now.
    SendDidNotProduceFrame(args, FrameSkippedReason::kNoDamage);
    return false;
  }

  // Trace this begin frame time through the Chrome stack
  TRACE_EVENT_FLOW_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
      "viz::BeginFrameArgs", args.frame_time.since_origin().InMicroseconds());

  if (settings_.using_synchronous_renderer_compositor) {
    BeginImplFrameSynchronous(args);
    return true;
  }

  bool inside_previous_begin_frame =
      state_machine_.begin_impl_frame_state() ==
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME;

  if (inside_process_scheduled_actions_ || inside_previous_begin_frame ||
      pending_begin_frame_args_.IsValid()) {
    // The BFS can send a begin frame while scheduler is processing previous
    // frame, or a MISSED begin frame inside the ProcessScheduledActions loop
    // when AddObserver is called. The BFS (e.g. mojo) may queue up many begin
    // frame calls, but we only want to process the last one. Saving the args,
    // and posting a task achieves that.
    if (pending_begin_frame_args_.IsValid()) {
      TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                           TRACE_EVENT_SCOPE_THREAD);
      SendDidNotProduceFrame(pending_begin_frame_args_,
                             FrameSkippedReason::kRecoverLatency);
    }
    pending_begin_frame_args_ = args;
    // ProcessScheduledActions() will post the previous frame's deadline if it
    // hasn't run yet, or post the begin frame task if the previous frame's
    // deadline has already run. If we're already inside
    // ProcessScheduledActions() this call will be a nop and the above will
    // happen at end of the top most call to ProcessScheduledActions().
    ProcessScheduledActions();
  } else {
    // This starts the begin frame immediately, and puts us in the
    // INSIDE_BEGIN_FRAME state, so if the message loop calls a bunch of
    // BeginFrames immediately after this call, they will be posted as a single
    // task, and all but the last BeginFrame will be dropped.
    BeginImplFrameWithDeadline(args);
  }
  return true;
}

void Scheduler::SetVideoNeedsBeginFrames(bool video_needs_begin_frames) {
  state_machine_.SetVideoNeedsBeginFrames(video_needs_begin_frames);
  ProcessScheduledActions();
}

void Scheduler::OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                            bool skip_draw) {
  DCHECK(settings_.using_synchronous_renderer_compositor);
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BeginImplFrameState::IDLE);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());

  state_machine_.SetResourcelessSoftwareDraw(resourceless_software_draw);
  state_machine_.SetSkipDraw(skip_draw);
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();

  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();
  state_machine_.SetResourcelessSoftwareDraw(false);
}

// This is separate from BeginImplFrameWithDeadline() because we only want at
// most one outstanding task even if |pending_begin_frame_args_| changes.
void Scheduler::HandlePendingBeginFrame() {
  DCHECK(pending_begin_frame_args_.IsValid());
  viz::BeginFrameArgs args = pending_begin_frame_args_;
  pending_begin_frame_args_ = viz::BeginFrameArgs();
  pending_begin_frame_task_.Cancel();

  BeginImplFrameWithDeadline(args);
}

void Scheduler::BeginImplFrameWithDeadline(const viz::BeginFrameArgs& args) {
  DCHECK(pending_begin_frame_task_.IsCancelled());
  DCHECK(!pending_begin_frame_args_.IsValid());

  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BeginImplFrameState::IDLE);

  bool main_thread_is_in_high_latency_mode =
      state_machine_.main_thread_missed_last_deadline();
  TRACE_EVENT2("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue(), "main_thread_missed_last_deadline",
               main_thread_is_in_high_latency_mode);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "MainThreadLatency", main_thread_is_in_high_latency_mode);

  base::TimeTicks now = Now();
  // Discard missed begin frames if they are too late. In full-pipe mode, we
  // ignore BeginFrame deadlines.
  if (!settings_.wait_for_all_pipeline_stages_before_draw &&
      args.type == viz::BeginFrameArgs::MISSED && args.deadline < now) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::MissedBeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    skipped_last_frame_missed_exceeded_deadline_ = true;
    SendDidNotProduceFrame(args, FrameSkippedReason::kRecoverLatency);
    return;
  }
  skipped_last_frame_missed_exceeded_deadline_ = false;

  viz::BeginFrameArgs adjusted_args = args;
  adjusted_args.deadline -= compositor_timing_history_->DrawDurationEstimate();
  adjusted_args.deadline -= kDeadlineFudgeFactor;

  // TODO(khushalsagar): We need to consider the deadline fudge factor here to
  // match the deadline used in BeginImplFrameDeadlineMode::REGULAR mode
  // (used in the case where the impl thread needs to redraw). In the case where
  // main_frame_to_active is fast, we should consider using
  // BeginImplFrameDeadlineMode::LATE instead to avoid putting the main
  // thread in high latency mode. See crbug.com/753146.
  base::TimeDelta bmf_to_activate_threshold =
      adjusted_args.interval -
      compositor_timing_history_->DrawDurationEstimate() - kDeadlineFudgeFactor;

  // An estimate of time from starting the main frame on the main thread to when
  // the resulting pending tree is activated. Note that this excludes the
  // durations where progress is blocked due to back pressure in the pipeline
  // (ready to commit to commit, ready to activate to activate, etc.)
  base::TimeDelta bmf_start_to_activate =
      compositor_timing_history_
          ->BeginMainFrameStartToReadyToCommitDurationEstimate() +
      compositor_timing_history_->CommitDurationEstimate() +
      compositor_timing_history_->CommitToReadyToActivateDurationEstimate() +
      compositor_timing_history_->ActivateDurationEstimate();

  base::TimeDelta bmf_to_activate_estimate_critical =
      bmf_start_to_activate +
      compositor_timing_history_->BeginMainFrameQueueDurationCriticalEstimate();
  state_machine_.SetCriticalBeginMainFrameToActivateIsFast(
      bmf_to_activate_estimate_critical < bmf_to_activate_threshold);

  // Update the BeginMainFrame args now that we know whether the main
  // thread will be on the critical path or not.
  begin_main_frame_args_ = adjusted_args;
  begin_main_frame_args_.on_critical_path = !ImplLatencyTakesPriority();

  // If we expect the main thread to respond within this frame, defer the
  // invalidation to merge it with the incoming main frame. Even if the response
  // is delayed such that the raster can not be completed within this frame's
  // draw, its better to delay the invalidation than blocking the pipeline with
  // an extra pending tree update to be flushed.
  base::TimeDelta time_since_main_frame_sent;
  if (compositor_timing_history_->begin_main_frame_sent_time() !=
      base::TimeTicks()) {
    time_since_main_frame_sent =
        now - compositor_timing_history_->begin_main_frame_sent_time();
  }
  base::TimeDelta bmf_sent_to_ready_to_commit_estimate =
      compositor_timing_history_
          ->BeginMainFrameStartToReadyToCommitDurationEstimate();
  if (begin_main_frame_args_.on_critical_path) {
    bmf_sent_to_ready_to_commit_estimate +=
        compositor_timing_history_
            ->BeginMainFrameQueueDurationCriticalEstimate();
  } else {
    bmf_sent_to_ready_to_commit_estimate +=
        compositor_timing_history_
            ->BeginMainFrameQueueDurationNotCriticalEstimate();
  }

  // We defer the invalidation if we expect the main thread to respond within
  // this frame, and our prediction in the last frame was correct. That
  // is, if we predicted the main thread to be fast and it fails to respond
  // before the deadline in the previous frame, we don't defer the invalidation
  // in the next frame.
  const bool main_thread_response_expected_before_deadline =
      bmf_sent_to_ready_to_commit_estimate - time_since_main_frame_sent <
      bmf_to_activate_threshold;
  const bool previous_invalidation_maybe_skipped_for_main_frame =
      state_machine_.should_defer_invalidation_for_fast_main_frame() &&
      state_machine_.main_thread_failed_to_respond_last_deadline();
  state_machine_.set_should_defer_invalidation_for_fast_main_frame(
      main_thread_response_expected_before_deadline &&
      !previous_invalidation_maybe_skipped_for_main_frame);

  base::TimeDelta bmf_to_activate_estimate = bmf_to_activate_estimate_critical;
  if (!begin_main_frame_args_.on_critical_path) {
    bmf_to_activate_estimate =
        bmf_start_to_activate +
        compositor_timing_history_
            ->BeginMainFrameQueueDurationNotCriticalEstimate();
  }
  bool can_activate_before_deadline =
      CanBeginMainFrameAndActivateBeforeDeadline(adjusted_args,
                                                 bmf_to_activate_estimate, now);

  if (ShouldRecoverMainLatency(adjusted_args, can_activate_before_deadline)) {
    TRACE_EVENT_INSTANT0("cc", "SkipBeginMainFrameToReduceLatency",
                         TRACE_EVENT_SCOPE_THREAD);
    state_machine_.SetSkipNextBeginMainFrameToReduceLatency(true);
  } else if (ShouldRecoverImplLatency(adjusted_args,
                                      can_activate_before_deadline)) {
    TRACE_EVENT_INSTANT0("cc", "SkipBeginImplFrameToReduceLatency",
                         TRACE_EVENT_SCOPE_THREAD);
    skipped_last_frame_to_reduce_latency_ = true;
    SendDidNotProduceFrame(args, FrameSkippedReason::kRecoverLatency);
    return;
  }

  skipped_last_frame_to_reduce_latency_ = false;

  BeginImplFrame(adjusted_args, now);
}

void Scheduler::BeginImplFrameSynchronous(const viz::BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue());
  // The main thread currently can't commit before we draw with the
  // synchronous compositor, so never consider the BeginMainFrame fast.
  state_machine_.SetCriticalBeginMainFrameToActivateIsFast(false);
  begin_main_frame_args_ = args;
  begin_main_frame_args_.on_critical_path = !ImplLatencyTakesPriority();

  BeginImplFrame(args, Now());
  compositor_timing_history_->WillFinishImplFrame(state_machine_.needs_redraw(),
                                                  args.frame_id);
  FinishImplFrame();
}

void Scheduler::FinishImplFrame() {
  state_machine_.OnBeginImplFrameIdle();

  // Send ack before calling ProcessScheduledActions() because it might send an
  // ack for any pending begin frame if we are going idle after this. This
  // ensures that the acks are sent in order.
  if (!state_machine_.did_submit_in_last_frame()) {
    SendDidNotProduceFrame(begin_impl_frame_tracker_.Current(),
                           state_machine_.draw_succeeded_in_last_frame()
                               ? FrameSkippedReason::kNoDamage
                               : FrameSkippedReason::kWaitingOnMain);
  }

  begin_impl_frame_tracker_.Finish();

  ProcessScheduledActions();
  DCHECK(!inside_scheduled_action_);
  {
    base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
    client_->DidFinishImplFrame();
  }

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);
}

void Scheduler::SendDidNotProduceFrame(const viz::BeginFrameArgs& args,
                                       FrameSkippedReason reason) {
  if (last_begin_frame_ack_.frame_id == args.frame_id)
    return;
  last_begin_frame_ack_ = viz::BeginFrameAck(args, false /* has_damage */);
  client_->DidNotProduceFrame(last_begin_frame_ack_, reason);
  compositor_timing_history_->DidNotProduceFrame(args.frame_id, reason);
}

// BeginImplFrame starts a compositor frame that will wait up until a deadline
// for a BeginMainFrame+activation to complete before it times out and draws
// any asynchronous animation and scroll/pinch updates.
void Scheduler::BeginImplFrame(const viz::BeginFrameArgs& args,
                               base::TimeTicks now) {
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BeginImplFrameState::IDLE);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
  DCHECK(state_machine_.HasInitializedLayerTreeFrameSink());

  {
    DCHECK(!inside_scheduled_action_);
    base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);

    begin_impl_frame_tracker_.Start(args);
    state_machine_.OnBeginImplFrame(args.frame_id, args.animate_only);
    devtools_instrumentation::DidBeginFrame(layer_tree_host_id_);
    compositor_timing_history_->WillBeginImplFrame(
        args, state_machine_.NewActiveTreeLikely(), now);
    bool has_damage =
        client_->WillBeginImplFrame(begin_impl_frame_tracker_.Current());

    if (!has_damage)
      state_machine_.AbortDraw();
  }

  ProcessScheduledActions();
}

void Scheduler::ScheduleBeginImplFrameDeadline() {
  using DeadlineMode = SchedulerStateMachine::BeginImplFrameDeadlineMode;
  deadline_mode_ = state_machine_.CurrentBeginImplFrameDeadlineMode();

  base::TimeTicks new_deadline;
  switch (deadline_mode_) {
    case DeadlineMode::NONE:
      // NONE is returned when deadlines aren't used (synchronous compositor),
      // or when outside a begin frame. In either case deadline task shouldn't
      // be posted or should be cancelled already.
      DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
      return;
    case DeadlineMode::BLOCKED: {
      // TODO(sunnyps): Posting the deadline for pending begin frame is required
      // for browser compositor (commit_to_active_tree) to make progress in some
      // cases. Change browser compositor deadline to LATE in state machine to
      // fix this.
      //
      // TODO(sunnyps): Full pipeline mode should always go from blocking
      // deadline to triggering deadline immediately, but DCHECKing for this
      // causes layout test failures.
      bool has_pending_begin_frame = pending_begin_frame_args_.IsValid();
      if (has_pending_begin_frame) {
        new_deadline = base::TimeTicks();
        break;
      } else {
        begin_impl_frame_deadline_task_.Cancel();
        return;
      }
    }
    case DeadlineMode::LATE: {
      // We are waiting for a commit without needing active tree draw or we
      // have nothing to do.
      new_deadline = begin_impl_frame_tracker_.Current().frame_time +
                     begin_impl_frame_tracker_.Current().interval;
      // Send early DidNotProduceFrame if we don't expect to produce a frame
      // soon so that display scheduler doesn't wait unnecessarily.
      // Note: This will only send one DidNotProduceFrame ack per begin frame.
      if (!state_machine_.NewActiveTreeLikely()) {
        SendDidNotProduceFrame(begin_impl_frame_tracker_.Current(),
                               FrameSkippedReason::kNoDamage);
      }
      break;
    }
    case DeadlineMode::REGULAR:
      // We are animating the active tree but we're also waiting for commit.
      new_deadline = begin_impl_frame_tracker_.Current().deadline;
      break;
    case DeadlineMode::IMMEDIATE:
      // Avoid using Now() for immediate deadlines because it's expensive, and
      // this method is called in every ProcessScheduledActions() call. Using
      // base::TimeTicks() achieves the same result.
      new_deadline = base::TimeTicks();
      break;
  }

  // Post deadline task only if we didn't have one already or something caused
  // us to change the deadline.
  bool has_no_deadline_task = begin_impl_frame_deadline_task_.IsCancelled();
  if (has_no_deadline_task || new_deadline != deadline_) {
    TRACE_EVENT2("cc", "Scheduler::ScheduleBeginImplFrameDeadline",
                 "new deadline", new_deadline, "deadline mode",
                 SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
                     deadline_mode_));
    deadline_ = new_deadline;
    deadline_scheduled_at_ = Now();

    begin_impl_frame_deadline_task_.Reset(base::BindOnce(
        &Scheduler::OnBeginImplFrameDeadline, base::Unretained(this)));

    base::TimeDelta delay =
        std::max(deadline_ - deadline_scheduled_at_, base::TimeDelta());
    task_runner_->PostDelayedTask(
        FROM_HERE, begin_impl_frame_deadline_task_.callback(), delay);
  }
}

void Scheduler::OnBeginImplFrameDeadline() {
  TRACE_EVENT0("cc,benchmark", "Scheduler::OnBeginImplFrameDeadline");
  begin_impl_frame_deadline_task_.Cancel();
  // We split the deadline actions up into two phases so the state machine
  // has a chance to trigger actions that should occur durring and after
  // the deadline separately. For example:
  // * Sending the BeginMainFrame will not occur after the deadline in
  //     order to wait for more user-input before starting the next commit.
  // * Creating a new OuputSurface will not occur during the deadline in
  //     order to allow the state machine to "settle" first.
  compositor_timing_history_->WillFinishImplFrame(
      state_machine_.needs_redraw(), begin_main_frame_args_.frame_id);
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();
  FinishImplFrame();
}

void Scheduler::DrawIfPossible() {
  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  bool drawing_with_new_active_tree =
      state_machine_.active_tree_needs_first_draw() &&
      !state_machine_.previous_pending_tree_was_impl_side();
  compositor_timing_history_->WillDraw();
  state_machine_.WillDraw();
  DrawResult result = client_->ScheduledActionDrawIfPossible();
  state_machine_.DidDraw(result);
  compositor_timing_history_->DidDraw(
      drawing_with_new_active_tree,
      client_->CompositedAnimationsCount(),
      client_->MainThreadAnimationsCount(), client_->CurrentFrameHadRAF(),
      client_->NextFrameHasPendingRAF(),
      client_->HasCustomPropertyAnimations());
}

void Scheduler::DrawForced() {
  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  bool drawing_with_new_active_tree =
      state_machine_.active_tree_needs_first_draw() &&
      !state_machine_.previous_pending_tree_was_impl_side();
  compositor_timing_history_->WillDraw();
  state_machine_.WillDraw();
  DrawResult result = client_->ScheduledActionDrawForced();
  state_machine_.DidDraw(result);
  compositor_timing_history_->DidDraw(
      drawing_with_new_active_tree,
      client_->CompositedAnimationsCount(),
      client_->MainThreadAnimationsCount(), client_->CurrentFrameHadRAF(),
      client_->NextFrameHasPendingRAF(),
      client_->HasCustomPropertyAnimations());
}

void Scheduler::SetDeferBeginMainFrame(bool defer_begin_main_frame) {
  TRACE_EVENT1("cc", "Scheduler::SetDeferBeginMainFrame",
               "defer_begin_main_frame", defer_begin_main_frame);
  state_machine_.SetDeferBeginMainFrame(defer_begin_main_frame);
  ProcessScheduledActions();
}

void Scheduler::SetMainThreadWantsBeginMainFrameNotExpected(bool new_state) {
  state_machine_.SetMainThreadWantsBeginMainFrameNotExpectedMessages(new_state);
  ProcessScheduledActions();
}

void Scheduler::ProcessScheduledActions() {
  // Do not perform actions during compositor shutdown.
  if (stopped_)
    return;

  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_ || inside_scheduled_action_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action;
  do {
    action = state_machine_.NextAction();
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                "SchedulerStateMachine", [this](perfetto::EventContext ctx) {
                  this->AsProtozeroInto(ctx.event()->set_cc_scheduler_state());
                });
    base::AutoReset<SchedulerStateMachine::Action> mark_inside_action(
        &inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::Action::NONE:
        break;
      case SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME:
        compositor_timing_history_->WillBeginMainFrame(begin_main_frame_args_);
        state_machine_.WillSendBeginMainFrame();
        client_->ScheduledActionSendBeginMainFrame(begin_main_frame_args_);
        last_dispatched_begin_main_frame_args_ = begin_main_frame_args_;
        break;
      case SchedulerStateMachine::Action::
          NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL:
        state_machine_.WillNotifyBeginMainFrameNotExpectedUntil();
        BeginMainFrameNotExpectedUntil(begin_main_frame_args_.frame_time +
                                       begin_main_frame_args_.interval);
        break;
      case SchedulerStateMachine::Action::
          NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON:
        state_machine_.WillNotifyBeginMainFrameNotExpectedSoon();
        BeginMainFrameNotExpectedSoon();
        break;
      case SchedulerStateMachine::Action::COMMIT: {
        bool commit_has_no_updates = false;
        state_machine_.WillCommit(commit_has_no_updates);
        compositor_timing_history_->WillCommit();
        client_->ScheduledActionCommit();
        last_commit_origin_frame_args_ = last_dispatched_begin_main_frame_args_;
        break;
      }
      case SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE:
        compositor_timing_history_->WillActivate();
        state_machine_.WillActivate();
        client_->ScheduledActionActivateSyncTree();
        compositor_timing_history_->DidActivate();
        last_activate_origin_frame_args_ = last_commit_origin_frame_args_;
        break;
      case SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION:
        state_machine_.WillPerformImplSideInvalidation();
        compositor_timing_history_->WillInvalidateOnImplSide();
        client_->ScheduledActionPerformImplSideInvalidation();
        break;
      case SchedulerStateMachine::Action::DRAW_IF_POSSIBLE:
        DrawIfPossible();
        break;
      case SchedulerStateMachine::Action::DRAW_FORCED:
        DrawForced();
        break;
      case SchedulerStateMachine::Action::DRAW_ABORT: {
        // No action is actually performed, but this allows the state machine to
        // drain the pipeline without actually drawing.
        state_machine_.AbortDraw();
        break;
      }
      case SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION:
        state_machine_.WillBeginLayerTreeFrameSinkCreation();
        client_->ScheduledActionBeginLayerTreeFrameSinkCreation();
        break;
      case SchedulerStateMachine::Action::PREPARE_TILES:
        state_machine_.WillPrepareTiles();
        client_->ScheduledActionPrepareTiles();
        break;
      case SchedulerStateMachine::Action::INVALIDATE_LAYER_TREE_FRAME_SINK: {
        state_machine_.WillInvalidateLayerTreeFrameSink();
        client_->ScheduledActionInvalidateLayerTreeFrameSink(
            state_machine_.RedrawPending());
        break;
      }
    }
  } while (action != SchedulerStateMachine::Action::NONE);

  ScheduleBeginImplFrameDeadline();

  PostPendingBeginFrameTask();
  StartOrStopBeginFrames();
}

void Scheduler::AsProtozeroInto(
    perfetto::protos::pbzero::ChromeCompositorSchedulerState* state) const {
  base::TimeTicks now = Now();

  state_machine_.AsProtozeroInto(state->set_state_machine());

  state->set_observing_begin_frame_source(observing_begin_frame_source_);
  state->set_begin_impl_frame_deadline_task(
      !begin_impl_frame_deadline_task_.IsCancelled());
  state->set_pending_begin_frame_task(!pending_begin_frame_task_.IsCancelled());
  state->set_skipped_last_frame_missed_exceeded_deadline(
      skipped_last_frame_missed_exceeded_deadline_);
  state->set_skipped_last_frame_to_reduce_latency(
      skipped_last_frame_to_reduce_latency_);
  state->set_inside_action(
      SchedulerStateMachine::ActionToProtozeroEnum(inside_action_));
  state->set_deadline_mode(
      SchedulerStateMachine::BeginImplFrameDeadlineModeToProtozeroEnum(
          deadline_mode_));

  state->set_deadline_us(deadline_.since_origin().InMicroseconds());
  state->set_deadline_scheduled_at_us(
      deadline_scheduled_at_.since_origin().InMicroseconds());

  state->set_now_us(Now().since_origin().InMicroseconds());
  state->set_now_to_deadline_delta_us((deadline_ - Now()).InMicroseconds());
  state->set_now_to_deadline_scheduled_at_delta_us(
      (deadline_scheduled_at_ - Now()).InMicroseconds());

  begin_impl_frame_tracker_.AsProtozeroInto(now,
                                            state->set_begin_impl_frame_args());

  BeginFrameObserverBase::AsProtozeroInto(
      state->set_begin_frame_observer_state());

  if (begin_frame_source_) {
    begin_frame_source_->AsProtozeroInto(state->set_begin_frame_source_state());
  }

  compositor_timing_history_->AsProtozeroInto(
      state->set_compositor_timing_history());
}

void Scheduler::UpdateCompositorTimingHistoryRecordingEnabled() {
  compositor_timing_history_->SetRecordingEnabled(
      state_machine_.HasInitializedLayerTreeFrameSink() &&
      state_machine_.visible());
}

bool Scheduler::ShouldRecoverMainLatency(
    const viz::BeginFrameArgs& args,
    bool can_activate_before_deadline) const {
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  if (!settings_.enable_main_latency_recovery)
    return false;

  // The main thread is in a low latency mode and there's no need to recover.
  if (!state_machine_.main_thread_missed_last_deadline())
    return false;

  // When prioritizing impl thread latency, we currently put the
  // main thread in a high latency mode. Don't try to fight it.
  if (state_machine_.ImplLatencyTakesPriority())
    return false;

  // Ensure that we have data from at least one frame before attempting latency
  // recovery. This prevents skipping of frames during loading where the main
  // thread is likely slow but we assume it to be fast since we have no history.
  static const int kMinNumberOfSamplesBeforeLatencyRecovery = 1;
  if (compositor_timing_history_
              ->begin_main_frame_start_to_ready_to_commit_sample_count() <
          kMinNumberOfSamplesBeforeLatencyRecovery ||
      compositor_timing_history_->commit_to_ready_to_activate_sample_count() <
          kMinNumberOfSamplesBeforeLatencyRecovery) {
    return false;
  }

  return can_activate_before_deadline;
}

bool Scheduler::ShouldRecoverImplLatency(
    const viz::BeginFrameArgs& args,
    bool can_activate_before_deadline) const {
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  if (!settings_.enable_impl_latency_recovery)
    return false;

  // Disable impl thread latency recovery when using the unthrottled
  // begin frame source since we will always get a BeginFrame before
  // the swap ack and our heuristics below will not work.
  if (begin_frame_source_ && !begin_frame_source_->IsThrottled())
    return false;

  // If we are swap throttled at the BeginFrame, that means the impl thread is
  // very likely in a high latency mode.
  bool impl_thread_is_likely_high_latency = state_machine_.IsDrawThrottled();
  if (!impl_thread_is_likely_high_latency)
    return false;

  // The deadline may be in the past if our draw time is too long.
  bool can_draw_before_deadline = args.frame_time < args.deadline;

  // When prioritizing impl thread latency, the deadline doesn't wait
  // for the main thread.
  if (state_machine_.ImplLatencyTakesPriority())
    return can_draw_before_deadline;

  // If we only have impl-side updates, the deadline doesn't wait for
  // the main thread.
  if (state_machine_.OnlyImplSideUpdatesExpected())
    return can_draw_before_deadline;

  // If we get here, we know the main thread is in a low-latency mode relative
  // to the impl thread. In this case, only try to also recover impl thread
  // latency if both the main and impl threads can run serially before the
  // deadline.
  return can_activate_before_deadline;
}

bool Scheduler::CanBeginMainFrameAndActivateBeforeDeadline(
    const viz::BeginFrameArgs& args,
    base::TimeDelta bmf_to_activate_estimate,
    base::TimeTicks now) const {
  // Check if the main thread computation and commit can be finished before the
  // impl thread's deadline.
  base::TimeTicks estimated_draw_time = now + bmf_to_activate_estimate;

  return estimated_draw_time < args.deadline;
}

bool Scheduler::IsBeginMainFrameSent() const {
  return state_machine_.begin_main_frame_state() ==
         SchedulerStateMachine::BeginMainFrameState::SENT;
}

viz::BeginFrameAck Scheduler::CurrentBeginFrameAckForActiveTree() const {
  return viz::BeginFrameAck(begin_main_frame_args_, true);
}

void Scheduler::ClearHistory() {
  // Ensure we reset decisions based on history from the previous navigation.
  state_machine_.SetSkipNextBeginMainFrameToReduceLatency(false);
  compositor_timing_history_->ClearHistory();
  ProcessScheduledActions();
}

}  // namespace cc
