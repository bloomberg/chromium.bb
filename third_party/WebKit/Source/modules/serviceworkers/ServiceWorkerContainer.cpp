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

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerMessageEvent.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/serviceworker/WebServiceWorker.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {

class RegistrationCallback : public WebServiceWorkerProvider::WebServiceWorkerRegistrationCallbacks {
public:
    explicit RegistrationCallback(ScriptPromiseResolver* resolver)
        : m_resolver(resolver) { }
    ~RegistrationCallback() override { }

    void onSuccess(WebPassOwnPtr<WebServiceWorkerRegistration::Handle> handle) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(ServiceWorkerRegistration::getOrCreate(m_resolver->executionContext(), handle.release()));
    }

    void onError(const WebServiceWorkerError& error) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    WTF_MAKE_NONCOPYABLE(RegistrationCallback);
};

class GetRegistrationCallback : public WebServiceWorkerProvider::WebServiceWorkerGetRegistrationCallbacks {
public:
    explicit GetRegistrationCallback(ScriptPromiseResolver* resolver)
        : m_resolver(resolver) { }
    ~GetRegistrationCallback() override { }

    void onSuccess(WebPassOwnPtr<WebServiceWorkerRegistration::Handle> webPassHandle) override
    {
        OwnPtr<WebServiceWorkerRegistration::Handle> handle = webPassHandle.release();
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        if (!handle) {
            // Resolve the promise with undefined.
            m_resolver->resolve();
            return;
        }
        m_resolver->resolve(ServiceWorkerRegistration::getOrCreate(m_resolver->executionContext(), handle.release()));
    }

    void onError(const WebServiceWorkerError& error) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    WTF_MAKE_NONCOPYABLE(GetRegistrationCallback);
};

class GetRegistrationsCallback : public WebServiceWorkerProvider::WebServiceWorkerGetRegistrationsCallbacks {
public:
    explicit GetRegistrationsCallback(ScriptPromiseResolver* resolver)
        : m_resolver(resolver) { }
    ~GetRegistrationsCallback() override { }

    void onSuccess(WebPassOwnPtr<WebVector<WebServiceWorkerRegistration::Handle*>> webPassRegistrations) override
    {
        Vector<OwnPtr<WebServiceWorkerRegistration::Handle>> handles;
        OwnPtr<WebVector<WebServiceWorkerRegistration::Handle*>> webRegistrations = webPassRegistrations.release();
        for (auto& handle : *webRegistrations) {
            handles.append(adoptPtr(handle));
        }

        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(ServiceWorkerRegistrationArray::take(m_resolver.get(), &handles));
    }

    void onError(const WebServiceWorkerError& error) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    WTF_MAKE_NONCOPYABLE(GetRegistrationsCallback);
};

class ServiceWorkerContainer::GetRegistrationForReadyCallback : public WebServiceWorkerProvider::WebServiceWorkerGetRegistrationForReadyCallbacks {
public:
    explicit GetRegistrationForReadyCallback(ReadyProperty* ready)
        : m_ready(ready) { }
    ~GetRegistrationForReadyCallback() override { }

    void onSuccess(WebPassOwnPtr<WebServiceWorkerRegistration::Handle> handle) override
    {
        ASSERT(m_ready->state() == ReadyProperty::Pending);

        if (m_ready->executionContext() && !m_ready->executionContext()->activeDOMObjectsAreStopped())
            m_ready->resolve(ServiceWorkerRegistration::getOrCreate(m_ready->executionContext(), handle.release()));
    }

private:
    Persistent<ReadyProperty> m_ready;
    WTF_MAKE_NONCOPYABLE(GetRegistrationForReadyCallback);
};

ServiceWorkerContainer* ServiceWorkerContainer::create(ExecutionContext* executionContext)
{
    return new ServiceWorkerContainer(executionContext);
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

DEFINE_TRACE(ServiceWorkerContainer)
{
    visitor->trace(m_controller);
    visitor->trace(m_ready);
    RefCountedGarbageCollectedEventTargetWithInlineData<ServiceWorkerContainer>::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

ScriptPromise ServiceWorkerContainer::registerServiceWorker(ScriptState* scriptState, const String& url, const RegistrationOptions& options)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "Failed to register a ServiceWorker: The document is in an invalid state."));
        return promise;
    }

    ExecutionContext* executionContext = scriptState->executionContext();
    // FIXME: May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    String errorMessage;
    // Restrict to secure origins: https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-privileged
    if (!executionContext->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL pageURL = KURL(KURL(), documentOrigin->toString());
    if (!SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(pageURL.protocol())) {
        resolver->reject(DOMException::create(SecurityError, "Failed to register a ServiceWorker: The URL protocol of the current origin ('" + documentOrigin->toString() + "') is not supported."));
        return promise;
    }

    KURL scriptURL = callingExecutionContext(scriptState->isolate())->completeURL(url);
    scriptURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(scriptURL)) {
        RefPtr<SecurityOrigin> scriptOrigin = SecurityOrigin::create(scriptURL);
        resolver->reject(DOMException::create(SecurityError, "Failed to register a ServiceWorker: The origin of the provided scriptURL ('" + scriptOrigin->toString() + "') does not match the current origin ('" + documentOrigin->toString() + "')."));
        return promise;
    }
    if (!SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(scriptURL.protocol())) {
        resolver->reject(DOMException::create(SecurityError, "Failed to register a ServiceWorker: The URL protocol of the script ('" + scriptURL.string() + "') is not supported."));
        return promise;
    }

    KURL patternURL;
    if (options.scope().isNull())
        patternURL = KURL(scriptURL, "./");
    else
        patternURL = callingExecutionContext(scriptState->isolate())->completeURL(options.scope());
    patternURL.removeFragmentIdentifier();

    if (!documentOrigin->canRequest(patternURL)) {
        RefPtr<SecurityOrigin> patternOrigin = SecurityOrigin::create(patternURL);
        resolver->reject(DOMException::create(SecurityError, "Failed to register a ServiceWorker: The origin of the provided scope ('" + patternOrigin->toString() + "') does not match the current origin ('" + documentOrigin->toString() + "')."));
        return promise;
    }
    if (!SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(patternURL.protocol())) {
        resolver->reject(DOMException::create(SecurityError, "Failed to register a ServiceWorker: The URL protocol of the scope ('" + patternURL.string() + "') is not supported."));
        return promise;
    }

    WebString webErrorMessage;
    if (!m_provider->validateScopeAndScriptURL(patternURL, scriptURL, &webErrorMessage)) {
        resolver->reject(V8ThrowException::createTypeError(scriptState->isolate(), WebString::fromUTF8("Failed to register a ServiceWorker: " + webErrorMessage.utf8())));
        return promise;
    }

    m_provider->registerServiceWorker(patternURL, scriptURL, new RegistrationCallback(resolver));

    return promise;
}

ScriptPromise ServiceWorkerContainer::getRegistration(ScriptState* scriptState, const String& documentURL)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "Failed to get a ServiceWorkerRegistration: The document is in an invalid state."));
        return promise;
    }

    ExecutionContext* executionContext = scriptState->executionContext();
    // FIXME: May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    String errorMessage;
    if (!executionContext->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL pageURL = KURL(KURL(), documentOrigin->toString());
    if (!SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(pageURL.protocol())) {
        resolver->reject(DOMException::create(SecurityError, "Failed to get a ServiceWorkerRegistration: The URL protocol of the current origin ('" + documentOrigin->toString() + "') is not supported."));
        return promise;
    }

    KURL completedURL = callingExecutionContext(scriptState->isolate())->completeURL(documentURL);
    completedURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(completedURL)) {
        RefPtr<SecurityOrigin> documentURLOrigin = SecurityOrigin::create(completedURL);
        resolver->reject(DOMException::create(SecurityError, "Failed to get a ServiceWorkerRegistration: The origin of the provided documentURL ('" + documentURLOrigin->toString() + "') does not match the current origin ('" + documentOrigin->toString() + "')."));
        return promise;
    }
    m_provider->getRegistration(completedURL, new GetRegistrationCallback(resolver));

    return promise;
}

ScriptPromise ServiceWorkerContainer::getRegistrations(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "Failed to get ServiceWorkerRegistration objects: The document is in an invalid state."));
        return promise;
    }

    ExecutionContext* executionContext = scriptState->executionContext();
    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    String errorMessage;
    if (!executionContext->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL pageURL = KURL(KURL(), documentOrigin->toString());
    if (!SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(pageURL.protocol())) {
        resolver->reject(DOMException::create(SecurityError, "Failed to get ServiceWorkerRegistration objects: The URL protocol of the current origin ('" + documentOrigin->toString() + "') is not supported."));
        return promise;
    }

    m_provider->getRegistrations(new GetRegistrationsCallback(resolver));

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

    if (!m_ready) {
        m_ready = createReadyProperty();
        if (m_provider)
            m_provider->getRegistrationForReady(new GetRegistrationForReadyCallback(m_ready.get()));
    }

    return m_ready->promise(callerState->world());
}

void ServiceWorkerContainer::setController(WebPassOwnPtr<WebServiceWorker::Handle> handle, bool shouldNotifyControllerChange)
{
    if (!executionContext())
        return;
    m_controller = ServiceWorker::from(executionContext(), handle.release());
    if (m_controller)
        UseCounter::count(executionContext(), UseCounter::ServiceWorkerControlledPage);
    if (shouldNotifyControllerChange)
        dispatchEvent(Event::create(EventTypeNames::controllerchange));
}

void ServiceWorkerContainer::dispatchMessageEvent(WebPassOwnPtr<WebServiceWorker::Handle> handle, const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    if (!executionContext() || !executionContext()->executingWindow())
        return;

    MessagePortArray* ports = MessagePort::toMessagePortArray(executionContext(), webChannels);
    RefPtr<SerializedScriptValue> value = SerializedScriptValueFactory::instance().createFromWire(message);
    ServiceWorker* source = ServiceWorker::from(executionContext(), handle.release());
    dispatchEvent(ServiceWorkerMessageEvent::create(ports, value, source, executionContext()->securityOrigin()->toString()));
}

const AtomicString& ServiceWorkerContainer::interfaceName() const
{
    return EventTargetNames::ServiceWorkerContainer;
}

ServiceWorkerContainer::ServiceWorkerContainer(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
    , m_provider(0)
{

    if (!executionContext)
        return;

    if (ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(executionContext)) {
        m_provider = client->provider();
        if (m_provider)
            m_provider->setClient(this);
    }
}

} // namespace blink
