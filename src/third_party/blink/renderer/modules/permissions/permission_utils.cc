// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_utils.h"

#include <utility>

#include "build/build_config.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_camera_device_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_permission_descriptor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// There are two PermissionDescriptor, one in Mojo bindings and one
// in v8 bindings so we'll rename one here.
using MojoPermissionDescriptor = mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

void ConnectToPermissionService(
    ExecutionContext* execution_context,
    mojo::PendingReceiver<mojom::blink::PermissionService> receiver) {
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      std::move(receiver));
}

String PermissionStatusToString(mojom::blink::PermissionStatus status) {
  switch (status) {
    case mojom::blink::PermissionStatus::GRANTED:
      return "granted";
    case mojom::blink::PermissionStatus::DENIED:
      return "denied";
    case mojom::blink::PermissionStatus::ASK:
      return "prompt";
  }
  NOTREACHED();
  return "denied";
}

PermissionDescriptorPtr CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = MojoPermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

PermissionDescriptorPtr CreateMidiPermissionDescriptor(bool sysex) {
  auto descriptor =
      CreatePermissionDescriptor(mojom::blink::PermissionName::MIDI);
  auto midi_extension = mojom::blink::MidiPermissionDescriptor::New();
  midi_extension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_midi(std::move(midi_extension));
  return descriptor;
}

PermissionDescriptorPtr CreateClipboardPermissionDescriptor(
    PermissionName name,
    bool allow_without_gesture,
    bool allow_without_sanitization) {
  auto descriptor = CreatePermissionDescriptor(name);
  auto clipboard_extension = mojom::blink::ClipboardPermissionDescriptor::New(
      allow_without_gesture, allow_without_sanitization);
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_clipboard(std::move(clipboard_extension));
  return descriptor;
}

PermissionDescriptorPtr CreateVideoCapturePermissionDescriptor(
    bool pan_tilt_zoom) {
  auto descriptor =
      CreatePermissionDescriptor(mojom::blink::PermissionName::VIDEO_CAPTURE);
  auto camera_device_extension =
      mojom::blink::CameraDevicePermissionDescriptor::New(pan_tilt_zoom);
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_camera_device(std::move(camera_device_extension));
  return descriptor;
}

PermissionDescriptorPtr ParsePermissionDescriptor(
    ScriptState* script_state,
    const ScriptValue& raw_descriptor,
    ExceptionState& exception_state) {
  PermissionDescriptor* permission =
      NativeValueTraits<PermissionDescriptor>::NativeValue(
          script_state->GetIsolate(), raw_descriptor.V8Value(),
          exception_state);

  if (exception_state.HadException())
    return nullptr;

  const String& name = permission->name();
  if (name == "geolocation")
    return CreatePermissionDescriptor(PermissionName::GEOLOCATION);
  if (name == "camera") {
#if !defined(OS_ANDROID)
    CameraDevicePermissionDescriptor* camera_device_permission =
        NativeValueTraits<CameraDevicePermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    if (exception_state.HadException())
      return nullptr;

    if (RuntimeEnabledFeatures::MediaCapturePanTiltEnabled()) {
      return CreateVideoCapturePermissionDescriptor(
          camera_device_permission->panTiltZoom());
    }
#endif
    return CreateVideoCapturePermissionDescriptor(false /* pan_tilt_zoom */);
  }
  if (name == "microphone")
    return CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE);
  if (name == "notifications")
    return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
  if (name == "persistent-storage")
    return CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE);
  if (name == "push") {
    PushPermissionDescriptor* push_permission =
        NativeValueTraits<PushPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    if (exception_state.HadException())
      return nullptr;

    // Only "userVisibleOnly" push is supported for now.
    if (!push_permission->userVisibleOnly()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "Push Permission without userVisibleOnly:true isn't supported yet.");
      return nullptr;
    }

    return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
  }
  if (name == "midi") {
    MidiPermissionDescriptor* midi_permission =
        NativeValueTraits<MidiPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    return CreateMidiPermissionDescriptor(midi_permission->sysex());
  }
  if (name == "background-sync")
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_SYNC);
  if (name == "ambient-light-sensor" || name == "accelerometer" ||
      name == "gyroscope" || name == "magnetometer") {
    // ALS requires an extra flag.
    if (name == "ambient-light-sensor") {
      if (!RuntimeEnabledFeatures::SensorExtraClassesEnabled()) {
        exception_state.ThrowTypeError(
            "GenericSensorExtraClasses flag is not enabled.");
        return nullptr;
      }
    }

    return CreatePermissionDescriptor(PermissionName::SENSORS);
  }
  if (name == "accessibility-events") {
    if (!RuntimeEnabledFeatures::AccessibilityObjectModelEnabled()) {
      exception_state.ThrowTypeError(
          "Accessibility Object Model is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::ACCESSIBILITY_EVENTS);
  }
  if (name == "clipboard-read" || name == "clipboard-write") {
    PermissionName permission_name = PermissionName::CLIPBOARD_READ;
    if (name == "clipboard-write")
      permission_name = PermissionName::CLIPBOARD_WRITE;

    ClipboardPermissionDescriptor* clipboard_permission =
        NativeValueTraits<ClipboardPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    return CreateClipboardPermissionDescriptor(
        permission_name, clipboard_permission->allowWithoutGesture(),
        clipboard_permission->allowWithoutSanitization());
  }
  if (name == "payment-handler")
    return CreatePermissionDescriptor(PermissionName::PAYMENT_HANDLER);
  if (name == "background-fetch")
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_FETCH);
  if (name == "idle-detection")
    return CreatePermissionDescriptor(PermissionName::IDLE_DETECTION);
  if (name == "periodic-background-sync")
    return CreatePermissionDescriptor(PermissionName::PERIODIC_BACKGROUND_SYNC);
  if (name == "screen-wake-lock") {
    if (!RuntimeEnabledFeatures::ScreenWakeLockEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("Screen Wake Lock is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::SCREEN_WAKE_LOCK);
  }
  if (name == "system-wake-lock") {
    if (!RuntimeEnabledFeatures::SystemWakeLockEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("System Wake Lock is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::SYSTEM_WAKE_LOCK);
  }
  if (name == "nfc") {
    if (!RuntimeEnabledFeatures::WebNFCEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("Web NFC is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::NFC);
  }
  if (name == "storage-access") {
    if (!RuntimeEnabledFeatures::StorageAccessAPIEnabled()) {
      exception_state.ThrowTypeError("The Storage Access API is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::STORAGE_ACCESS);
  }
  if (name == "window-placement") {
    if (!RuntimeEnabledFeatures::WindowPlacementEnabled()) {
      exception_state.ThrowTypeError("Window Placement is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::WINDOW_PLACEMENT);
  }
  return nullptr;
}

}  // namespace blink
