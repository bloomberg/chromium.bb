// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DeviceMotionController::DeviceMotionController(Document& document)
    : DeviceSingleWindowEventController(document),
      Supplement<Document>(document) {}

DeviceMotionController::~DeviceMotionController() = default;

const char DeviceMotionController::kSupplementName[] = "DeviceMotionController";

DeviceMotionController& DeviceMotionController::From(Document& document) {
  DeviceMotionController* controller =
      Supplement<Document>::From<DeviceMotionController>(document);
  if (!controller) {
    controller = new DeviceMotionController(document);
    ProvideTo(document, controller);
  }
  return *controller;
}

void DeviceMotionController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  if (frame) {
    if (GetDocument().IsSecureContext()) {
      UseCounter::Count(frame, WebFeature::kDeviceMotionSecureOrigin);
    } else {
      Deprecation::CountDeprecation(frame,
                                    WebFeature::kDeviceMotionInsecureOrigin);
      HostsUsingFeatures::CountAnyWorld(
          GetDocument(),
          HostsUsingFeatures::Feature::kDeviceMotionInsecureHost);
      if (frame->GetSettings()->GetStrictPowerfulFeatureRestrictions())
        return;
    }
  }

  if (!has_event_listener_) {
    Platform::Current()->RecordRapporURL("DeviceSensors.DeviceMotion",
                                         WebURL(GetDocument().Url()));

    if (!IsSameSecurityOriginAsMainFrame()) {
      Platform::Current()->RecordRapporURL(
          "DeviceSensors.DeviceMotionCrossOrigin", WebURL(GetDocument().Url()));
    }

    if (!CheckPolicyFeatures({mojom::FeaturePolicyFeature::kAccelerometer,
                              mojom::FeaturePolicyFeature::kGyroscope})) {
      DeviceOrientationController::LogToConsolePolicyFeaturesDisabled(
          frame, EventTypeName());
      return;
    }
  }

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

bool DeviceMotionController::HasLastData() {
  return motion_event_pump_
             ? motion_event_pump_->LatestDeviceMotionData() != nullptr
             : false;
}

void DeviceMotionController::RegisterWithDispatcher() {
  if (!motion_event_pump_) {
    LocalFrame* frame = GetDocument().GetFrame();
    if (!frame)
      return;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        frame->GetTaskRunner(TaskType::kSensor);
    motion_event_pump_ = new DeviceMotionEventPump(task_runner);
  }
  motion_event_pump_->SetController(this);
}

void DeviceMotionController::UnregisterWithDispatcher() {
  if (motion_event_pump_)
    motion_event_pump_->RemoveController();
}

Event* DeviceMotionController::LastEvent() const {
  return DeviceMotionEvent::Create(
      EventTypeNames::devicemotion,
      motion_event_pump_ ? motion_event_pump_->LatestDeviceMotionData()
                         : nullptr);
}

bool DeviceMotionController::IsNullEvent(Event* event) const {
  DeviceMotionEvent* motion_event = ToDeviceMotionEvent(event);
  return !motion_event->GetDeviceMotionData()->CanProvideEventData();
}

const AtomicString& DeviceMotionController::EventTypeName() const {
  return EventTypeNames::devicemotion;
}

void DeviceMotionController::Trace(blink::Visitor* visitor) {
  DeviceSingleWindowEventController::Trace(visitor);
  visitor->Trace(motion_event_pump_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
