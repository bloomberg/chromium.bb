// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DeviceOrientationController::DeviceOrientationController(Document& document)
    : DeviceSingleWindowEventController(document),
      Supplement<Document>(document) {}

DeviceOrientationController::~DeviceOrientationController() = default;

void DeviceOrientationController::DidUpdateData() {
  if (override_orientation_data_)
    return;
  DispatchDeviceEvent(LastEvent());
}

const char DeviceOrientationController::kSupplementName[] =
    "DeviceOrientationController";

DeviceOrientationController& DeviceOrientationController::From(
    Document& document) {
  DeviceOrientationController* controller =
      Supplement<Document>::From<DeviceOrientationController>(document);
  if (!controller) {
    controller = new DeviceOrientationController(document);
    ProvideTo(document, controller);
  }
  return *controller;
}

void DeviceOrientationController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  if (frame) {
    if (GetDocument().IsSecureContext()) {
      UseCounter::Count(frame, WebFeature::kDeviceOrientationSecureOrigin);
    } else {
      Deprecation::CountDeprecation(
          frame, WebFeature::kDeviceOrientationInsecureOrigin);
      HostsUsingFeatures::CountAnyWorld(
          GetDocument(),
          HostsUsingFeatures::Feature::kDeviceOrientationInsecureHost);
      if (GetDocument()
              .GetFrame()
              ->GetSettings()
              ->GetStrictPowerfulFeatureRestrictions())
        return;
    }
  }

  if (!has_event_listener_) {
    Platform::Current()->RecordRapporURL("DeviceSensors.DeviceOrientation",
                                         WebURL(GetDocument().Url()));

    if (!IsSameSecurityOriginAsMainFrame()) {
      Platform::Current()->RecordRapporURL(
          "DeviceSensors.DeviceOrientationCrossOrigin",
          WebURL(GetDocument().Url()));
    }

    if (!CheckPolicyFeatures({mojom::FeaturePolicyFeature::kAccelerometer,
                              mojom::FeaturePolicyFeature::kGyroscope})) {
      LogToConsolePolicyFeaturesDisabled(frame, EventTypeName());
      return;
    }
  }

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

DeviceOrientationData* DeviceOrientationController::LastData() const {
  return override_orientation_data_
             ? override_orientation_data_.Get()
             : orientation_event_pump_
                   ? orientation_event_pump_->LatestDeviceOrientationData()
                   : nullptr;
}

bool DeviceOrientationController::HasLastData() {
  return LastData();
}

void DeviceOrientationController::RegisterWithDispatcher() {
  RegisterWithOrientationEventPump(false /* absolute */);
}

void DeviceOrientationController::UnregisterWithDispatcher() {
  if (orientation_event_pump_)
    orientation_event_pump_->RemoveController(this);
}

Event* DeviceOrientationController::LastEvent() const {
  return DeviceOrientationEvent::Create(EventTypeName(), LastData());
}

bool DeviceOrientationController::IsNullEvent(Event* event) const {
  DeviceOrientationEvent* orientation_event = ToDeviceOrientationEvent(event);
  return !orientation_event->Orientation()->CanProvideEventData();
}

const AtomicString& DeviceOrientationController::EventTypeName() const {
  return EventTypeNames::deviceorientation;
}

void DeviceOrientationController::SetOverride(
    DeviceOrientationData* device_orientation_data) {
  DCHECK(device_orientation_data);
  override_orientation_data_ = device_orientation_data;
  DispatchDeviceEvent(LastEvent());
}

void DeviceOrientationController::ClearOverride() {
  if (!override_orientation_data_)
    return;
  override_orientation_data_.Clear();
  if (LastData())
    DidUpdateData();
}

void DeviceOrientationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(override_orientation_data_);
  visitor->Trace(orientation_event_pump_);
  DeviceSingleWindowEventController::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

void DeviceOrientationController::RegisterWithOrientationEventPump(
    bool absolute) {
  if (!orientation_event_pump_) {
    LocalFrame* frame = GetDocument().GetFrame();
    if (!frame)
      return;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        frame->GetTaskRunner(TaskType::kSensor);
    orientation_event_pump_ =
        new DeviceOrientationEventPump(task_runner, absolute);
  }
  orientation_event_pump_->AddController(this);
}

// static
void DeviceOrientationController::LogToConsolePolicyFeaturesDisabled(
    LocalFrame* frame,
    const AtomicString& event_name) {
  if (!frame)
    return;
  const String& message = String::Format(
      "The %s events are blocked by feature policy. "
      "See "
      "https://github.com/WICG/feature-policy/blob/master/"
      "features.md#sensor-features",
      event_name.Ascii().data());
  ConsoleMessage* console_message = ConsoleMessage::Create(
      kJSMessageSource, kWarningMessageLevel, std::move(message));
  frame->Console().AddMessage(console_message);
}

}  // namespace blink
