// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushManager.h"

#include <memory>

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "modules/push_messaging/PushController.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushPermissionStatusCallbacks.h"
#include "modules/push_messaging/PushSubscription.h"
#include "modules/push_messaging/PushSubscriptionCallbacks.h"
#include "modules/push_messaging/PushSubscriptionOptions.h"
#include "modules/push_messaging/PushSubscriptionOptionsInit.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/push_messaging/WebPushClient.h"
#include "public/platform/modules/push_messaging/WebPushProvider.h"
#include "public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"

namespace blink {
namespace {

WebPushProvider* PushProvider() {
  WebPushProvider* web_push_provider = Platform::Current()->PushProvider();
  DCHECK(web_push_provider);
  return web_push_provider;
}

}  // namespace

PushManager::PushManager(ServiceWorkerRegistration* registration)
    : registration_(registration) {
  DCHECK(registration);
}

// static
Vector<String> PushManager::supportedContentEncodings() {
  return Vector<String>({"aes128gcm", "aesgcm"});
}

ScriptPromise PushManager::subscribe(ScriptState* script_state,
                                     const PushSubscriptionOptionsInit& options,
                                     ExceptionState& exception_state) {
  if (!registration_->active())
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kAbortError,
                             "Subscription failed - no active Service Worker"));

  const WebPushSubscriptionOptions& web_options =
      PushSubscriptionOptions::ToWeb(options, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // The document context is the only reasonable context from which to ask the
  // user for permission to use the Push API. The embedder should persist the
  // permission so that later calls in different contexts can succeed.
  if (ExecutionContext::From(script_state)->IsDocument()) {
    Document* document = ToDocument(ExecutionContext::From(script_state));
    if (!document->domWindow() || !document->GetFrame())
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kInvalidStateError,
                               "Document is detached from window."));
    PushController::ClientFrom(document->GetFrame())
        .Subscribe(registration_->WebRegistration(), web_options,
                   UserGestureIndicator::ProcessingUserGestureThreadSafe(),
                   std::make_unique<PushSubscriptionCallbacks>(resolver,
                                                               registration_));
  } else {
    PushProvider()->Subscribe(
        registration_->WebRegistration(), web_options,
        UserGestureIndicator::ProcessingUserGestureThreadSafe(),
        std::make_unique<PushSubscriptionCallbacks>(resolver, registration_));
  }

  return promise;
}

ScriptPromise PushManager::getSubscription(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  PushProvider()->GetSubscription(
      registration_->WebRegistration(),
      std::make_unique<PushSubscriptionCallbacks>(resolver, registration_));
  return promise;
}

ScriptPromise PushManager::permissionState(
    ScriptState* script_state,
    const PushSubscriptionOptionsInit& options,
    ExceptionState& exception_state) {
  if (ExecutionContext::From(script_state)->IsDocument()) {
    Document* document = ToDocument(ExecutionContext::From(script_state));
    if (!document->domWindow() || !document->GetFrame())
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kInvalidStateError,
                               "Document is detached from window."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  PushProvider()->GetPermissionStatus(
      registration_->WebRegistration(),
      PushSubscriptionOptions::ToWeb(options, exception_state),
      std::make_unique<PushPermissionStatusCallbacks>(resolver));
  return promise;
}

void PushManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
}

}  // namespace blink
