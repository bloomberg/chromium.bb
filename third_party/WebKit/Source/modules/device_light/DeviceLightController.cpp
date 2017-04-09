// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_light/DeviceLightController.h"

#include "core/dom/Document.h"
#include "modules/EventModules.h"
#include "modules/device_light/DeviceLightDispatcher.h"
#include "modules/device_light/DeviceLightEvent.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

DeviceLightController::DeviceLightController(Document& document)
    : DeviceSingleWindowEventController(document),
      Supplement<Document>(document) {}

DeviceLightController::~DeviceLightController() {}

const char* DeviceLightController::SupplementName() {
  return "DeviceLightController";
}

DeviceLightController& DeviceLightController::From(Document& document) {
  DeviceLightController* controller = static_cast<DeviceLightController*>(
      Supplement<Document>::From(document, SupplementName()));
  if (!controller) {
    controller = new DeviceLightController(document);
    Supplement<Document>::ProvideTo(document, SupplementName(), controller);
  }
  return *controller;
}

bool DeviceLightController::HasLastData() {
  return DeviceLightDispatcher::Instance().LatestDeviceLightData() >= 0;
}

void DeviceLightController::RegisterWithDispatcher() {
  DeviceLightDispatcher::Instance().AddController(this);
}

void DeviceLightController::UnregisterWithDispatcher() {
  DeviceLightDispatcher::Instance().RemoveController(this);
}

Event* DeviceLightController::LastEvent() const {
  return DeviceLightEvent::Create(
      EventTypeNames::devicelight,
      DeviceLightDispatcher::Instance().LatestDeviceLightData());
}

bool DeviceLightController::IsNullEvent(Event* event) const {
  DeviceLightEvent* light_event = ToDeviceLightEvent(event);
  return light_event->value() == std::numeric_limits<double>::infinity();
}

const AtomicString& DeviceLightController::EventTypeName() const {
  return EventTypeNames::devicelight;
}

DEFINE_TRACE(DeviceLightController) {
  DeviceSingleWindowEventController::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
