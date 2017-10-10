// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRSession.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/vr/latest/VR.h"
#include "modules/vr/latest/VRDevice.h"
#include "modules/vr/latest/VRFrameOfReference.h"
#include "modules/vr/latest/VRFrameOfReferenceOptions.h"
#include "modules/vr/latest/VRSessionEvent.h"

namespace blink {

namespace {

const char kSessionEnded[] = "VRSession has already ended.";

const char kUnknownFrameOfReference[] = "Unknown frame of reference type";

const char kNonEmulatedStageNotSupported[] =
    "Non-emulated 'stage' frame of reference not yet supported";

}  // namespace

VRSession::VRSession(VRDevice* device, bool exclusive)
    : device_(device), exclusive_(exclusive) {}

void VRSession::setDepthNear(double value) {
  depth_near_ = value;
}

void VRSession::setDepthFar(double value) {
  depth_far_ = value;
}

ExecutionContext* VRSession::GetExecutionContext() const {
  return device_->GetExecutionContext();
}

const AtomicString& VRSession::InterfaceName() const {
  return EventTargetNames::VRSession;
}

ScriptPromise VRSession::requestFrameOfReference(
    ScriptState* script_state,
    const String& type,
    const VRFrameOfReferenceOptions& options) {
  if (detached_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, kSessionEnded));
  }

  VRFrameOfReference* frameOfRef = nullptr;
  if (type == "headModel") {
    frameOfRef =
        new VRFrameOfReference(this, VRFrameOfReference::kTypeHeadModel);
  } else if (type == "eyeLevel") {
    frameOfRef =
        new VRFrameOfReference(this, VRFrameOfReference::kTypeEyeLevel);
  } else if (type == "stage") {
    if (!options.disableStageEmulation()) {
      frameOfRef = new VRFrameOfReference(this, VRFrameOfReference::kTypeStage);
      frameOfRef->UseEmulatedHeight(options.stageEmulationHeight());
    } else {
      // TODO(bajones): Support native stages using the standing transform.
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(kNotSupportedError,
                                             kNonEmulatedStageNotSupported));
    }
  }

  if (!frameOfRef) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotSupportedError, kUnknownFrameOfReference));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(frameOfRef);

  return promise;
}

ScriptPromise VRSession::end(ScriptState* script_state) {
  // Don't allow a session to end twice.
  if (detached_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, kSessionEnded));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(bajones): If there's any work that needs to be done asynchronously on
  // session end it should be completed before this promise is resolved.

  ForceEnd();

  resolver->Resolve();
  return promise;
}

void VRSession::ForceEnd() {
  // Detach this session from the device.
  detached_ = true;

  DispatchEvent(VRSessionEvent::Create(EventTypeNames::end, this));
}

void VRSession::OnFocus() {
  if (!blurred_)
    return;

  blurred_ = false;
  DispatchEvent(VRSessionEvent::Create(EventTypeNames::focus, this));
}

void VRSession::OnBlur() {
  if (blurred_)
    return;

  blurred_ = true;
  DispatchEvent(VRSessionEvent::Create(EventTypeNames::blur, this));
}

DEFINE_TRACE(VRSession) {
  visitor->Trace(device_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
