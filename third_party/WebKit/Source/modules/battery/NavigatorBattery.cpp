// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/NavigatorBattery.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "modules/battery/BatteryManager.h"

namespace blink {

NavigatorBattery::NavigatorBattery(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise NavigatorBattery::getBattery(ScriptState* script_state,
                                           Navigator& navigator) {
  return NavigatorBattery::From(navigator).getBattery(script_state);
}

ScriptPromise NavigatorBattery::getBattery(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  // Check to see if this request would be blocked according to the Battery
  // Status API specification.
  if (context->IsDocument()) {
    LocalFrame* frame = ToDocument(context)->GetFrame();
    if (frame) {
      if (!context->IsSecureContext())
        UseCounter::Count(frame, WebFeature::kBatteryStatusInsecureOrigin);
      UseCounter::CountIfFeatureWouldBeBlockedByFeaturePolicy(
          *frame, WebFeature::kBatteryStatusCrossOrigin,
          WebFeature::kBatteryStatusSameOriginABA);
    }
  }

  if (!battery_manager_)
    battery_manager_ = BatteryManager::Create(context);
  return battery_manager_->StartRequest(script_state);
}

const char* NavigatorBattery::SupplementName() {
  return "NavigatorBattery";
}

NavigatorBattery& NavigatorBattery::From(Navigator& navigator) {
  NavigatorBattery* supplement = static_cast<NavigatorBattery*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorBattery(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

void NavigatorBattery::Trace(blink::Visitor* visitor) {
  visitor->Trace(battery_manager_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
