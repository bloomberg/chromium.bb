// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_orientation/DeviceOrientationController.h"

#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/Settings.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/EventModules.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "modules/device_orientation/DeviceOrientationDispatcher.h"
#include "modules/device_orientation/DeviceOrientationEvent.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/Platform.h"

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
             : DispatcherInstance().LatestDeviceOrientationData();
}

bool DeviceOrientationController::HasLastData() {
  return LastData();
}

void DeviceOrientationController::RegisterWithDispatcher() {
  DispatcherInstance().AddController(this);
}

void DeviceOrientationController::UnregisterWithDispatcher() {
  DispatcherInstance().RemoveController(this);
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

DeviceOrientationDispatcher& DeviceOrientationController::DispatcherInstance()
    const {
  return DeviceOrientationDispatcher::Instance(false);
}

void DeviceOrientationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(override_orientation_data_);
  DeviceSingleWindowEventController::Trace(visitor);
  Supplement<Document>::Trace(visitor);
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
      "https://github.com/WICG/feature-policy/blob/gh-pages/"
      "features.md#sensor-features",
      event_name.Ascii().data());
  ConsoleMessage* console_message = ConsoleMessage::Create(
      kJSMessageSource, kWarningMessageLevel, std::move(message));
  frame->Console().AddMessage(console_message);
}

}  // namespace blink
