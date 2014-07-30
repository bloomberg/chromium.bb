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

const AtomicString& ServiceWorkerRegistration::interfaceName() const
{
    return EventTargetNames::ServiceWorkerRegistration;
}

PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> ServiceWorkerRegistration::take(ScriptPromiseResolver* resolver, WebType* registration)
{
    if (!registration)
        return nullptr;
    return create(resolver->scriptState()->executionContext(), adoptPtr(registration));
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

PassRefPtrWillBeRawPtr<ServiceWorkerRegistration> ServiceWorkerRegistration::create(ExecutionContext* executionContext, PassOwnPtr<WebServiceWorkerRegistration> outerRegistration)
{
    RefPtrWillBeRawPtr<ServiceWorkerRegistration> registration = adoptRefWillBeRefCountedGarbageCollected(new ServiceWorkerRegistration(executionContext, outerRegistration));
    registration->suspendIfNeeded();
    return registration.release();
}

ServiceWorkerRegistration::ServiceWorkerRegistration(ExecutionContext* executionContext, PassOwnPtr<WebServiceWorkerRegistration> outerRegistration)
    : ActiveDOMObject(executionContext)
    , m_outerRegistration(outerRegistration)
    , m_provider(0)
{
    ASSERT(m_outerRegistration);
    ScriptWrappable::init(this);

    if (!executionContext)
        return;
    if (ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(executionContext))
        m_provider = client->provider();
}

} // namespace blink
