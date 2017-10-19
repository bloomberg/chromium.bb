// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_orientation/DeviceOrientationAbsoluteController.h"

#include "core/frame/Settings.h"
#include "modules/device_orientation/DeviceOrientationDispatcher.h"

namespace blink {

DeviceOrientationAbsoluteController::DeviceOrientationAbsoluteController(
    Document& document)
    : DeviceOrientationController(document) {}

DeviceOrientationAbsoluteController::~DeviceOrientationAbsoluteController() {}

const char* DeviceOrientationAbsoluteController::SupplementName() {
  return "DeviceOrientationAbsoluteController";
}

DeviceOrientationAbsoluteController& DeviceOrientationAbsoluteController::From(
    Document& document) {
  DeviceOrientationAbsoluteController* controller =
      static_cast<DeviceOrientationAbsoluteController*>(
          Supplement<Document>::From(
              document, DeviceOrientationAbsoluteController::SupplementName()));
  if (!controller) {
    controller = new DeviceOrientationAbsoluteController(document);
    Supplement<Document>::ProvideTo(
        document, DeviceOrientationAbsoluteController::SupplementName(),
        controller);
  }
  return *controller;
}

void DeviceOrientationAbsoluteController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  if (GetDocument().GetFrame()) {
    if (GetDocument().IsSecureContext()) {
      UseCounter::Count(GetDocument().GetFrame(),
                        WebFeature::kDeviceOrientationAbsoluteSecureOrigin);
    } else {
      Deprecation::CountDeprecation(
          GetDocument().GetFrame(),
          WebFeature::kDeviceOrientationAbsoluteInsecureOrigin);
      // TODO: add rappor logging of insecure origins as in
      // DeviceOrientationController.
      if (GetDocument()
              .GetFrame()
              ->GetSettings()
              ->GetStrictPowerfulFeatureRestrictions())
        return;
    }
  }

  // TODO: add rappor url logging as in DeviceOrientationController.

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

DeviceOrientationDispatcher&
DeviceOrientationAbsoluteController::DispatcherInstance() const {
  return DeviceOrientationDispatcher::Instance(true);
}

const AtomicString& DeviceOrientationAbsoluteController::EventTypeName() const {
  return EventTypeNames::deviceorientationabsolute;
}

void DeviceOrientationAbsoluteController::Trace(blink::Visitor* visitor) {
  DeviceOrientationController::Trace(visitor);
}

}  // namespace blink
