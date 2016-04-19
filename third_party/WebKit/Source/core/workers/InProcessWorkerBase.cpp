// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/InProcessWorkerBase.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/events/MessageEvent.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/InProcessWorkerGlobalScopeProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"

namespace blink {

InProcessWorkerBase::InProcessWorkerBase(ExecutionContext* context)
    : AbstractWorker(context)
    , ActiveScriptWrappable(this)
    , m_contextProxy(nullptr)
{
}

InProcessWorkerBase::~InProcessWorkerBase()
{
    ASSERT(isMainThread());
    if (!m_contextProxy)
        return;
    m_contextProxy->workerObjectDestroyed();
}

void InProcessWorkerBase::postMessage(ExecutionContext* context, PassRefPtr<SerializedScriptValue> message, const MessagePortArray& ports, ExceptionState& exceptionState)
{
    ASSERT(m_contextProxy);
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(context, ports, exceptionState);
    if (exceptionState.hadException())
        return;
    m_contextProxy->postMessageToWorkerGlobalScope(message, channels.release());
}

bool InProcessWorkerBase::initialize(ExecutionContext* context, const String& url, ExceptionState& exceptionState)
{
    suspendIfNeeded();

    KURL scriptURL = resolveURL(url, exceptionState);
    if (scriptURL.isEmpty())
        return false;

    m_scriptLoader = WorkerScriptLoader::create();
    m_scriptLoader->loadAsynchronously(
        *context,
        scriptURL,
        DenyCrossOriginRequests,
        context->securityContext().addressSpace(),
        bind(&InProcessWorkerBase::onResponse, this),
        bind(&InProcessWorkerBase::onFinished, this));

    m_contextProxy = createInProcessWorkerGlobalScopeProxy(context);

    return true;
}

void InProcessWorkerBase::terminate()
{
    if (m_contextProxy)
        m_contextProxy->terminateWorkerGlobalScope();
}

void InProcessWorkerBase::stop()
{
    terminate();
}

bool InProcessWorkerBase::hasPendingActivity() const
{
    // The worker context does not exist while loading, so we must ensure that the worker object is not collected, nor are its event listeners.
    return (m_contextProxy && m_contextProxy->hasPendingActivity()) || m_scriptLoader;
}

ContentSecurityPolicy* InProcessWorkerBase::contentSecurityPolicy()
{
    if (m_scriptLoader)
        return m_scriptLoader->contentSecurityPolicy();
    return m_contentSecurityPolicy.get();
}

void InProcessWorkerBase::onResponse()
{
    InspectorInstrumentation::didReceiveScriptResponse(getExecutionContext(), m_scriptLoader->identifier());
}

void InProcessWorkerBase::onFinished()
{
    if (m_scriptLoader->failed()) {
        dispatchEvent(Event::createCancelable(EventTypeNames::error));
    } else {
        ASSERT(m_contextProxy);
        m_contextProxy->startWorkerGlobalScope(m_scriptLoader->url(), getExecutionContext()->userAgent(), m_scriptLoader->script());
        InspectorInstrumentation::scriptImported(getExecutionContext(), m_scriptLoader->identifier(), m_scriptLoader->script());
    }
    m_contentSecurityPolicy = m_scriptLoader->releaseContentSecurityPolicy();
    m_scriptLoader = nullptr;
}

DEFINE_TRACE(InProcessWorkerBase)
{
    visitor->trace(m_contentSecurityPolicy);
    AbstractWorker::trace(visitor);
}

} // namespace blink
