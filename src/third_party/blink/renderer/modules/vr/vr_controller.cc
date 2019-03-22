// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/vr_controller.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/vr/navigator_vr.h"
#include "third_party/blink/renderer/modules/vr/vr_get_devices_callback.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

VRController::VRController(NavigatorVR* navigator_vr)
    : ContextLifecycleObserver(navigator_vr->GetDocument()),
      navigator_vr_(navigator_vr),
      display_synced_(false),
      binding_(this) {
  navigator_vr->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&VRController::Dispose, WrapWeakPersistent(this)));

  device::mojom::blink::VRServiceClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  service_->SetClient(std::move(client));

  service_->RequestDevice(
      WTF::Bind(&VRController::OnRequestDeviceReturned, WrapPersistent(this)));
}

VRController::~VRController() = default;

void VRController::GetDisplays(ScriptPromiseResolver* resolver) {
  // If we've previously synced the VRDisplays or no longer have a valid service
  // connection just return the current list. In the case of the service being
  // disconnected this will be an empty array.
  if (!service_ || display_synced_) {
    LogGetDisplayResult();
    HeapVector<Member<VRDisplay>> displays;
    if (display_)
      displays.push_back(display_);
    resolver->Resolve(displays);
    return;
  }

  // Otherwise we're still waiting for the full list of displays to be populated
  // so queue up the promise for resolution when onDisplaysSynced is called.
  pending_get_devices_callbacks_.push_back(
      std::make_unique<VRGetDevicesCallback>(resolver));
}

void VRController::SetListeningForActivate(bool listening) {
  if (!service_ || !display_) {
    pending_listening_for_activate_ = listening;
    return;
  }

  if (listening_for_activate_ && listening) {
    // We're already listening so leave things as is.
    return;
  }

  listening_for_activate_ = listening;

  if (listening) {
    service_->SetListeningForActivate(display_->GetDisplayClient());
  } else {
    service_->SetListeningForActivate(nullptr);
  }
}

// Called when the XRDevice has been initialized.
void VRController::OnRequestDeviceReturned(
    device::mojom::blink::XRDevicePtr device) {
  if (!device) {
    // There are no devices connected to the system. We can't do any VR, at all.
    OnGetDisplays();
    return;
  }

  device->GetImmersiveVRDisplayInfo(WTF::Bind(
      &VRController::OnImmersiveDisplayInfoReturned, WrapPersistent(this)));

  display_ = MakeGarbageCollected<VRDisplay>(navigator_vr_, std::move(device));

  if (pending_listening_for_activate_) {
    SetListeningForActivate(pending_listening_for_activate_);
    pending_listening_for_activate_ = false;
  }
}

void VRController::OnNewDeviceReturned(
    device::mojom::blink::XRDevicePtr device) {
  if (device) {
    display_->OnConnected();
  }
  OnRequestDeviceReturned(std::move(device));
}

void VRController::OnDeviceChanged() {
  if (!display_ && !display_synced_) {
    // We're already underway checking if there is a device.
    return;
  }

  display_synced_ = false;

  if (!display_) {
    service_->RequestDevice(
        WTF::Bind(&VRController::OnNewDeviceReturned, WrapPersistent(this)));
  } else if (!display_->canPresent()) {
    // If we can't present, see if that's changed.
    display_->device()->GetImmersiveVRDisplayInfo(WTF::Bind(
        &VRController::OnImmersiveDisplayInfoReturned, WrapPersistent(this)));
  } else {
    display_synced_ = true;
  }
}

void VRController::FocusChanged() {
  if (display_)
    display_->FocusChanged();
}

void VRController::OnImmersiveDisplayInfoReturned(
    device::mojom::blink::VRDisplayInfoPtr info) {
  if (!display_) {
    // We must have been disposed and are shutting down.
    return;
  }

  has_presentation_capable_display_ = info ? true : false;

  if (info) {
    display_->OnChanged(std::move(info), true /* is_immersive */);
    display_->OnConnected();
    has_display_ = true;
  }
  display_->FocusChanged();

  display_synced_ = true;
  OnGetDisplays();
}

void VRController::LogGetDisplayResult() {
  Document* doc = navigator_vr_->GetDocument();
  if (has_display_ && doc && doc->IsInMainFrame()) {
    ukm::builders::XR_WebXR ukm_builder(doc->UkmSourceID());
    ukm_builder.SetReturnedDevice(1);
    if (has_presentation_capable_display_) {
      ukm_builder.SetReturnedPresentationCapableDevice(1);
    }
    ukm_builder.Record(doc->UkmRecorder());
  }
}

void VRController::OnGetDisplays() {
  while (!pending_get_devices_callbacks_.IsEmpty()) {
    LogGetDisplayResult();

    HeapVector<Member<VRDisplay>> displays;
    if (display_)
      displays.push_back(display_);

    std::unique_ptr<VRGetDevicesCallback> callback =
        pending_get_devices_callbacks_.TakeFirst();
    callback->OnSuccess(displays);
  }
}

void VRController::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void VRController::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  binding_.Close();

  // Shutdown all displays' message pipe
  if (display_) {
    display_->Dispose();
    display_ = nullptr;
  }

  // Ensure that any outstanding getDisplays promises are resolved.
  OnGetDisplays();
}

void VRController::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_vr_);
  visitor->Trace(display_);

  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
