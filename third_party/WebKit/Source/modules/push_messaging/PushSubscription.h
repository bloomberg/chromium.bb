// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushSubscription_h
#define PushSubscription_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/DOMTimeStamp.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class PushSubscriptionOptions;
class ServiceWorkerRegistration;
class ScriptPromiseResolver;
class ScriptState;
struct WebPushSubscription;

class PushSubscription final
    : public GarbageCollectedFinalized<PushSubscription>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushSubscription* Take(ScriptPromiseResolver*,
                                std::unique_ptr<WebPushSubscription>,
                                ServiceWorkerRegistration*);
  static void Dispose(WebPushSubscription* subscription_raw);

  virtual ~PushSubscription();

  KURL endpoint() const { return endpoint_; }
  DOMTimeStamp expirationTime(bool& out_is_null) const;

  PushSubscriptionOptions* options() const { return options_.Get(); }

  DOMArrayBuffer* getKey(const AtomicString& name) const;
  ScriptPromise unsubscribe(ScriptState*);

  ScriptValue toJSONForBinding(ScriptState*);

  DECLARE_TRACE();

 private:
  PushSubscription(const WebPushSubscription&, ServiceWorkerRegistration*);

  KURL endpoint_;

  Member<PushSubscriptionOptions> options_;

  Member<DOMArrayBuffer> p256dh_;
  Member<DOMArrayBuffer> auth_;

  Member<ServiceWorkerRegistration> service_worker_registration_;
};

}  // namespace blink

#endif  // PushSubscription_h
