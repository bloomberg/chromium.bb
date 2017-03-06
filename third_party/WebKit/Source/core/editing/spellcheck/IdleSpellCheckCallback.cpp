// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/IdleSpellCheckCallback.h"

#include "core/dom/IdleRequestOptions.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/editing/commands/UndoStep.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutObject.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

const int kColdModeTimerIntervalMS = 1000;
const int kConsecutiveColdModeTimerIntervalMS = 200;
const int kRequestTimeoutMS = 200;
const int kInvalidHandle = -1;
const int kDummyHandleForForcedInvocation = -2;
const double kForcedInvocationDeadlineSeconds = 10;

}  // namespace

IdleSpellCheckCallback::~IdleSpellCheckCallback() {}

DEFINE_TRACE(IdleSpellCheckCallback) {
  visitor->trace(m_frame);
  IdleRequestCallback::trace(visitor);
  SynchronousMutationObserver::trace(visitor);
}

IdleSpellCheckCallback* IdleSpellCheckCallback::create(LocalFrame& frame) {
  return new IdleSpellCheckCallback(frame);
}

IdleSpellCheckCallback::IdleSpellCheckCallback(LocalFrame& frame)
    : m_state(State::kInactive),
      m_idleCallbackHandle(kInvalidHandle),
      m_needsMoreColdModeInvocationForTesting(false),
      m_frame(frame),
      m_coldModeTimer(TaskRunnerHelper::get(TaskType::UnspecedTimer, &frame),
                      this,
                      &IdleSpellCheckCallback::coldModeTimerFired) {}

SpellCheckRequester& IdleSpellCheckCallback::spellCheckRequester() const {
  return frame().spellChecker().spellCheckRequester();
}

bool IdleSpellCheckCallback::isSpellCheckingEnabled() const {
  return frame().spellChecker().isSpellCheckingEnabled();
}

void IdleSpellCheckCallback::requestInvocation() {
  DCHECK_EQ(m_idleCallbackHandle, kInvalidHandle);

  IdleRequestOptions options;
  options.setTimeout(kRequestTimeoutMS);
  m_idleCallbackHandle = frame().document()->requestIdleCallback(this, options);
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

  if (m_state != State::kColdModeRequested)
    requestInvocation();
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

  requestInvocation();
  m_state = State::kColdModeRequested;
}

void IdleSpellCheckCallback::hotModeInvocation(IdleDeadline* deadline) {
  // TODO(xiaochengh): Implementation.
}

void IdleSpellCheckCallback::coldModeInvocation(IdleDeadline* deadline) {
  // TODO(xiaochengh): Implementation.
}

bool IdleSpellCheckCallback::coldModeFinishesFullDocument() const {
  if (m_needsMoreColdModeInvocationForTesting) {
    m_needsMoreColdModeInvocationForTesting = false;
    return false;
  }

  // TODO(xiaochengh): Implementation.
  return true;
}

void IdleSpellCheckCallback::handleEvent(IdleDeadline* deadline) {
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
    coldModeInvocation(deadline);
    if (coldModeFinishesFullDocument())
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

}  // namespace blink
