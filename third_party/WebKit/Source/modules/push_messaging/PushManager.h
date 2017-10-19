// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushManager_h
#define PushManager_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class PushSubscriptionOptionsInit;
class ScriptPromise;
class ScriptState;
class ServiceWorkerRegistration;

class MODULES_EXPORT PushManager final : public GarbageCollected<PushManager>,
                                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushManager* Create(ServiceWorkerRegistration* registration) {
    return new PushManager(registration);
  }

  // Web-exposed property:
  static Vector<String> supportedContentEncodings();

  // Web-exposed methods:
  ScriptPromise subscribe(ScriptState*,
                          const PushSubscriptionOptionsInit&,
                          ExceptionState&);
  ScriptPromise getSubscription(ScriptState*);
  ScriptPromise permissionState(ScriptState*,
                                const PushSubscriptionOptionsInit&,
                                ExceptionState&);

  void Trace(blink::Visitor*);

 private:
  explicit PushManager(ServiceWorkerRegistration*);

  Member<ServiceWorkerRegistration> registration_;
};

}  // namespace blink

#endif  // PushManager_h
