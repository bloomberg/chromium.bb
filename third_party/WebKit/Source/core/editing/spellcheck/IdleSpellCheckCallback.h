// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdleSpellCheckCallback_h
#define IdleSpellCheckCallback_h

#include "core/dom/IdleRequestCallback.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "platform/Timer.h"

namespace blink {

class LocalFrame;
class SpellCheckRequester;

// Main class for the implementation of idle time spell checker.
class CORE_EXPORT IdleSpellCheckCallback final
    : public IdleRequestCallback,
      public SynchronousMutationObserver {
  DISALLOW_COPY_AND_ASSIGN(IdleSpellCheckCallback);
  USING_GARBAGE_COLLECTED_MIXIN(IdleSpellCheckCallback);

 public:
  static IdleSpellCheckCallback* create(LocalFrame&);
  ~IdleSpellCheckCallback() override;

  enum class State {
    kInactive,
    kHotModeRequested,
    kInHotModeInvocation,
    kColdModeTimerStarted,
    kColdModeRequested,
    kInColdModeInvocation
  };

  State state() const { return m_state; }

  // Transit to HotModeRequested, if possible. Called by operations that need
  // spell checker to follow up.
  void setNeedsInvocation();

  // Cleans everything up and makes the callback inactive. Should be called when
  // document is detached or spellchecking is globally disabled.
  void deactivate();

  void documentAttached(Document*);

  // Exposed for testing only.
  SpellCheckRequester& spellCheckRequester() const;
  void forceInvocationForTesting();
  void setNeedsMoreColdModeInvocationForTesting() {
    m_needsMoreColdModeInvocationForTesting = true;
  }
  void skipColdModeTimerForTesting();
  int idleCallbackHandle() const { return m_idleCallbackHandle; }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit IdleSpellCheckCallback(LocalFrame&);
  void handleEvent(IdleDeadline*) override;

  LocalFrame& frame() const { return *m_frame; }

  // Returns whether spell checking is globally enabled.
  bool isSpellCheckingEnabled() const;

  // Calls requestIdleCallback with this IdleSpellCheckCallback.
  void requestInvocation();

  // Functions for hot mode.
  void hotModeInvocation(IdleDeadline*);

  // Transit to ColdModeTimerStarted, if possible. Sets up a timer, and requests
  // cold mode invocation if no critical operation occurs before timer firing.
  void setNeedsColdModeInvocation();

  // Functions for cold mode.
  void coldModeTimerFired(TimerBase*);
  void coldModeInvocation(IdleDeadline*);
  bool coldModeFinishesFullDocument() const;

  // Implements |SynchronousMutationObserver|.
  void contextDestroyed(Document*) final;

  State m_state;
  int m_idleCallbackHandle;
  mutable bool m_needsMoreColdModeInvocationForTesting;
  const Member<LocalFrame> m_frame;

  TaskRunnerTimer<IdleSpellCheckCallback> m_coldModeTimer;
};

}  // namespace blink

#endif  // IdleSpellCheckCallback_h
