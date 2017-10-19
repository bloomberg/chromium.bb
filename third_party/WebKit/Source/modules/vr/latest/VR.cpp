// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VR.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/vr/latest/VRDevice.h"
#include "modules/vr/latest/VRDeviceEvent.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "public/platform/InterfaceProvider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

namespace {

const char kNavigatorDetachedError[] =
    "The navigator.vr object is no longer associated with a document.";

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"vr\" is disallowed by feature policy.";

const char kCrossOriginSubframeBlocked[] =
    "Blocked call to navigator.vr.getDevices inside a cross-origin "
    "iframe because the frame has never been activated by the user.";

}  // namespace

VR::VR(LocalFrame& frame)
    : ContextLifecycleObserver(frame.GetDocument()),
      devices_synced_(false),
      binding_(this) {
  frame.GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      ConvertToBaseCallback(WTF::Bind(&VR::Dispose, WrapWeakPersistent(this))));

  device::mojom::blink::VRServiceClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));

  // Setting the client kicks off a request for the details of any connected
  // VRDevices.
  service_->SetClient(std::move(client),
                      ConvertToBaseCallback(WTF::Bind(&VR::OnDevicesSynced,
                                                      WrapPersistent(this))));
}

ExecutionContext* VR::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& VR::InterfaceName() const {
  return EventTargetNames::VR;
}

ScriptPromise VR::getDevices(ScriptState* script_state) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    // Reject if the frame is inaccessible.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kNavigatorDetachedError));
  }

  if (IsSupportedInFeaturePolicy(WebFeaturePolicyFeature::kWebVr)) {
    if (!frame->IsFeatureEnabled(WebFeaturePolicyFeature::kWebVr)) {
      // Only allow the call to be made if the appropraite feature policy is in
      // place.
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kSecurityError, kFeaturePolicyBlocked));
    }
  } else if (!frame->HasReceivedUserGesture() &&
             frame->IsCrossOriginSubframe()) {
    // Block calls from cross-origin iframes that have never had a user gesture.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kSecurityError, kCrossOriginSubframeBlocked));
  }

  // If we're still waiting for a previous call to resolve return that promise
  // again.
  if (pending_devices_resolver_) {
    return pending_devices_resolver_->Promise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If we've previously synced the VRDevices or no longer have a valid service
  // connection just return the current list. In the case of the service being
  // disconnected this will be an empty array.
  if (!service_ || devices_synced_) {
    resolver->Resolve(devices_);
    return promise;
  }

  // Otherwise wait for the full list of devices to be populated and resolve
  // when onDevicesSynced is called.
  pending_devices_resolver_ = resolver;

  return promise;
}

// Each time a new VRDevice is connected we'll recieve a VRDisplayPtr for it
// here. Upon calling SetClient in the constructor we should receive one call
// for each VRDevice that was already connected at the time.
void VR::OnDisplayConnected(
    device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider,
    device::mojom::blink::VRDisplayHostPtr display,
    device::mojom::blink::VRDisplayClientRequest client_request,
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  VRDevice* vr_device =
      new VRDevice(this, std::move(magic_window_provider), std::move(display),
                   std::move(client_request), std::move(display_info));

  devices_.push_back(vr_device);

  DispatchEvent(
      VRDeviceEvent::Create(EventTypeNames::deviceconnect, vr_device));
}

// Called when the VRService has called OnDevicesConnected for all active
// VRDevices.
void VR::OnDevicesSynced() {
  devices_synced_ = true;
  OnGetDevices();
}

// Called when details for every connected VRDevice has been received.
void VR::OnGetDevices() {
  if (pending_devices_resolver_) {
    pending_devices_resolver_->Resolve(devices_);
    pending_devices_resolver_ = nullptr;
  }
}

void VR::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void VR::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  binding_.Close();

  // Shutdown all devices' message pipe
  for (const auto& device : devices_)
    device->Dispose();

  devices_.clear();

  // Ensure that any outstanding getDevices promises are resolved. They will
  // receive an empty array of devices.
  OnGetDevices();
}

void VR::Trace(blink::Visitor* visitor) {
  visitor->Trace(devices_);
  visitor->Trace(pending_devices_resolver_);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
