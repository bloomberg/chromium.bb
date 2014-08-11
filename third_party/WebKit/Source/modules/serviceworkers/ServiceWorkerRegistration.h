// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistration_h
#define ServiceWorkerRegistration_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "public/platform/WebServiceWorkerRegistration.h"
#include "public/platform/WebServiceWorkerRegistrationProxy.h"
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
    , public EventTargetWithInlineData
    , public WebServiceWorkerRegistrationProxy {
    REFCOUNTED_EVENT_TARGET(ServiceWorkerRegistration);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
public:
    virtual ~ServiceWorkerRegistration() { }

    // EventTarget overrides.
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }

    // WebServiceWorkerRegistrationProxy overrides.
    virtual void dispatchUpdateFoundEvent() OVERRIDE;
    virtual void setInstalling(WebServiceWorker*) OVERRIDE;
    virtual void setWaiting(WebServiceWorker*) OVERRIDE;
    virtual void setActive(WebServiceWorker*) OVERRIDE;

    // For CallbackPromiseAdapter.
    typedef WebServiceWorkerRegistration WebType;
    static PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> take(ScriptPromiseResolver*, WebType* registration);
    static void dispose(WebType* registration);

    PassRefPtrWillBeRawPtr<ServiceWorker> installing() { return m_installing.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> waiting() { return m_waiting.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> active() { return m_active.get(); }

    String scope() const;

    ScriptPromise unregister(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound);

private:
    static PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> create(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration>);
    ServiceWorkerRegistration(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration>);

    OwnPtr<WebServiceWorkerRegistration> m_outerRegistration;
    WebServiceWorkerProvider* m_provider;
    RefPtrWillBeMember<ServiceWorker> m_installing;
    RefPtrWillBeMember<ServiceWorker> m_waiting;
    RefPtrWillBeMember<ServiceWorker> m_active;
};

} // namespace blink

#endif // ServiceWorkerRegistration_h
