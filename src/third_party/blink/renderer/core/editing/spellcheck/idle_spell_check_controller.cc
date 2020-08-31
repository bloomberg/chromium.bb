// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/commands/undo_step.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/cold_mode_spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/hot_mode_spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

namespace {

constexpr base::TimeDelta kColdModeTimerInterval =
    base::TimeDelta::FromMilliseconds(1000);
constexpr base::TimeDelta kConsecutiveColdModeTimerInterval =
    base::TimeDelta::FromMilliseconds(200);
const int kHotModeRequestTimeoutMS = 200;
const int kInvalidHandle = -1;
const int kDummyHandleForForcedInvocation = -2;
constexpr base::TimeDelta kIdleSpellcheckTestTimeout =
    base::TimeDelta::FromSeconds(10);

}  // namespace

class IdleSpellCheckController::IdleCallback final
    : public ScriptedIdleTaskController::IdleTask {
 public:
  explicit IdleCallback(IdleSpellCheckController* controller)
      : controller_(controller) {}

  void Trace(Visitor* visitor) final {
    visitor->Trace(controller_);
    ScriptedIdleTaskController::IdleTask::Trace(visitor);
  }

 private:
  void invoke(IdleDeadline* deadline) final { controller_->Invoke(deadline); }

  const Member<IdleSpellCheckController> controller_;

  DISALLOW_COPY_AND_ASSIGN(IdleCallback);
};

IdleSpellCheckController::~IdleSpellCheckController() = default;

void IdleSpellCheckController::Trace(Visitor* visitor) {
  visitor->Trace(cold_mode_requester_);
  visitor->Trace(spell_check_requeseter_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

IdleSpellCheckController::IdleSpellCheckController(
    LocalDOMWindow& window,
    SpellCheckRequester& requester)
    : ExecutionContextLifecycleObserver(&window),
      state_(State::kInactive),
      idle_callback_handle_(kInvalidHandle),
      last_processed_undo_step_sequence_(0),
      cold_mode_requester_(
          MakeGarbageCollected<ColdModeSpellCheckRequester>(window)),
      spell_check_requeseter_(requester) {}

LocalDOMWindow& IdleSpellCheckController::GetWindow() const {
  DCHECK(GetExecutionContext());
  return *To<LocalDOMWindow>(GetExecutionContext());
}

Document& IdleSpellCheckController::GetDocument() const {
  DCHECK(GetExecutionContext());
  return *GetWindow().document();
}

bool IdleSpellCheckController::IsSpellCheckingEnabled() const {
  if (!GetExecutionContext())
    return false;
  return GetWindow().GetSpellChecker().IsSpellCheckingEnabled();
}

void IdleSpellCheckController::DisposeIdleCallback() {
  if (idle_callback_handle_ != kInvalidHandle && GetExecutionContext())
    GetDocument().CancelIdleCallback(idle_callback_handle_);
  idle_callback_handle_ = kInvalidHandle;
}

void IdleSpellCheckController::Deactivate() {
  state_ = State::kInactive;
  if (cold_mode_timer_.IsActive())
    cold_mode_timer_.Cancel();
  cold_mode_requester_->ClearProgress();
  DisposeIdleCallback();
  spell_check_requeseter_->Deactivate();
}

void IdleSpellCheckController::SetNeedsInvocation() {
  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  if (state_ == State::kHotModeRequested)
    return;

  cold_mode_requester_->ClearProgress();

  if (state_ == State::kColdModeTimerStarted) {
    DCHECK(cold_mode_timer_.IsActive());
    cold_mode_timer_.Cancel();
  }

  if (state_ == State::kColdModeRequested)
    DisposeIdleCallback();

  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(kHotModeRequestTimeoutMS);
  idle_callback_handle_ = GetDocument().RequestIdleCallback(
      MakeGarbageCollected<IdleCallback>(this), options);
  state_ = State::kHotModeRequested;
}

void IdleSpellCheckController::SetNeedsColdModeInvocation() {
  DCHECK(IsSpellCheckingEnabled());
  if (state_ != State::kInactive && state_ != State::kInHotModeInvocation &&
      state_ != State::kInColdModeInvocation)
    return;

  DCHECK(!cold_mode_timer_.IsActive());
  base::TimeDelta interval = state_ == State::kInColdModeInvocation
                                 ? kConsecutiveColdModeTimerInterval
                                 : kColdModeTimerInterval;
  cold_mode_timer_ = PostDelayedCancellableTask(
      *GetWindow().GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
      WTF::Bind(&IdleSpellCheckController::ColdModeTimerFired,
                WrapPersistent(this)),
      interval);
  state_ = State::kColdModeTimerStarted;
}

void IdleSpellCheckController::ColdModeTimerFired() {
  DCHECK_EQ(State::kColdModeTimerStarted, state_);

  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  idle_callback_handle_ = GetDocument().RequestIdleCallback(
      MakeGarbageCollected<IdleCallback>(this), IdleRequestOptions::Create());
  state_ = State::kColdModeRequested;
}

void IdleSpellCheckController::HotModeInvocation(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "IdleSpellCheckController::hotModeInvocation");

  // TODO(xiaochengh): Figure out if this has any performance impact.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  HotModeSpellCheckRequester requester(*spell_check_requeseter_);

  requester.CheckSpellingAt(
      GetWindow().GetFrame()->Selection().GetSelectionInDOMTree().Extent());

  const uint64_t watermark = last_processed_undo_step_sequence_;
  for (const UndoStep* step :
       GetWindow().GetFrame()->GetEditor().GetUndoStack().UndoSteps()) {
    if (step->SequenceNumber() <= watermark)
      break;
    last_processed_undo_step_sequence_ =
        std::max(step->SequenceNumber(), last_processed_undo_step_sequence_);
    if (deadline->timeRemaining() == 0)
      break;
    // The ending selection stored in undo stack can be invalid, disconnected
    // or have been moved to another document, so we should check its validity
    // before using it.
    if (!step->EndingSelection().IsValidFor(GetDocument()))
      continue;
    requester.CheckSpellingAt(step->EndingSelection().Extent());
  }
}

void IdleSpellCheckController::Invoke(IdleDeadline* deadline) {
  DCHECK_NE(idle_callback_handle_, kInvalidHandle);
  idle_callback_handle_ = kInvalidHandle;

  if (!IsSpellCheckingEnabled()) {
    Deactivate();
    return;
  }

  if (state_ == State::kHotModeRequested) {
    state_ = State::kInHotModeInvocation;
    HotModeInvocation(deadline);
    SetNeedsColdModeInvocation();
  } else if (state_ == State::kColdModeRequested) {
    state_ = State::kInColdModeInvocation;
    cold_mode_requester_->Invoke(deadline);
    if (cold_mode_requester_->FullyChecked())
      state_ = State::kInactive;
    else
      SetNeedsColdModeInvocation();
  } else {
    NOTREACHED();
  }
}

void IdleSpellCheckController::ContextDestroyed() {
  Deactivate();
}

void IdleSpellCheckController::ForceInvocationForTesting() {
  if (!IsSpellCheckingEnabled())
    return;

  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks::Now() + kIdleSpellcheckTestTimeout,
      IdleDeadline::CallbackType::kCalledWhenIdle);

  switch (state_) {
    case State::kColdModeTimerStarted:
      cold_mode_timer_.Cancel();
      state_ = State::kColdModeRequested;
      idle_callback_handle_ = kDummyHandleForForcedInvocation;
      Invoke(deadline);
      break;
    case State::kHotModeRequested:
    case State::kColdModeRequested:
      GetDocument().CancelIdleCallback(idle_callback_handle_);
      Invoke(deadline);
      break;
    case State::kInactive:
    case State::kInHotModeInvocation:
    case State::kInColdModeInvocation:
      NOTREACHED();
  }
}

void IdleSpellCheckController::SkipColdModeTimerForTesting() {
  DCHECK(cold_mode_timer_.IsActive());
  cold_mode_timer_.Cancel();
  ColdModeTimerFired();
}

void IdleSpellCheckController::SetNeedsMoreColdModeInvocationForTesting() {
  cold_mode_requester_->SetNeedsMoreInvocationForTesting();
}

}  // namespace blink
