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

class ServiceWorkerRegistration final
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<ServiceWorkerRegistration>
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , public WebServiceWorkerRegistrationProxy {
    DEFINE_WRAPPERTYPEINFO();
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<ServiceWorkerRegistration>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
public:
    // EventTarget overrides.
    virtual const AtomicString& interfaceName() const override;
    virtual ExecutionContext* executionContext() const override { return ActiveDOMObject::executionContext(); }

    // WebServiceWorkerRegistrationProxy overrides.
    virtual void dispatchUpdateFoundEvent() override;
    virtual void setInstalling(WebServiceWorker*) override;
    virtual void setWaiting(WebServiceWorker*) override;
    virtual void setActive(WebServiceWorker*) override;

    // For CallbackPromiseAdapter.
    typedef WebServiceWorkerRegistration WebType;
    static ServiceWorkerRegistration* from(ExecutionContext*, WebType* registration);
    static ServiceWorkerRegistration* take(ScriptPromiseResolver*, WebType* registration);
    static void dispose(WebType* registration);

    PassRefPtrWillBeRawPtr<ServiceWorker> installing() { return m_installing.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> waiting() { return m_waiting.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> active() { return m_active.get(); }

    String scope() const;

    ScriptPromise unregister(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound);

    virtual void trace(Visitor*) override;

private:
    static ServiceWorkerRegistration* getOrCreate(ExecutionContext*, WebServiceWorkerRegistration*);
    ServiceWorkerRegistration(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration>);

    // ActiveDOMObject overrides.
    virtual bool hasPendingActivity() const override;
    virtual void stop() override;

    OwnPtr<WebServiceWorkerRegistration> m_outerRegistration;
    WebServiceWorkerProvider* m_provider;
    RefPtrWillBeMember<ServiceWorker> m_installing;
    RefPtrWillBeMember<ServiceWorker> m_waiting;
    RefPtrWillBeMember<ServiceWorker> m_active;

    bool m_stopped;
};

} // namespace blink

#endif // ServiceWorkerRegistration_h
