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
#include "ServiceWorker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/MessagePort.h"
#include "core/events/Event.h"
#include "modules/EventTargetModules.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebServiceWorkerState.h"
#include "public/platform/WebString.h"
#include <v8.h>

namespace blink {

class ServiceWorker::ThenFunction final : public ScriptFunction {
public:
    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ServiceWorker> observer)
    {
        ThenFunction* self = new ThenFunction(scriptState, observer);
        return self->bindToV8Function();
    }

    virtual void trace(Visitor* visitor) override
    {
        visitor->trace(m_observer);
        ScriptFunction::trace(visitor);
    }

private:
    ThenFunction(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ServiceWorker> observer)
        : ScriptFunction(scriptState)
        , m_observer(observer)
    {
    }

    virtual ScriptValue call(ScriptValue value) override
    {
        m_observer->onPromiseResolved();
        return value;
    }

    RefPtrWillBeMember<ServiceWorker> m_observer;
};

const AtomicString& ServiceWorker::interfaceName() const
{
    return EventTargetNames::ServiceWorker;
}

void ServiceWorker::postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& exceptionState)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;
    if (m_outerWorker->state() == WebServiceWorkerStateRedundant) {
        exceptionState.throwDOMException(InvalidStateError, "ServiceWorker is in redundant state.");
        return;
    }

    WebString messageString = message->toWireString();
    OwnPtr<WebMessagePortChannelArray> webChannels = MessagePort::toWebMessagePortChannelArray(channels.release());
    m_outerWorker->postMessage(messageString, webChannels.leakPtr());
}

void ServiceWorker::terminate(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidAccessError, "Not supported.");
}

bool ServiceWorker::isReady()
{
    return m_proxyState == Ready;
}

void ServiceWorker::dispatchStateChangeEvent()
{
    ASSERT(isReady());
    this->dispatchEvent(Event::create(EventTypeNames::statechange));
}

String ServiceWorker::scriptURL() const
{
    return m_outerWorker->url().string();
}

const AtomicString& ServiceWorker::state() const
{
    DEFINE_STATIC_LOCAL(AtomicString, unknown, ("unknown", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, parsed, ("parsed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, installing, ("installing", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, installed, ("installed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, activating, ("activating", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, activated, ("activated", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, redundant, ("redundant", AtomicString::ConstructFromLiteral));

    switch (m_outerWorker->state()) {
    case WebServiceWorkerStateUnknown:
        // The web platform should never see this internal state
        ASSERT_NOT_REACHED();
        return unknown;
    case WebServiceWorkerStateParsed:
        return parsed;
    case WebServiceWorkerStateInstalling:
        return installing;
    case WebServiceWorkerStateInstalled:
        return installed;
    case WebServiceWorkerStateActivating:
        return activating;
    case WebServiceWorkerStateActivated:
        return activated;
    case WebServiceWorkerStateRedundant:
        return redundant;
    default:
        ASSERT_NOT_REACHED();
        return nullAtom;
    }
}

PassRefPtrWillBeRawPtr<ServiceWorker> ServiceWorker::from(ExecutionContext* executionContext, WebType* worker)
{
    if (!worker)
        return nullptr;

    RefPtrWillBeRawPtr<ServiceWorker> serviceWorker = getOrCreate(executionContext, worker);
    if (serviceWorker->m_proxyState == Initial)
        serviceWorker->setProxyState(Ready);
    return serviceWorker.release();
}

PassRefPtrWillBeRawPtr<ServiceWorker> ServiceWorker::take(ScriptPromiseResolver* resolver, WebType* worker)
{
    if (!worker)
        return nullptr;

    RefPtrWillBeRawPtr<ServiceWorker> serviceWorker = getOrCreate(resolver->scriptState()->executionContext(), worker);
    ScriptState::Scope scope(resolver->scriptState());
    if (serviceWorker->m_proxyState == Initial)
        serviceWorker->waitOnPromise(resolver);
    return serviceWorker;
}

void ServiceWorker::dispose(WebType* worker)
{
    if (worker && !worker->proxy())
        delete worker;
}

void ServiceWorker::setProxyState(ProxyState state)
{
    if (m_proxyState == state)
        return;
    switch (m_proxyState) {
    case Initial:
        ASSERT(state == RegisterPromisePending || state == Ready || state == ContextStopped);
        break;
    case RegisterPromisePending:
        ASSERT(state == Ready || state == ContextStopped);
        break;
    case Ready:
        ASSERT(state == ContextStopped);
        break;
    case ContextStopped:
        ASSERT_NOT_REACHED();
        break;
    }

    ProxyState oldState = m_proxyState;
    m_proxyState = state;
    if (oldState == Ready || state == Ready)
        m_outerWorker->proxyReadyChanged();
}

void ServiceWorker::onPromiseResolved()
{
    if (m_proxyState == ContextStopped)
        return;
    setProxyState(Ready);
}

void ServiceWorker::waitOnPromise(ScriptPromiseResolver* resolver)
{
    if (resolver->promise().isEmpty()) {
        // The document was detached during registration. The state doesn't really
        // matter since this ServiceWorker will immediately die.
        setProxyState(ContextStopped);
        return;
    }
    setProxyState(RegisterPromisePending);
    resolver->promise().then(ThenFunction::createFunction(resolver->scriptState(), this));
}

bool ServiceWorker::hasPendingActivity() const
{
    if (AbstractWorker::hasPendingActivity())
        return true;
    if (m_proxyState == ContextStopped)
        return false;
    return m_outerWorker->state() != WebServiceWorkerStateRedundant;
}

void ServiceWorker::stop()
{
    setProxyState(ContextStopped);
}

PassRefPtrWillBeRawPtr<ServiceWorker> ServiceWorker::getOrCreate(ExecutionContext* executionContext, WebType* outerWorker)
{
    if (!outerWorker)
        return nullptr;

    WebServiceWorkerProxy* proxy = outerWorker->proxy();
    ServiceWorker* existingServiceWorker = proxy ? proxy->unwrap() : 0;
    if (existingServiceWorker) {
        ASSERT(existingServiceWorker->executionContext() == executionContext);
        return existingServiceWorker;
    }

    RefPtrWillBeRawPtr<ServiceWorker> worker = adoptRefWillBeNoop(new ServiceWorker(executionContext, adoptPtr(outerWorker)));
    worker->suspendIfNeeded();
    return worker.release();
}

ServiceWorker::ServiceWorker(ExecutionContext* executionContext, PassOwnPtr<WebServiceWorker> worker)
    : AbstractWorker(executionContext)
    , WebServiceWorkerProxy(this)
    , m_outerWorker(worker)
    , m_proxyState(Initial)
{
    ASSERT(m_outerWorker);
    m_outerWorker->setProxy(this);
}

} // namespace blink
