// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ServiceWorkerRegistration.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "public/platform/WebServiceWorkerProvider.h"

namespace blink {

class UndefinedValue {
public:
#ifdef DISABLE_SERVICE_WORKER_REGISTRATION
    typedef WebServiceWorker WebType;
#else
    typedef WebServiceWorkerRegistration WebType;
#endif
    static V8UndefinedType take(ScriptPromiseResolver* resolver, WebType* registration)
    {
        ASSERT(!registration); // Anything passed here will be leaked.
        return V8UndefinedType();
    }
    static void dispose(WebType* registration)
    {
        ASSERT(!registration); // Anything passed here will be leaked.
    }

private:
    UndefinedValue();
};

static void deleteIfNoExistingOwner(WebServiceWorker* serviceWorker)
{
    if (serviceWorker && !serviceWorker->proxy())
        delete serviceWorker;
}

const AtomicString& ServiceWorkerRegistration::interfaceName() const
{
    return EventTargetNames::ServiceWorkerRegistration;
}

void ServiceWorkerRegistration::dispatchUpdateFoundEvent()
{
    dispatchEvent(Event::create(EventTypeNames::updatefound));
}

void ServiceWorkerRegistration::setInstalling(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_installing = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerRegistration::setWaiting(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_waiting = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerRegistration::setActive(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_active = ServiceWorker::from(executionContext(), serviceWorker);
}

PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> ServiceWorkerRegistration::take(ScriptPromiseResolver* resolver, WebType* registration)
{
    if (!registration)
        return nullptr;
    return getOrCreate(resolver->scriptState()->executionContext(), registration);
}

void ServiceWorkerRegistration::dispose(WebType* registration)
{
    delete registration;
}

String ServiceWorkerRegistration::scope() const
{
    return m_outerRegistration->scope().string();
}

ScriptPromise ServiceWorkerRegistration::unregister(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "No associated provider is available"));
        return promise;
    }

    RefPtr<SecurityOrigin> documentOrigin = scriptState->executionContext()->securityOrigin();
    KURL scopeURL = scriptState->executionContext()->completeURL(scope());
    scopeURL.removeFragmentIdentifier();
    if (!scope().isEmpty() && !documentOrigin->canRequest(scopeURL)) {
        resolver->reject(DOMException::create(SecurityError, "Can only unregister for scopes in the document's origin."));
        return promise;
    }

    m_provider->unregisterServiceWorker(scopeURL, new CallbackPromiseAdapter<UndefinedValue, ServiceWorkerError>(resolver));
    return promise;
}

PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> ServiceWorkerRegistration::getOrCreate(ExecutionContext* executionContext, WebServiceWorkerRegistration* outerRegistration)
{
    if (!outerRegistration)
        return nullptr;

    WebServiceWorkerRegistrationProxy* proxy = outerRegistration->proxy();
    if (proxy) {
        ServiceWorkerRegistration* existingRegistration = *proxy;
        if (existingRegistration) {
            ASSERT(existingRegistration->executionContext() == executionContext);
            return existingRegistration;
        }
    }

    RefPtrWillBeRawPtr<ServiceWorkerRegistration> registration = adoptRefWillBeRefCountedGarbageCollected(new ServiceWorkerRegistration(executionContext, adoptPtr(outerRegistration)));
    registration->suspendIfNeeded();
    return registration.release();
}

ServiceWorkerRegistration::ServiceWorkerRegistration(ExecutionContext* executionContext, PassOwnPtr<WebServiceWorkerRegistration> outerRegistration)
    : ActiveDOMObject(executionContext)
    , WebServiceWorkerRegistrationProxy(this)
    , m_outerRegistration(outerRegistration)
    , m_provider(0)
    , m_stopped(false)
{
    ASSERT(m_outerRegistration);
    ScriptWrappable::init(this);

    if (!executionContext)
        return;
    if (ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(executionContext))
        m_provider = client->provider();
    m_outerRegistration->setProxy(this);
}

void ServiceWorkerRegistration::trace(Visitor* visitor)
{
    visitor->trace(m_installing);
    visitor->trace(m_waiting);
    visitor->trace(m_active);
    EventTargetWithInlineData::trace(visitor);
}

bool ServiceWorkerRegistration::hasPendingActivity() const
{
    return !m_stopped;
}

void ServiceWorkerRegistration::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_outerRegistration->proxyStopped();
}

} // namespace blink
