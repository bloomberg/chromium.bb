// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/NavigatorVR.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "modules/vr/VRPose.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

void RejectNavigatorDetached(ScriptPromiseResolver* resolver) {
  DOMException* exception = DOMException::Create(
      kInvalidStateError,
      "The object is no longer associated with a document.");
  resolver->Reject(exception);
}

}  // namespace

NavigatorVR* NavigatorVR::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorVR& NavigatorVR::From(Navigator& navigator) {
  NavigatorVR* supplement = static_cast<NavigatorVR*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorVR(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* script_state,
                                         Navigator& navigator) {
  if (!navigator.GetFrame()) {
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::Create(script_state);
    ScriptPromise promise = resolver->Promise();
    RejectNavigatorDetached(resolver);
    return promise;
  }
  return NavigatorVR::From(navigator).getVRDisplays(script_state);
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!GetDocument()) {
    RejectNavigatorDetached(resolver);
    return promise;
  }

  LocalFrame* frame = GetDocument()->GetFrame();
  // TODO(bshe): Add different error string for cases when promise is rejected.
  if (!frame) {
    RejectNavigatorDetached(resolver);
    return promise;
  }
  if (IsSupportedInFeaturePolicy(WebFeaturePolicyFeature::kWebVr)) {
    if (!frame->IsFeatureEnabled(WebFeaturePolicyFeature::kWebVr)) {
      RejectNavigatorDetached(resolver);
      return promise;
    }
  } else if (!frame->HasReceivedUserGesture() &&
             frame->IsCrossOriginSubframe()) {
    RejectNavigatorDetached(resolver);
    return promise;
  }

  UseCounter::Count(*GetDocument(), WebFeature::kVRGetDisplays);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context->IsSecureContext())
    UseCounter::Count(*GetDocument(), WebFeature::kVRGetDisplaysInsecureOrigin);

  Platform::Current()->RecordRapporURL("VR.WebVR.GetDisplays",
                                       GetDocument()->Url());

  Controller()->GetDisplays(resolver);

  return promise;
}

VRController* NavigatorVR::Controller() {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  if (!controller_) {
    controller_ = new VRController(this);
    controller_->SetListeningForActivate(focused_ && listening_for_activate_);
    controller_->FocusChanged();
  }

  return controller_;
}

Document* NavigatorVR::GetDocument() {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  return GetSupplementable()->GetFrame()->GetDocument();
}

DEFINE_TRACE(NavigatorVR) {
  visitor->Trace(controller_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorVR::NavigatorVR(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      FocusChangedObserver(navigator.GetFrame()->GetPage()) {
  navigator.GetFrame()->DomWindow()->RegisterEventListenerObserver(this);
  FocusedFrameChanged();
}

NavigatorVR::~NavigatorVR() {}

const char* NavigatorVR::SupplementName() {
  return "NavigatorVR";
}

void NavigatorVR::EnqueueVREvent(VRDisplayEvent* event) {
  if (!GetSupplementable()->GetFrame())
    return;

  GetSupplementable()->GetFrame()->DomWindow()->EnqueueWindowEvent(event);
}

void NavigatorVR::DispatchVREvent(VRDisplayEvent* event) {
  if (!(GetSupplementable()->GetFrame()))
    return;

  LocalDOMWindow* window = GetSupplementable()->GetFrame()->DomWindow();
  DCHECK(window);
  event->SetTarget(window);
  window->DispatchEvent(event);
}

void NavigatorVR::FocusedFrameChanged() {
  bool focused = IsFrameFocused(GetSupplementable()->GetFrame());
  if (focused == focused_)
    return;
  focused_ = focused;
  if (controller_) {
    controller_->SetListeningForActivate(listening_for_activate_ && focused);
    controller_->FocusChanged();
  }
}

void NavigatorVR::DidAddEventListener(LocalDOMWindow* window,
                                      const AtomicString& event_type) {
  if (event_type == EventTypeNames::vrdisplayactivate) {
    listening_for_activate_ = true;
    Controller()->SetListeningForActivate(focused_);
  } else if (event_type == EventTypeNames::vrdisplayconnect) {
    // If the page is listening for connection events make sure we've created a
    // controller so that we'll be notified of new devices.
    Controller();
  }
}

void NavigatorVR::DidRemoveEventListener(LocalDOMWindow* window,
                                         const AtomicString& event_type) {
  if (event_type == EventTypeNames::vrdisplayactivate &&
      !window->HasEventListeners(EventTypeNames::vrdisplayactivate)) {
    listening_for_activate_ = false;
    Controller()->SetListeningForActivate(false);
  }
}

void NavigatorVR::DidRemoveAllEventListeners(LocalDOMWindow* window) {
  if (!controller_)
    return;

  controller_->SetListeningForActivate(false);
  listening_for_activate_ = false;
}

}  // namespace blink
