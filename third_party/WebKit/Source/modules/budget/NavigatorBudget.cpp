// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/NavigatorBudget.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Navigator.h"
#include "modules/budget/BudgetService.h"

namespace blink {

NavigatorBudget::NavigatorBudget(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
const char NavigatorBudget::kSupplementName[] = "NavigatorBudget";

// static
NavigatorBudget& NavigatorBudget::From(Navigator& navigator) {
  // Get the unique NavigatorBudget associated with this navigator.
  NavigatorBudget* navigator_budget =
      Supplement<Navigator>::From<NavigatorBudget>(navigator);
  if (!navigator_budget) {
    // If there isn't one already, create it now and associate it.
    navigator_budget = new NavigatorBudget(navigator);
    ProvideTo(navigator, navigator_budget);
  }
  return *navigator_budget;
}

BudgetService* NavigatorBudget::budget(ExecutionContext* context) {
  if (!budget_) {
    if (auto* interface_provider = context->GetInterfaceProvider()) {
      budget_ = BudgetService::Create(interface_provider);
    }
  }
  return budget_.Get();
}

// static
BudgetService* NavigatorBudget::budget(ExecutionContext* context,
                                       Navigator& navigator) {
  return NavigatorBudget::From(navigator).budget(context);
}

void NavigatorBudget::Trace(blink::Visitor* visitor) {
  visitor->Trace(budget_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
