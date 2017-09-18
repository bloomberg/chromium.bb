// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VR.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "platform/feature_policy/FeaturePolicy.h"

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

VR::VR(LocalFrame& frame) : ContextLifecycleObserver(frame.GetDocument()) {}

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

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Until we can return an accurate list of devices arbitrarily resolve with an
  // empty array.
  v8::Local<v8::Array> empty_array =
      v8::Array::New(script_state->GetIsolate(), 0);
  resolver->Resolve(empty_array);

  return promise;
}

void VR::ContextDestroyed(ExecutionContext*) {}

DEFINE_TRACE(VR) {
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
