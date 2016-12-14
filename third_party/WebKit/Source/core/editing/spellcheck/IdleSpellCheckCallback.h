// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdleSpellCheckCallback_h
#define IdleSpellCheckCallback_h

#include "core/dom/IdleRequestCallback.h"
#include "core/frame/LocalFrame.h"
#include "platform/Timer.h"

namespace blink {

// Main class for the implementation of idle time spell checker.
class IdleSpellCheckCallback final : public IdleRequestCallback {
 public:
  // TODO(xiaochengh): Make each LocalFrame own an IdleSpellCheckCallback.
  static IdleSpellCheckCallback* create(LocalFrame&);
  ~IdleSpellCheckCallback() override;

  // Transit to HotModeRequested, if possible. Called by operations that need
  // spell checker to follow up.
  // TODO(xiaochengh): Add proper call sites.
  void setNeedsHotModeInvocation();

  // Transit to ColdModeTimerStarted, if possible. Sets up a timer, and requests
  // cold mode invocation if no critical operation occurs before timer firing.
  // TODO(xiaochengh): Add proper call sites.
  void setNeedsColdModeInvocation();

  // Cleans everything up and makes the callback inactive. Should be called when
  // document is detached or spellchecking is globally disabled.
  // TODO(xiaochengh): Add proper call sites.
  void deactivate();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit IdleSpellCheckCallback(LocalFrame&);
  void handleEvent(IdleDeadline*) override;

  LocalFrame& frame() const { return *m_frame; }

  enum class State {
    kInactive,
    kHotModeRequested,
    kInHotModeInvocation,
    kColdModeTimerStarted,
    kColdModeRequested,
    kInColdModeInvocation
  };

  // Returns whether spell checking is globally enabled.
  bool isSpellCheckingEnabled() const;

  // Calls requestIdleCallback with this IdleSpellCheckCallback.
  void requestInvocation();

  // Functions for hot mode.
  void hotModeInvocation(IdleDeadline*);

  // Functions for cold mode.
  void coldModeTimerFired(TimerBase*);
  void coldModeInvocation(IdleDeadline*);
  bool coldModeFinishesFullDocument() const;

  State m_state;
  const Member<LocalFrame> m_frame;

  // TODO(xiaochengh): assign the timer to some proper task runner.
  Timer<IdleSpellCheckCallback> m_coldModeTimer;
};

}  // namespace blink

#endif  // IdleSpellCheckCallback_h
