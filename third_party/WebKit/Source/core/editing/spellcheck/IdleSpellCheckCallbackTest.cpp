// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/IdleSpellCheckCallback.h"

#include "core/editing/spellcheck/SpellCheckTestBase.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

using State = IdleSpellCheckCallback::State;

class IdleSpellCheckCallbackTest : public SpellCheckTestBase {
 protected:
  IdleSpellCheckCallback& IdleChecker() {
    return GetFrame().GetSpellChecker().GetIdleSpellCheckCallback();
  }

  void TransitTo(State state) {
    switch (state) {
      case State::kInactive:
        IdleChecker().Deactivate();
        break;
      case State::kHotModeRequested:
        IdleChecker().SetNeedsInvocation();
        break;
      case State::kColdModeTimerStarted:
        break;
      case State::kColdModeRequested:
        IdleChecker().SkipColdModeTimerForTesting();
        break;
      case State::kInHotModeInvocation:
      case State::kInColdModeInvocation:
        NOTREACHED();
    }
  }
};

// Test cases for lifecycle state transitions.

TEST_F(IdleSpellCheckCallbackTest, Initialization) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenInactive) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kInactive);
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenHotModeRequested) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kHotModeRequested);
  int handle = IdleChecker().IdleCallbackHandle();
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_EQ(handle, IdleChecker().IdleCallbackHandle());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenColdModeTimerStarted) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenColdModeRequested) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  int handle = IdleChecker().IdleCallbackHandle();
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(handle, IdleChecker().IdleCallbackHandle());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, HotModeTransitToColdMode) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kHotModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, ColdModeTimerStartedToRequested) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().SkipColdModeTimerForTesting();
  EXPECT_EQ(State::kColdModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, ColdModeStayAtColdMode) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  IdleChecker().SetNeedsMoreColdModeInvocationForTesting();
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, ColdModeToInactive) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenInactive) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kInactive);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenHotModeRequested) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kHotModeRequested);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenColdModeTimerStarted) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeTimerStarted);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenColdModeRequested) {
  if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

}  // namespace blink
