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

#include "EventTargetNames.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/NewScriptState.h"
#include "core/dom/MessagePort.h"
#include "core/events/Event.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebServiceWorkerState.h"
#include "public/platform/WebString.h"

namespace WebCore {

const AtomicString& ServiceWorker::interfaceName() const
{
    return EventTargetNames::ServiceWorker;
}

void ServiceWorker::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& exceptionState)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;

    blink::WebString messageString = message->toWireString();
    OwnPtr<blink::WebMessagePortChannelArray> webChannels = MessagePort::toWebMessagePortChannelArray(channels.release());
    m_outerWorker->postMessage(messageString, webChannels.leakPtr());
}

void ServiceWorker::dispatchStateChangeEvent()
{
    this->dispatchEvent(Event::create(EventTypeNames::statechange));
}

const AtomicString& ServiceWorker::state() const
{
    DEFINE_STATIC_LOCAL(AtomicString, unknown, ("unknown", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, parsed, ("parsed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, installing, ("installing", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, installed, ("installed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, activating, ("activating", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, active, ("active", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, deactivated, ("deactivated", AtomicString::ConstructFromLiteral));

    switch (m_outerWorker->state()) {
    case blink::WebServiceWorkerStateUnknown:
        // The web platform should never see this internal state
        ASSERT_NOT_REACHED();
        return unknown;
    case blink::WebServiceWorkerStateParsed:
        return parsed;
    case blink::WebServiceWorkerStateInstalling:
        return installing;
    case blink::WebServiceWorkerStateInstalled:
        return installed;
    case blink::WebServiceWorkerStateActivating:
        return activating;
    case blink::WebServiceWorkerStateActive:
        return active;
    case blink::WebServiceWorkerStateDeactivated:
        return deactivated;
    default:
        ASSERT_NOT_REACHED();
        return nullAtom;
    }
}

PassRefPtr<ServiceWorker> ServiceWorker::from(NewScriptState* scriptState, WebType* worker)
{
    return create(scriptState->executionContext(), adoptPtr(worker));
}

PassRefPtr<ServiceWorker> ServiceWorker::create(ExecutionContext* executionContext, PassOwnPtr<blink::WebServiceWorker> outerWorker)
{
    RefPtr<ServiceWorker> worker = adoptRef(new ServiceWorker(executionContext, outerWorker));
    worker->suspendIfNeeded();
    return worker.release();
}

ServiceWorker::ServiceWorker(ExecutionContext* executionContext, PassOwnPtr<blink::WebServiceWorker> worker)
    : AbstractWorker(executionContext)
    , m_outerWorker(worker)
{
    ScriptWrappable::init(this);
    ASSERT(m_outerWorker);
    m_outerWorker->setProxy(this);
}

} // namespace WebCore
