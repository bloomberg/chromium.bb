// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/NavigatorBattery.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/battery/BatteryManager.h"

namespace blink {

NavigatorBattery::NavigatorBattery(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise NavigatorBattery::getBattery(ScriptState* script_state,
                                           Navigator& navigator) {
  return NavigatorBattery::From(navigator).getBattery(script_state);
}

ScriptPromise NavigatorBattery::getBattery(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Check secure context.
  String error_message;
  if (!execution_context->IsSecureContext(error_message)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, error_message));
  }

  // Check top-level browsing context.
  if (!ToDocument(execution_context)->domWindow()->GetFrame() ||
      !ToDocument(execution_context)->GetFrame()->IsMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(
                          kSecurityError, "Not a top-level browsing context."));
  }

  if (!battery_manager_)
    battery_manager_ = BatteryManager::Create(execution_context);

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

DEFINE_TRACE(NavigatorBattery) {
  visitor->Trace(battery_manager_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
