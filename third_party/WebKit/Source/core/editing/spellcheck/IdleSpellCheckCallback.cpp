// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/IdleSpellCheckCallback.h"

#include "core/dom/IdleRequestOptions.h"
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
#include "platform/tracing/TraceEvent.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

const int kColdModeTimerIntervalMS = 1000;
const int kConsecutiveColdModeTimerIntervalMS = 200;
const int kRequestTimeoutMS = 200;

}  // namespace

IdleSpellCheckCallback::~IdleSpellCheckCallback() {}

DEFINE_TRACE(IdleSpellCheckCallback) {
  visitor->trace(m_frame);
  IdleRequestCallback::trace(visitor);
}

IdleSpellCheckCallback* IdleSpellCheckCallback::create(LocalFrame& frame) {
  return new IdleSpellCheckCallback(frame);
}

IdleSpellCheckCallback::IdleSpellCheckCallback(LocalFrame& frame)
    : m_state(State::kInactive),
      m_frame(frame),
      m_coldModeTimer(this, &IdleSpellCheckCallback::coldModeTimerFired) {}

SpellCheckRequester& IdleSpellCheckCallback::spellCheckRequester() const {
  // TODO(xiaochengh): decouple with SpellChecker after SpellCheckRequester is
  // moved to IdleSpellCheckCallback.
  return frame().spellChecker().spellCheckRequester();
}

bool IdleSpellCheckCallback::isSpellCheckingEnabled() const {
  // TODO(xiaochengh): decouple with SpellChecker.
  return frame().spellChecker().isSpellCheckingEnabled();
}

void IdleSpellCheckCallback::prepareForLeakDetection() {
  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    spellCheckRequester().prepareForLeakDetection();
}

void IdleSpellCheckCallback::requestInvocation() {
  IdleRequestOptions options;
  options.setTimeout(kRequestTimeoutMS);
  frame().document()->requestIdleCallback(this, options);
}

void IdleSpellCheckCallback::deactivate() {
  m_state = State::kInactive;
  if (m_coldModeTimer.isActive())
    m_coldModeTimer.stop();
}

void IdleSpellCheckCallback::setNeedsHotModeInvocation() {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  if (!isSpellCheckingEnabled()) {
    deactivate();
    return;
  }

  if (m_state == State::kColdModeTimerStarted) {
    DCHECK(m_coldModeTimer.isActive());
    m_coldModeTimer.stop();
  }

  if (m_state != State::kColdModeRequested)
    requestInvocation();
  m_state = State::kHotModeRequested;
}

void IdleSpellCheckCallback::setNeedsColdModeInvocation() {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

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
  // TODO(xiaochengh): Implementation.
  return true;
}

void IdleSpellCheckCallback::handleEvent(IdleDeadline* deadline) {
  DCHECK(frame().document());
  DCHECK(frame().document()->isActive());

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

}  // namespace blink
