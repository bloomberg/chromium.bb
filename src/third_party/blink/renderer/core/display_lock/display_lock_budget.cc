// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

namespace blink {

DisplayLockBudget::DisplayLockBudget(DisplayLockContext* context)
    : context_(context) {}

bool DisplayLockBudget::MarkAncestorsDirtyForPhaseIfNeeded(Phase phase) {
  switch (phase) {
    case Phase::kStyle:
      return context_->MarkAncestorsForStyleRecalcIfNeeded();
    case Phase::kLayout:
      return context_->MarkAncestorsForLayoutIfNeeded();
    case Phase::kPrePaint:
      return context_->MarkAncestorsForPrePaintIfNeeded();
  }
  NOTREACHED();
  return false;
}

bool DisplayLockBudget::IsElementDirtyForPhase(Phase phase) const {
  switch (phase) {
    case Phase::kStyle:
      return context_->IsElementDirtyForStyleRecalc();
    case Phase::kLayout:
      return context_->IsElementDirtyForLayout();
    case Phase::kPrePaint:
      return context_->IsElementDirtyForPrePaint();
  }
  NOTREACHED();
  return false;
}

}  // namespace blink
