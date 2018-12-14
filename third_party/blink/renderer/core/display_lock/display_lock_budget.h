// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_BUDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_BUDGET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class DisplayLockContext;
class CORE_EXPORT DisplayLockBudget {
 public:
  enum class Phase : unsigned {
    kStyle,
    kLayout,
    kPrePaint,
    kPaint,
    kFirst = kStyle,
    kLast = kPaint
  };

  DisplayLockBudget(DisplayLockContext*);
  virtual ~DisplayLockBudget() = default;

  // Returns true if the given phase is allowed to proceed under the current
  // budget.
  virtual bool ShouldPerformPhase(Phase) const = 0;

  // Notifies the budget that the given phase was completed.
  virtual void DidPerformPhase(Phase) = 0;

  // Notifies the budget that a new lifecycle update phase is going to start.
  virtual void WillStartLifecycleUpdate() = 0;

  // Notifies the budget that a lifecycle update phase finished. This returns
  // true if another update cycle is needed in order to process more phases.
  virtual bool DidFinishLifecycleUpdate() = 0;

 protected:
  // Marks the ancestor chain dirty for the given phase if it's needed. Returns
  // true if the ancestors were marked dirty and false otherwise.
  bool MarkAncestorsDirtyForPhaseIfNeeded(Phase);

 private:
  // This is a backpointer to the context, which should always outlive this
  // budget, so it's untraced.
  UntracedMember<DisplayLockContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_BUDGET_H_
