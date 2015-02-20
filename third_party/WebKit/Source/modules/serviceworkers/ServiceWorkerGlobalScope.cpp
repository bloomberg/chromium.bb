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
#include "ServiceWorkerGlobalScope.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/events/Event.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/ThreadableLoader.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/EventTargetModules.h"
#include "modules/fetch/GlobalFetch.h"
#include "modules/serviceworkers/CacheStorage.h"
#include "modules/serviceworkers/InspectorServiceWorkerCacheAgent.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "modules/serviceworkers/ServiceWorkerScriptCachedMetadataHandler.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebServiceWorkerSkipWaitingCallbacks.h"
#include "public/platform/WebURL.h"
#include "wtf/CurrentTime.h"

namespace blink {

class ServiceWorkerGlobalScope::SkipWaitingCallback final : public WebServiceWorkerSkipWaitingCallbacks {
    WTF_MAKE_NONCOPYABLE(SkipWaitingCallback);
public:
    explicit SkipWaitingCallback(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver) { }
    ~SkipWaitingCallback() { }

    void onSuccess() override
    {
        m_resolver->resolve();
    }

private:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
};

PassRefPtrWillBeRawPtr<ServiceWorkerGlobalScope> ServiceWorkerGlobalScope::create(ServiceWorkerThread* thread, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
{
    RefPtrWillBeRawPtr<ServiceWorkerGlobalScope> context = adoptRefWillBeNoop(new ServiceWorkerGlobalScope(startupData->m_scriptURL, startupData->m_userAgent, thread, monotonicallyIncreasingTime(), startupData->m_starterOrigin, startupData->m_workerClients.release()));

    context->setV8CacheOptions(startupData->m_v8CacheOptions);
    context->applyContentSecurityPolicyFromString(startupData->m_contentSecurityPolicy, startupData->m_contentSecurityPolicyType);

    return context.release();
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(const KURL& url, const String& userAgent, ServiceWorkerThread* thread, double timeOrigin, const SecurityOrigin* starterOrigin, PassOwnPtrWillBeRawPtr<WorkerClients> workerClients)
    : WorkerGlobalScope(url, userAgent, thread, timeOrigin, starterOrigin, workerClients)
    , m_didEvaluateScript(false)
    , m_hadErrorInTopLevelEventHandler(false)
    , m_eventNestingLevel(0)
{
    workerInspectorController()->registerModuleAgent(InspectorServiceWorkerCacheAgent::create(this));
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope()
{
}

void ServiceWorkerGlobalScope::didEvaluateWorkerScript()
{
    m_didEvaluateScript = true;
}

CacheStorage* ServiceWorkerGlobalScope::caches(ExecutionContext* context)
{
    if (!m_caches)
        m_caches = CacheStorage::create(ServiceWorkerGlobalScopeClient::from(context)->cacheStorage());
    return m_caches;
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* scriptState, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    return GlobalFetch::fetch(scriptState, *this, input, init, exceptionState);
}

ServiceWorkerClients* ServiceWorkerGlobalScope::clients()
{
    if (!m_clients)
        m_clients = ServiceWorkerClients::create();
    return m_clients;
}

ServiceWorkerRegistration* ServiceWorkerGlobalScope::registration()
{
    return m_registration;
}

void ServiceWorkerGlobalScope::close(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidAccessError, "Not supported.");
}

ScriptPromise ServiceWorkerGlobalScope::skipWaiting(ScriptState* scriptState)
{
    ExecutionContext* executionContext = scriptState->executionContext();
    // FIXME: short-term fix, see details at: https://codereview.chromium.org/535193002/.
    if (!executionContext)
        return ScriptPromise();

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    ServiceWorkerGlobalScopeClient::from(executionContext)->skipWaiting(new SkipWaitingCallback(resolver));
    return promise;
}

void ServiceWorkerGlobalScope::setRegistration(WebServiceWorkerRegistration* registration)
{
    if (!executionContext()) {
        ServiceWorkerRegistration::dispose(registration);
        return;
    }
    m_registration = ServiceWorkerRegistration::from(executionContext(), registration);
}

bool ServiceWorkerGlobalScope::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (m_didEvaluateScript) {
        if (eventType == EventTypeNames::install) {
            RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Event handler of 'install' event must be added on the initial evaluation of worker script.");
            addMessageToWorkerConsole(consoleMessage.release());
        } else if (eventType == EventTypeNames::activate) {
            RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Event handler of 'activate' event must be added on the initial evaluation of worker script.");
            addMessageToWorkerConsole(consoleMessage.release());
        }
    }
    return WorkerGlobalScope::addEventListener(eventType, listener, useCapture);
}

const AtomicString& ServiceWorkerGlobalScope::interfaceName() const
{
    return EventTargetNames::ServiceWorkerGlobalScope;
}

bool ServiceWorkerGlobalScope::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    m_eventNestingLevel++;
    bool result = WorkerGlobalScope::dispatchEvent(event.get());
    if (event->interfaceName() == EventNames::ErrorEvent && m_eventNestingLevel == 2 && !event->defaultPrevented())
        m_hadErrorInTopLevelEventHandler = true;
    m_eventNestingLevel--;
    return result;
}

void ServiceWorkerGlobalScope::dispatchExtendableEvent(PassRefPtrWillBeRawPtr<Event> event, WaitUntilObserver* observer)
{
    ASSERT(m_eventNestingLevel == 0);
    m_hadErrorInTopLevelEventHandler = false;

    observer->willDispatchEvent();
    dispatchEvent(event);
    if (thread()->terminated())
        m_hadErrorInTopLevelEventHandler = true;
    observer->didDispatchEvent(m_hadErrorInTopLevelEventHandler);
}

DEFINE_TRACE(ServiceWorkerGlobalScope)
{
    visitor->trace(m_clients);
    visitor->trace(m_registration);
    visitor->trace(m_caches);
    WorkerGlobalScope::trace(visitor);
}

void ServiceWorkerGlobalScope::importScripts(const Vector<String>& urls, ExceptionState& exceptionState)
{
    // Bust the MemoryCache to ensure script requests reach the browser-side
    // and get added to and retrieved from the ServiceWorker's script cache.
    // FIXME: Revisit in light of the solution to crbug/388375.
    for (Vector<String>::const_iterator it = urls.begin(); it != urls.end(); ++it)
        MemoryCache::removeURLFromCache(this->executionContext(), completeURL(*it));
    WorkerGlobalScope::importScripts(urls, exceptionState);
}

PassOwnPtr<CachedMetadataHandler> ServiceWorkerGlobalScope::createWorkerScriptCachedMetadataHandler(const KURL& scriptURL, const Vector<char>* metaData)
{
    return ServiceWorkerScriptCachedMetadataHandler::create(this, scriptURL, metaData);
}

void ServiceWorkerGlobalScope::logExceptionToConsole(const String& errorMessage, int scriptId, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    WorkerGlobalScope::logExceptionToConsole(errorMessage, scriptId, sourceURL, lineNumber, columnNumber, callStack);
    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, errorMessage, sourceURL, lineNumber);
    consoleMessage->setScriptId(scriptId);
    consoleMessage->setCallStack(callStack);
    addMessageToWorkerConsole(consoleMessage.release());
}

} // namespace blink
