// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistration_h
#define ServiceWorkerRegistration_h

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class ScriptPromise;
class ScriptState;
class WebServiceWorkerProvider;

// The implementation of a service worker registration object in Blink. Actual
// registration representation is in the embedder and this class accesses it
// via WebServiceWorkerRegistration::Handle object.
class ServiceWorkerRegistration final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<ServiceWorkerRegistration>
    , public ActiveDOMObject
    , public WebServiceWorkerRegistrationProxy
    , public HeapSupplementable<ServiceWorkerRegistration> {
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(ServiceWorkerRegistration);
    USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
public:
    // EventTarget overrides.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override { return ActiveDOMObject::executionContext(); }

    // WebServiceWorkerRegistrationProxy overrides.
    void dispatchUpdateFoundEvent() override;
    void setInstalling(WebPassOwnPtr<WebServiceWorker::Handle>) override;
    void setWaiting(WebPassOwnPtr<WebServiceWorker::Handle>) override;
    void setActive(WebPassOwnPtr<WebServiceWorker::Handle>) override;

    // Returns an existing registration object for the handle if it exists.
    // Otherwise, returns a new registration object.
    static ServiceWorkerRegistration* getOrCreate(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration::Handle>);

    ServiceWorker* installing() { return m_installing; }
    ServiceWorker* waiting() { return m_waiting; }
    ServiceWorker* active() { return m_active; }

    String scope() const;

    WebServiceWorkerRegistration* webRegistration() { return m_handle->registration(); }

    ScriptPromise update(ScriptState*);
    ScriptPromise unregister(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound);

    ~ServiceWorkerRegistration() override;

    // Eager finalization needed to promptly release owned WebServiceWorkerRegistration.
    EAGERLY_FINALIZE();
    DECLARE_VIRTUAL_TRACE();

private:
    ServiceWorkerRegistration(ExecutionContext*, PassOwnPtr<WebServiceWorkerRegistration::Handle>);

    // ActiveDOMObject overrides.
    bool hasPendingActivity() const override;
    void stop() override;

    // A handle to the registration representation in the embedder.
    OwnPtr<WebServiceWorkerRegistration::Handle> m_handle;

    WebServiceWorkerProvider* m_provider;
    Member<ServiceWorker> m_installing;
    Member<ServiceWorker> m_waiting;
    Member<ServiceWorker> m_active;

    bool m_stopped;
};

class ServiceWorkerRegistrationArray {
    STATIC_ONLY(ServiceWorkerRegistrationArray);
public:
    static HeapVector<Member<ServiceWorkerRegistration>> take(ScriptPromiseResolver* resolver, Vector<OwnPtr<WebServiceWorkerRegistration::Handle>>* webServiceWorkerRegistrations)
    {
        HeapVector<Member<ServiceWorkerRegistration>> registrations;
        for (auto& registration : *webServiceWorkerRegistrations)
            registrations.append(ServiceWorkerRegistration::getOrCreate(resolver->executionContext(), registration.release()));
        return registrations;
    }
};

} // namespace blink

#endif // ServiceWorkerRegistration_h
