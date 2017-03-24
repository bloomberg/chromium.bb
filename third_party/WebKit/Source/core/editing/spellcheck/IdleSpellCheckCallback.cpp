// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/IdleSpellCheckCallback.h"

#include "core/dom/IdleRequestOptions.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/UndoStack.h"
#include "core/editing/commands/UndoStep.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/spellcheck/ColdModeSpellCheckRequester.h"
#include "core/editing/spellcheck/HotModeSpellCheckRequester.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

const int kColdModeTimerIntervalMS = 1000;
const int kConsecutiveColdModeTimerIntervalMS = 200;
const int kHotModeRequestTimeoutMS = 200;
const int kInvalidHandle = -1;
const int kDummyHandleForForcedInvocation = -2;
const double kForcedInvocationDeadlineSeconds = 10;

}  // namespace

IdleSpellCheckCallback::~IdleSpellCheckCallback() {}

DEFINE_TRACE(IdleSpellCheckCallback) {
  visitor->trace(m_frame);
  visitor->trace(m_coldModeRequester);
  IdleRequestCallback::trace(visitor);
  SynchronousMutationObserver::trace(visitor);
}

IdleSpellCheckCallback* IdleSpellCheckCallback::create(LocalFrame& frame) {
  return new IdleSpellCheckCallback(frame);
}

IdleSpellCheckCallback::IdleSpellCheckCallback(LocalFrame& frame)
    : m_state(State::kInactive),
      m_idleCallbackHandle(kInvalidHandle),
      m_frame(frame),
      m_lastProcessedUndoStepSequence(0),
      m_coldModeRequester(ColdModeSpellCheckRequester::create(frame)),
      m_coldModeTimer(TaskRunnerHelper::get(TaskType::UnspecedTimer, &frame),
                      this,
                      &IdleSpellCheckCallback::coldModeTimerFired) {}

SpellCheckRequester& IdleSpellCheckCallback::spellCheckRequester() const {
  return frame().spellChecker().spellCheckRequester();
}

bool IdleSpellCheckCallback::isSpellCheckingEnabled() const {
  return frame().spellChecker().isSpellCheckingEnabled();
}

void IdleSpellCheckCallback::deactivate() {
  m_state = State::kInactive;
  if (m_coldModeTimer.isActive())
    m_coldModeTimer.stop();
  if (m_idleCallbackHandle != kInvalidHandle)
    frame().document()->cancelIdleCallback(m_idleCallbackHandle);
  m_idleCallbackHandle = kInvalidHandle;
}

void IdleSpellCheckCallback::setNeedsInvocation() {
  if (!isSpellCheckingEnabled()) {
    deactivate();
    return;
  }

  if (m_state == State::kHotModeRequested)
    return;

  if (m_state == State::kColdModeTimerStarted) {
    DCHECK(m_coldModeTimer.isActive());
    m_coldModeTimer.stop();
  }

  if (m_state == State::kColdModeRequested) {
    frame().document()->cancelIdleCallback(m_idleCallbackHandle);
    m_idleCallbackHandle = kInvalidHandle;
  }

  IdleRequestOptions options;
  options.setTimeout(kHotModeRequestTimeoutMS);
  m_idleCallbackHandle = frame().document()->requestIdleCallback(this, options);
  m_state = State::kHotModeRequested;
}

void IdleSpellCheckCallback::setNeedsColdModeInvocation() {
  if (!isSpellCheckingEnabled()) {
    deactivate();
    return;
  }

  if (m_state != State::kInactive && m_state != State::kInHotModeInvocation &&
      m_state != State::kInColdModeInvocation)
    return;

  DCHECK(!m_coldModeTimer.isActive());
  int intervalMS = m_state == State::kInColdModeInvocation
                       ? kConsecutiveColdModeTimerIntervalMS
                       : kColdModeTimerIntervalMS;
  m_coldModeTimer.startOneShot(intervalMS / 1000.0, BLINK_FROM_HERE);
  m_state = State::kColdModeTimerStarted;
}

void IdleSpellCheckCallback::coldModeTimerFired(TimerBase*) {
  DCHECK_EQ(State::kColdModeTimerStarted, m_state);

  if (!isSpellCheckingEnabled()) {
    deactivate();
    return;
  }

  m_idleCallbackHandle =
      frame().document()->requestIdleCallback(this, IdleRequestOptions());
  m_state = State::kColdModeRequested;
}

void IdleSpellCheckCallback::hotModeInvocation(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "IdleSpellCheckCallback::hotModeInvocation");

  // TODO(xiaochengh): Figure out if this has any performance impact.
  frame().document()->updateStyleAndLayout();

  HotModeSpellCheckRequester requester(spellCheckRequester());

  requester.checkSpellingAt(frame().selection().selectionInDOMTree().extent());

  const uint64_t watermark = m_lastProcessedUndoStepSequence;
  for (const UndoStep* step : frame().editor().undoStack().undoSteps()) {
    if (step->sequenceNumber() <= watermark)
      break;
    m_lastProcessedUndoStepSequence =
        std::max(step->sequenceNumber(), m_lastProcessedUndoStepSequence);
    if (deadline->timeRemaining() == 0)
      break;
    requester.checkSpellingAt(step->endingSelection().extent());
  }
}

void IdleSpellCheckCallback::handleEvent(IdleDeadline* deadline) {
  DCHECK(RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled());
  DCHECK(frame().document());
  DCHECK(frame().document()->isActive());
  DCHECK_NE(m_idleCallbackHandle, kInvalidHandle);
  m_idleCallbackHandle = kInvalidHandle;

  if (!isSpellCheckingEnabled()) {
    deactivate();
    return;
  }

  if (m_state == State::kHotModeRequested) {
    m_state = State::kInHotModeInvocation;
    hotModeInvocation(deadline);
    setNeedsColdModeInvocation();
  } else if (m_state == State::kColdModeRequested) {
    m_state = State::kInColdModeInvocation;
    m_coldModeRequester->invoke(deadline);
    if (m_coldModeRequester->fullDocumentChecked())
      m_state = State::kInactive;
    else
      setNeedsColdModeInvocation();
  } else {
    NOTREACHED();
  }
}

void IdleSpellCheckCallback::documentAttached(Document* document) {
  setNeedsColdModeInvocation();
  setContext(document);
}

void IdleSpellCheckCallback::contextDestroyed(Document*) {
  deactivate();
}

void IdleSpellCheckCallback::forceInvocationForTesting() {
  if (!isSpellCheckingEnabled())
    return;

  IdleDeadline* deadline = IdleDeadline::create(
      kForcedInvocationDeadlineSeconds + monotonicallyIncreasingTime(),
      IdleDeadline::CallbackType::CalledWhenIdle);

  switch (m_state) {
    case State::kColdModeTimerStarted:
      m_coldModeTimer.stop();
      m_state = State::kColdModeRequested;
      m_idleCallbackHandle = kDummyHandleForForcedInvocation;
      handleEvent(deadline);
      break;
    case State::kHotModeRequested:
    case State::kColdModeRequested:
      frame().document()->cancelIdleCallback(m_idleCallbackHandle);
      handleEvent(deadline);
      break;
    case State::kInactive:
    case State::kInHotModeInvocation:
    case State::kInColdModeInvocation:
      NOTREACHED();
  }
}

void IdleSpellCheckCallback::skipColdModeTimerForTesting() {
  DCHECK(m_coldModeTimer.isActive());
  m_coldModeTimer.stop();
  coldModeTimerFired(&m_coldModeTimer);
}

void IdleSpellCheckCallback::setNeedsMoreColdModeInvocationForTesting() {
  m_coldModeRequester->setNeedsMoreInvocationForTesting();
}

}  // namespace blink
