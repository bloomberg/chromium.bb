// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BudgetState_h
#define BudgetState_h

#include "core/dom/DOMTimeStamp.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class BudgetService;

// This exposes the BudgetState interface which is returned from BudgetService
// when there is a GetBudget call.
class BudgetState final : public GarbageCollected<BudgetState>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BudgetState();
  BudgetState(double budget_at, DOMTimeStamp);
  BudgetState(const BudgetState& other);

  double budgetAt() const { return budget_at_; }
  DOMTimeStamp time() const { return time_; }

  void SetBudgetAt(const double budget_at) { budget_at_ = budget_at; }
  void SetTime(const DOMTimeStamp& time) { time_ = time; }

  void Trace(blink::Visitor* visitor) {}

 private:
  double budget_at_;
  DOMTimeStamp time_;
};

}  // namespace blink

#endif  // BudgetState_h
