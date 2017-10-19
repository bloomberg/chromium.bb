// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/WorkerNavigatorBudget.h"

#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerNavigator.h"
#include "core/workers/WorkerThread.h"
#include "modules/budget/BudgetService.h"

namespace blink {

WorkerNavigatorBudget::WorkerNavigatorBudget(WorkerNavigator& worker_navigator)
    : Supplement<WorkerNavigator>(worker_navigator) {}

// static
const char* WorkerNavigatorBudget::SupplementName() {
  return "WorkerNavigatorBudget";
}

// static
WorkerNavigatorBudget& WorkerNavigatorBudget::From(
    WorkerNavigator& worker_navigator) {
  // Get the unique WorkerNavigatorBudget associated with this workerNavigator.
  WorkerNavigatorBudget* worker_navigator_budget =
      static_cast<WorkerNavigatorBudget*>(Supplement<WorkerNavigator>::From(
          worker_navigator, SupplementName()));
  if (!worker_navigator_budget) {
    // If there isn't one already, create it now and associate it.
    worker_navigator_budget = new WorkerNavigatorBudget(worker_navigator);
    Supplement<WorkerNavigator>::ProvideTo(worker_navigator, SupplementName(),
                                           worker_navigator_budget);
  }
  return *worker_navigator_budget;
}

BudgetService* WorkerNavigatorBudget::budget(ExecutionContext* context) {
  if (!budget_) {
    WorkerThread* thread = ToWorkerGlobalScope(context)->GetThread();
    budget_ = BudgetService::Create(&thread->GetInterfaceProvider());
  }
  return budget_.Get();
}

// static
BudgetService* WorkerNavigatorBudget::budget(
    ExecutionContext* context,
    WorkerNavigator& worker_navigator) {
  return WorkerNavigatorBudget::From(worker_navigator).budget(context);
}

void WorkerNavigatorBudget::Trace(blink::Visitor* visitor) {
  visitor->Trace(budget_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

}  // namespace blink
