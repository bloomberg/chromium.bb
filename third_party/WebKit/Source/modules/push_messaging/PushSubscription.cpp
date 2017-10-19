// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushSubscription.h"

#include <memory>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushSubscriptionOptions.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/Base64.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/push_messaging/WebPushProvider.h"
#include "public/platform/modules/push_messaging/WebPushSubscription.h"

namespace blink {

PushSubscription* PushSubscription::Take(
    ScriptPromiseResolver*,
    std::unique_ptr<WebPushSubscription> push_subscription,
    ServiceWorkerRegistration* service_worker_registration) {
  if (!push_subscription)
    return nullptr;
  return new PushSubscription(*push_subscription, service_worker_registration);
}

void PushSubscription::Dispose(WebPushSubscription* push_subscription) {
  if (push_subscription)
    delete push_subscription;
}

PushSubscription::PushSubscription(
    const WebPushSubscription& subscription,
    ServiceWorkerRegistration* service_worker_registration)
    : endpoint_(subscription.endpoint),
      options_(PushSubscriptionOptions::Create(subscription.options)),
      p256dh_(DOMArrayBuffer::Create(subscription.p256dh.Data(),
                                     subscription.p256dh.size())),
      auth_(DOMArrayBuffer::Create(subscription.auth.Data(),
                                   subscription.auth.size())),
      service_worker_registration_(service_worker_registration) {}

PushSubscription::~PushSubscription() {}

DOMTimeStamp PushSubscription::expirationTime(bool& out_is_null) const {
  // This attribute reflects the time at which the subscription will expire,
  // which is not relevant to this implementation yet as subscription refreshes
  // are not supported.
  out_is_null = true;

  return 0;
}

DOMArrayBuffer* PushSubscription::getKey(const AtomicString& name) const {
  if (name == "p256dh")
    return p256dh_;
  if (name == "auth")
    return auth_;

  return nullptr;
}

ScriptPromise PushSubscription::unsubscribe(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  WebPushProvider* web_push_provider = Platform::Current()->PushProvider();
  DCHECK(web_push_provider);

  web_push_provider->Unsubscribe(
      service_worker_registration_->WebRegistration(),
      std::make_unique<CallbackPromiseAdapter<bool, PushError>>(resolver));
  return promise;
}

ScriptValue PushSubscription::toJSONForBinding(ScriptState* script_state) {
  DCHECK(p256dh_);

  V8ObjectBuilder result(script_state);
  result.AddString("endpoint", endpoint());
  result.AddNull("expirationTime");

  V8ObjectBuilder keys(script_state);
  keys.Add("p256dh",
           WTF::Base64URLEncode(static_cast<const char*>(p256dh_->Data()),
                                p256dh_->ByteLength()));
  keys.Add("auth", WTF::Base64URLEncode(static_cast<const char*>(auth_->Data()),
                                        auth_->ByteLength()));
  result.Add("keys", keys);

  return result.GetScriptValue();
}

void PushSubscription::Trace(blink::Visitor* visitor) {
  visitor->Trace(options_);
  visitor->Trace(p256dh_);
  visitor->Trace(auth_);
  visitor->Trace(service_worker_registration_);
}

}  // namespace blink
