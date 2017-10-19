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
const char* NavigatorBudget::SupplementName() {
  return "NavigatorBudget";
}

// static
NavigatorBudget& NavigatorBudget::From(Navigator& navigator) {
  // Get the unique NavigatorBudget associated with this navigator.
  NavigatorBudget* navigator_budget = static_cast<NavigatorBudget*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!navigator_budget) {
    // If there isn't one already, create it now and associate it.
    navigator_budget = new NavigatorBudget(navigator);
    Supplement<Navigator>::ProvideTo(navigator, SupplementName(),
                                     navigator_budget);
  }
  return *navigator_budget;
}

BudgetService* NavigatorBudget::budget() {
  if (!budget_) {
    Navigator* navigator = GetSupplementable();
    if (navigator->GetFrame()) {
      budget_ = BudgetService::Create(
          navigator->GetFrame()->Client()->GetInterfaceProvider());
    }
  }
  return budget_.Get();
}

// static
BudgetService* NavigatorBudget::budget(Navigator& navigator) {
  return NavigatorBudget::From(navigator).budget();
}

void NavigatorBudget::Trace(blink::Visitor* visitor) {
  visitor->Trace(budget_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
