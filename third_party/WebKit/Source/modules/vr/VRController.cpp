// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRController.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "platform/wtf/Assertions.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

VRController::VRController(NavigatorVR* navigator_vr)
    : ContextLifecycleObserver(navigator_vr->GetDocument()),
      navigator_vr_(navigator_vr),
      display_synced_(false),
      binding_(this) {
  navigator_vr->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(ConvertToBaseCallback(
      WTF::Bind(&VRController::Dispose, WrapWeakPersistent(this))));

  device::mojom::blink::VRServiceClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  service_->SetClient(std::move(client), ConvertToBaseCallback(WTF::Bind(
                                             &VRController::OnDisplaysSynced,
                                             WrapPersistent(this))));
}

VRController::~VRController() {}

void VRController::GetDisplays(ScriptPromiseResolver* resolver) {
  // If we've previously synced the VRDisplays or no longer have a valid service
  // connection just return the current list. In the case of the service being
  // disconnected this will be an empty array.
  if (!service_ || display_synced_) {
    resolver->Resolve(displays_);
    return;
  }

  // Otherwise we're still waiting for the full list of displays to be populated
  // so queue up the promise for resolution when onDisplaysSynced is called.
  pending_get_devices_callbacks_.push_back(
      WTF::MakeUnique<VRGetDevicesCallback>(resolver));
}

void VRController::SetListeningForActivate(bool listening) {
  if (service_)
    service_->SetListeningForActivate(listening);
}

// Each time a new VRDisplay is connected we'll receive a VRDisplayPtr for it
// here. Upon calling SetClient in the constructor we should receive one call
// for each VRDisplay that was already connected at the time.
void VRController::OnDisplayConnected(
    device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider,
    device::mojom::blink::VRDisplayHostPtr display,
    device::mojom::blink::VRDisplayClientRequest request,
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  VRDisplay* vr_display =
      new VRDisplay(navigator_vr_, std::move(magic_window_provider),
                    std::move(display), std::move(request));
  vr_display->Update(display_info);
  vr_display->OnConnected();
  vr_display->FocusChanged();
  displays_.push_back(vr_display);
}

void VRController::FocusChanged() {
  for (const auto& display : displays_)
    display->FocusChanged();
}

// Called when the VRService has called OnDisplayConnected for all active
// VRDisplays.
void VRController::OnDisplaysSynced() {
  display_synced_ = true;
  OnGetDisplays();
}

void VRController::OnGetDisplays() {
  while (!pending_get_devices_callbacks_.IsEmpty()) {
    std::unique_ptr<VRGetDevicesCallback> callback =
        pending_get_devices_callbacks_.TakeFirst();
    callback->OnSuccess(displays_);
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
  for (const auto& display : displays_)
    display->Dispose();

  displays_.clear();

  // Ensure that any outstanding getDisplays promises are resolved.
  OnGetDisplays();
}

void VRController::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_vr_);
  visitor->Trace(displays_);

  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
