/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/workers/Worker.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/events/EventListener.h"
#include "core/events/EventNames.h"
#include "core/events/MessageEvent.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/DOMWindow.h"
#include "core/page/UseCounter.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "wtf/MainThread.h"

namespace WebCore {

inline Worker::Worker(ScriptExecutionContext* context)
    : AbstractWorker(context)
    , m_contextProxy(WorkerGlobalScopeProxy::create(this))
{
    ScriptWrappable::init(this);
}

PassRefPtr<Worker> Worker::create(ScriptExecutionContext* context, const String& url, ExceptionState& es)
{
    ASSERT(isMainThread());
    UseCounter::count(toDocument(context)->domWindow(), UseCounter::WorkerStart);

    RefPtr<Worker> worker = adoptRef(new Worker(context));

    worker->suspendIfNeeded();

    KURL scriptURL = worker->resolveURL(url, es);
    if (scriptURL.isEmpty())
        return 0;

    // The worker context does not exist while loading, so we must ensure that the worker object is not collected, nor are its event listeners.
    worker->setPendingActivity(worker.get());

    worker->m_scriptLoader = WorkerScriptLoader::create();
    worker->m_scriptLoader->loadAsynchronously(context, scriptURL, DenyCrossOriginRequests, worker.get());

    return worker.release();
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    ASSERT(scriptExecutionContext()); // The context is protected by worker context proxy, so it cannot be destroyed while a Worker exists.
    m_contextProxy->workerObjectDestroyed();
}

const AtomicString& Worker::interfaceName() const
{
    return eventNames().interfaceForWorker;
}

void Worker::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& es)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, es);
    if (es.hadException())
        return;
    m_contextProxy->postMessageToWorkerGlobalScope(message, channels.release());
}

void Worker::terminate()
{
    m_contextProxy->terminateWorkerGlobalScope();
}

bool Worker::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a worker, so pages with workers can not go into page cache.
    return false;
}

void Worker::stop()
{
    terminate();
}

bool Worker::hasPendingActivity() const
{
    return m_contextProxy->hasPendingActivity() || ActiveDOMObject::hasPendingActivity();
}

void Worker::didReceiveResponse(unsigned long identifier, const ResourceResponse&)
{
    InspectorInstrumentation::didReceiveScriptResponse(scriptExecutionContext(), identifier);
}

void Worker::notifyFinished()
{
    if (m_scriptLoader->failed()) {
        dispatchEvent(Event::createCancelable(eventNames().errorEvent));
    } else {
        WorkerThreadStartMode startMode = DontPauseWorkerGlobalScopeOnStart;
        if (InspectorInstrumentation::shouldPauseDedicatedWorkerOnStart(scriptExecutionContext()))
            startMode = PauseWorkerGlobalScopeOnStart;
        m_contextProxy->startWorkerGlobalScope(m_scriptLoader->url(), scriptExecutionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script(), startMode);
        InspectorInstrumentation::scriptImported(scriptExecutionContext(), m_scriptLoader->identifier(), m_scriptLoader->script());
    }
    m_scriptLoader = nullptr;

    unsetPendingActivity(this);
}

} // namespace WebCore
