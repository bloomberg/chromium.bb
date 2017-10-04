// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRDevice.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/UserGestureIndicator.h"
#include "modules/EventTargetModules.h"
#include "modules/vr/latest/VR.h"
#include "modules/vr/latest/VRSession.h"

namespace blink {

namespace {

const char kExclusiveNotSupported[] =
    "VRDevice does not support the creation of exclusive sessions.";

const char kNonExclusiveNotSupported[] =
    "VRDevice does not support the creation of non-exclusive sessions.";

const char kRequestNotInUserGesture[] =
    "Exclusive sessions can only be requested during a user gesture.";

}  // namespace

VRDevice::VRDevice(
    VR* vr,
    device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider,
    device::mojom::blink::VRDisplayHostPtr display,
    device::mojom::blink::VRDisplayClientRequest client_request,
    device::mojom::blink::VRDisplayInfoPtr display_info)
    : vr_(vr),
      magic_window_provider_(std::move(magic_window_provider)),
      display_(std::move(display)),
      display_client_binding_(this, std::move(client_request)) {
  SetVRDisplayInfo(std::move(display_info));
}

ExecutionContext* VRDevice::GetExecutionContext() const {
  return vr_->GetExecutionContext();
}

const AtomicString& VRDevice::InterfaceName() const {
  return EventTargetNames::VRDevice;
}

const char* VRDevice::checkSessionSupport(
    const VRSessionCreationOptions& options) const {
  if (options.exclusive()) {
    // Validation for exclusive sessions.
    if (!supports_exclusive_) {
      return kExclusiveNotSupported;
    }
  } else {
    // Validation for non-exclusive sessions.
    // TODO: Add support for non-exclusive sessions in a follow up CL.
    return kNonExclusiveNotSupported;
  }

  return nullptr;
}

ScriptPromise VRDevice::supportsSession(
    ScriptState* script_state,
    const VRSessionCreationOptions& options) const {
  // Check to see if the device is capable of supporting the requested session
  // options. Note that reporting support here does not guarantee that creating
  // a session with those options will succeed, as other external and
  // time-sensitve factors (focus state, existance of another exclusive session,
  // etc.) may prevent the creation of a session as well.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError, reject_reason));
  }

  // If the above checks pass, resolve without a value. Future API iterations
  // may specify a value to be returned here.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve();
  return promise;
}

ScriptPromise VRDevice::requestSession(
    ScriptState* script_state,
    const VRSessionCreationOptions& options) {
  // Check first to see if the device is capable of supporting the requested
  // options.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError, reject_reason));
  }

  // Check if the current page state prevents the requested session from being
  // created.
  if (options.exclusive()) {
    if (!UserGestureIndicator::ProcessingUserGesture()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kInvalidStateError, kRequestNotInUserGesture));
    }
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  VRSession* session = new VRSession(this, options.exclusive());

  // TODO: A follow up CL will establish a VRPresentationProvider connection
  // before resolving the promise.
  resolver->Resolve(session);

  return promise;
}

// TODO: Forward these calls on to the sessions once they've been implemented.
void VRDevice::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  SetVRDisplayInfo(std::move(display_info));
}
void VRDevice::OnExitPresent() {}
void VRDevice::OnBlur() {}
void VRDevice::OnFocus() {}
void VRDevice::OnActivate(device::mojom::blink::VRDisplayEventReason,
                          OnActivateCallback on_handled) {}
void VRDevice::OnDeactivate(device::mojom::blink::VRDisplayEventReason) {}

void VRDevice::Dispose() {
  display_client_binding_.Close();
}

void VRDevice::SetVRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  display_info_ = std::move(display_info);
  device_name_ = display_info_->displayName;
  is_external_ = display_info_->capabilities->hasExternalDisplay;
  supports_exclusive_ = (display_info_->capabilities->canPresent);
}

DEFINE_TRACE(VRDevice) {
  visitor->Trace(vr_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
