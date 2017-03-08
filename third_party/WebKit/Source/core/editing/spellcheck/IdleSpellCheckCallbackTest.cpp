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
  IdleSpellCheckCallback& idleChecker() {
    return frame().spellChecker().idleSpellCheckCallback();
  }

  void SetUp() override {
    SpellCheckTestBase::SetUp();
    if (!RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
      idleChecker().documentAttached(frame().document());
  }

  void transitTo(State state) {
    switch (state) {
      case State::kInactive:
        idleChecker().deactivate();
        break;
      case State::kHotModeRequested:
        idleChecker().setNeedsInvocation();
        break;
      case State::kColdModeTimerStarted:
        break;
      case State::kColdModeRequested:
        idleChecker().skipColdModeTimerForTesting();
        break;
      case State::kInHotModeInvocation:
      case State::kInColdModeInvocation:
        NOTREACHED();
    }
  }
};

// Test cases for lifecycle state transitions.

TEST_F(IdleSpellCheckCallbackTest, Initialization) {
  EXPECT_EQ(State::kColdModeTimerStarted, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenInactive) {
  transitTo(State::kInactive);
  idleChecker().setNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, idleChecker().state());
  EXPECT_NE(-1, idleChecker().idleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenHotModeRequested) {
  transitTo(State::kHotModeRequested);
  int handle = idleChecker().idleCallbackHandle();
  idleChecker().setNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, idleChecker().state());
  EXPECT_EQ(handle, idleChecker().idleCallbackHandle());
  EXPECT_NE(-1, idleChecker().idleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenColdModeTimerStarted) {
  transitTo(State::kColdModeTimerStarted);
  idleChecker().setNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, idleChecker().state());
  EXPECT_NE(-1, idleChecker().idleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, RequestWhenColdModeRequested) {
  transitTo(State::kColdModeRequested);
  int handle = idleChecker().idleCallbackHandle();
  idleChecker().setNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, idleChecker().state());
  EXPECT_NE(handle, idleChecker().idleCallbackHandle());
  EXPECT_NE(-1, idleChecker().idleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, HotModeTransitToColdMode) {
  transitTo(State::kHotModeRequested);
  idleChecker().forceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, ColdModeTimerStartedToRequested) {
  transitTo(State::kColdModeTimerStarted);
  idleChecker().skipColdModeTimerForTesting();
  EXPECT_EQ(State::kColdModeRequested, idleChecker().state());
  EXPECT_NE(-1, idleChecker().idleCallbackHandle());
}

TEST_F(IdleSpellCheckCallbackTest, ColdModeStayAtColdMode) {
  transitTo(State::kColdModeRequested);
  idleChecker().setNeedsMoreColdModeInvocationForTesting();
  idleChecker().forceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, ColdModeToInactive) {
  transitTo(State::kColdModeRequested);
  idleChecker().forceInvocationForTesting();
  EXPECT_EQ(State::kInactive, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenInactive) {
  transitTo(State::kInactive);
  document().shutdown();
  EXPECT_EQ(State::kInactive, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenHotModeRequested) {
  transitTo(State::kHotModeRequested);
  document().shutdown();
  EXPECT_EQ(State::kInactive, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenColdModeTimerStarted) {
  transitTo(State::kColdModeTimerStarted);
  document().shutdown();
  EXPECT_EQ(State::kInactive, idleChecker().state());
}

TEST_F(IdleSpellCheckCallbackTest, DetachWhenColdModeRequested) {
  transitTo(State::kColdModeRequested);
  document().shutdown();
  EXPECT_EQ(State::kInactive, idleChecker().state());
}

}  // namespace blink
