/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/events/MessageEvent.h"
#include "modules/serviceworkers/RegistrationOptionList.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebServiceWorker.h"
#include "public/platform/WebServiceWorkerProvider.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

using blink::WebServiceWorker;
using blink::WebServiceWorkerProvider;

namespace blink {

PassRefPtrWillBeRawPtr<ServiceWorkerContainer> ServiceWorkerContainer::create(ExecutionContext* executionContext)
{
    return adoptRefWillBeNoop(new ServiceWorkerContainer(executionContext));
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
    ASSERT(!m_provider);
}

void ServiceWorkerContainer::willBeDetachedFromFrame()
{
    if (m_provider) {
        m_provider->setClient(0);
        m_provider = nullptr;
    }
}

void ServiceWorkerContainer::trace(Visitor* visitor)
{
    visitor->trace(m_active);
    visitor->trace(m_controller);
    visitor->trace(m_installing);
    visitor->trace(m_waiting);
    visitor->trace(m_ready);
}

ScriptPromise ServiceWorkerContainer::registerServiceWorker(ScriptState* scriptState, const String& url, const Dictionary& dictionary)
{
    RegistrationOptionList options(dictionary);
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "No associated provider is available"));
        return promise;
    }

    // FIXME: This should use the container's execution context, not
    // the callers.
    ExecutionContext* executionContext = scriptState->executionContext();
    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    String errorMessage;
    if (!documentOrigin->canAccessFeatureRequiringSecureOrigin(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL patternURL = executionContext->completeURL(options.scope);
    patternURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(patternURL)) {
        resolver->reject(DOMException::create(SecurityError, "The scope must match the current origin."));
        return promise;
    }

    KURL scriptURL = executionContext->completeURL(url);
    scriptURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(scriptURL)) {
        resolver->reject(DOMException::create(SecurityError, "The origin of the script must match the current origin."));
        return promise;
    }

    m_provider->registerServiceWorker(patternURL, scriptURL, new CallbackPromiseAdapter<ServiceWorkerRegistration, ServiceWorkerError>(resolver));

    return promise;
}

class UndefinedValue {
public:
    typedef WebServiceWorkerRegistration WebType;
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

ScriptPromise ServiceWorkerContainer::unregisterServiceWorker(ScriptState* scriptState, const String& pattern)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "No associated provider is available"));
        return promise;
    }

    // FIXME: This should use the container's execution context, not
    // the callers.
    RefPtr<SecurityOrigin> documentOrigin = scriptState->executionContext()->securityOrigin();
    String errorMessage;
    if (!documentOrigin->canAccessFeatureRequiringSecureOrigin(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL patternURL = scriptState->executionContext()->completeURL(pattern);
    patternURL.removeFragmentIdentifier();
    if (!pattern.isEmpty() && !documentOrigin->canRequest(patternURL)) {
        resolver->reject(DOMException::create(SecurityError, "The scope must match the current origin."));
        return promise;
    }

    m_provider->unregisterServiceWorker(patternURL, new CallbackPromiseAdapter<UndefinedValue, ServiceWorkerError>(resolver));
    return promise;
}

ServiceWorkerContainer::ReadyProperty* ServiceWorkerContainer::createReadyProperty()
{
    return new ReadyProperty(executionContext(), this, ReadyProperty::Ready);
}

ScriptPromise ServiceWorkerContainer::ready(ScriptState* callerState)
{
    if (!executionContext())
        return ScriptPromise();

    if (!callerState->world().isMainWorld()) {
        // FIXME: Support .ready from isolated worlds when
        // ScriptPromiseProperty can vend Promises in isolated worlds.
        return ScriptPromise::rejectWithDOMException(callerState, DOMException::create(NotSupportedError, "'ready' is only supported in pages."));
    }

    return m_ready->promise(callerState->world());
}

// If the WebServiceWorker is up for adoption (does not have a
// WebServiceWorkerProxy owner), rejects the adoption by deleting the
// WebServiceWorker.
static void deleteIfNoExistingOwner(WebServiceWorker* serviceWorker)
{
    if (serviceWorker && !serviceWorker->proxy())
        delete serviceWorker;
}

void ServiceWorkerContainer::setActive(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    RefPtrWillBeRawPtr<ServiceWorker> previousReadyWorker = m_active;
    m_active = ServiceWorker::from(executionContext(), serviceWorker);
    checkReadyChanged(previousReadyWorker.release());
}

void ServiceWorkerContainer::checkReadyChanged(PassRefPtrWillBeRawPtr<ServiceWorker> previousReadyWorker)
{
    ServiceWorker* currentReadyWorker = m_active.get();

    if (previousReadyWorker == currentReadyWorker)
        return;

    if (m_ready->state() != ReadyProperty::Pending) {
        // Already resolved Promises are now stale because the
        // ready worker changed
        m_ready = createReadyProperty();
    }

    if (currentReadyWorker)
        m_ready->resolve(currentReadyWorker);
}

void ServiceWorkerContainer::setController(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_controller = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerContainer::setInstalling(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_installing = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerContainer::setWaiting(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_waiting = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerContainer::dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    if (!executionContext() || !executionContext()->executingWindow())
        return;

    OwnPtrWillBeRawPtr<MessagePortArray> ports = MessagePort::toMessagePortArray(executionContext(), webChannels);
    RefPtr<SerializedScriptValue> value = SerializedScriptValue::createFromWire(message);
    executionContext()->executingWindow()->dispatchEvent(MessageEvent::create(ports.release(), value));
}

ServiceWorkerContainer::ServiceWorkerContainer(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
    , m_provider(0)
{
    ScriptWrappable::init(this);

    if (!executionContext)
        return;

    m_ready = createReadyProperty();

    if (ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(executionContext)) {
        m_provider = client->provider();
        if (m_provider)
            m_provider->setClient(this);
    }
}

} // namespace blink
