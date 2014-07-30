// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistration_h
#define ServiceWorkerRegistration_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "public/platform/WebServiceWorkerRegistration.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class WebServiceWorkerProvider;

class ServiceWorkerRegistration FINAL
    : public RefCountedWillBeRefCountedGarbageCollected<ServiceWorkerRegistration>
    , public ActiveDOMObject
    , public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(ServiceWorkerRegistration);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
public:
    virtual ~ServiceWorkerRegistration() { }

    // EventTarget override.
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }

    // For CallbackPromiseAdapter.
    typedef WebServiceWorkerRegistration WebType;
    static PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> take(ScriptPromiseResolver*, WebType* registration);

    String scope() const;

    ScriptPromise unregister(ScriptState*);

private:
    static PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> create(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration>);
    ServiceWorkerRegistration(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration>);

    OwnPtr<WebServiceWorkerRegistration> m_outerRegistration;
    WebServiceWorkerProvider* m_provider;
};

} // namespace blink

#endif // ServiceWorkerRegistration_h
