// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/BatteryManager.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/Event.h"
#include "modules/battery/BatteryDispatcher.h"
#include "platform/wtf/Assertions.h"

namespace blink {

BatteryManager* BatteryManager::Create(ExecutionContext* context) {
  BatteryManager* battery_manager = new BatteryManager(context);
  battery_manager->SuspendIfNeeded();
  return battery_manager;
}

BatteryManager::~BatteryManager() {}

BatteryManager::BatteryManager(ExecutionContext* context)
    : SuspendableObject(context),
      PlatformEventController(ToDocument(context)) {}

ScriptPromise BatteryManager::StartRequest(ScriptState* script_state) {
  if (!battery_property_) {
    battery_property_ = new BatteryProperty(
        ExecutionContext::From(script_state), this, BatteryProperty::kReady);

    // If the context is in a stopped state already, do not start updating.
    if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
      battery_property_->Resolve(this);
    } else {
      has_event_listener_ = true;
      StartUpdating();
    }
  }

  return battery_property_->Promise(script_state->World());
}

bool BatteryManager::charging() {
  return battery_status_.Charging();
}

double BatteryManager::chargingTime() {
  return battery_status_.charging_time();
}

double BatteryManager::dischargingTime() {
  return battery_status_.discharging_time();
}

double BatteryManager::level() {
  return battery_status_.Level();
}

void BatteryManager::DidUpdateData() {
  DCHECK(battery_property_);

  BatteryStatus old_status = battery_status_;
  battery_status_ = *BatteryDispatcher::Instance().LatestData();

  if (battery_property_->GetState() == ScriptPromisePropertyBase::kPending) {
    battery_property_->Resolve(this);
    return;
  }

  Document* document = ToDocument(GetExecutionContext());
  DCHECK(document);
  if (document->IsContextSuspended() || document->IsContextDestroyed())
    return;

  if (battery_status_.Charging() != old_status.Charging())
    DispatchEvent(Event::Create(EventTypeNames::chargingchange));
  if (battery_status_.charging_time() != old_status.charging_time())
    DispatchEvent(Event::Create(EventTypeNames::chargingtimechange));
  if (battery_status_.discharging_time() != old_status.discharging_time())
    DispatchEvent(Event::Create(EventTypeNames::dischargingtimechange));
  if (battery_status_.Level() != old_status.Level())
    DispatchEvent(Event::Create(EventTypeNames::levelchange));
}

void BatteryManager::RegisterWithDispatcher() {
  BatteryDispatcher::Instance().AddController(this);
}

void BatteryManager::UnregisterWithDispatcher() {
  BatteryDispatcher::Instance().RemoveController(this);
}

bool BatteryManager::HasLastData() {
  return BatteryDispatcher::Instance().LatestData();
}

void BatteryManager::Suspend() {
  has_event_listener_ = false;
  StopUpdating();
}

void BatteryManager::Resume() {
  has_event_listener_ = true;
  StartUpdating();
}

void BatteryManager::ContextDestroyed(ExecutionContext*) {
  has_event_listener_ = false;
  battery_property_ = nullptr;
  StopUpdating();
}

bool BatteryManager::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners attached to it.
  return GetExecutionContext() && HasEventListeners();
}

void BatteryManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(battery_property_);
  PlatformEventController::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  SuspendableObject::Trace(visitor);
}

}  // namespace blink
