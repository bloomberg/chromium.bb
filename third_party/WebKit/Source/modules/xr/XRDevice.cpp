// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRDevice.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/xr/XR.h"
#include "modules/xr/XRFrameProvider.h"
#include "modules/xr/XRSession.h"

namespace blink {

namespace {

const char kActiveExclusiveSession[] =
    "XRDevice already has an active, exclusive session";

const char kExclusiveNotSupported[] =
    "XRDevice does not support the creation of exclusive sessions.";

const char kNonExclusiveNotSupported[] =
    "XRDevice does not support the creation of non-exclusive sessions.";

const char kRequestNotInUserGesture[] =
    "Exclusive sessions can only be requested during a user gesture.";

}  // namespace

XRDevice::XRDevice(
    XR* xr,
    device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider,
    device::mojom::blink::VRDisplayHostPtr display,
    device::mojom::blink::VRDisplayClientRequest client_request,
    device::mojom::blink::VRDisplayInfoPtr display_info)
    : xr_(xr),
      magic_window_provider_(std::move(magic_window_provider)),
      display_(std::move(display)),
      display_client_binding_(this, std::move(client_request)) {
  SetXRDisplayInfo(std::move(display_info));
}

ExecutionContext* XRDevice::GetExecutionContext() const {
  return xr_->GetExecutionContext();
}

const AtomicString& XRDevice::InterfaceName() const {
  return EventTargetNames::XRDevice;
}

const char* XRDevice::checkSessionSupport(
    const XRSessionCreationOptions& options) const {
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

ScriptPromise XRDevice::supportsSession(
    ScriptState* script_state,
    const XRSessionCreationOptions& options) const {
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

ScriptPromise XRDevice::requestSession(
    ScriptState* script_state,
    const XRSessionCreationOptions& options) {
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
    if (frameProvider()->exclusive_session()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kInvalidStateError, kActiveExclusiveSession));
    }

    Document* doc = ToDocumentOrNull(ExecutionContext::From(script_state));
    if (!Frame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr)) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kInvalidStateError, kRequestNotInUserGesture));
    }
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  XRSession* session = new XRSession(this, options.exclusive());

  if (options.exclusive()) {
    frameProvider()->BeginExclusiveSession(session, resolver);
  } else {
    resolver->Resolve(session);
  }

  return promise;
}

// TODO: Forward these calls on to the sessions once they've been implemented.
void XRDevice::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  SetXRDisplayInfo(std::move(display_info));
}
void XRDevice::OnExitPresent() {}
void XRDevice::OnBlur() {}
void XRDevice::OnFocus() {}
void XRDevice::OnActivate(device::mojom::blink::VRDisplayEventReason,
                          OnActivateCallback on_handled) {}
void XRDevice::OnDeactivate(device::mojom::blink::VRDisplayEventReason) {}

XRFrameProvider* XRDevice::frameProvider() {
  if (!frame_provider_)
    frame_provider_ = new XRFrameProvider(this);

  return frame_provider_;
}

void XRDevice::Dispose() {
  display_client_binding_.Close();
}

void XRDevice::SetXRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  display_info_ = std::move(display_info);
  is_external_ = display_info_->capabilities->hasExternalDisplay;
  supports_exclusive_ = (display_info_->capabilities->canPresent);
}

void XRDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(frame_provider_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
