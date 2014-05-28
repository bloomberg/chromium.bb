/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#include "modules/websockets/WorkerThreadableWebSocketChannel.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerRunLoop.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/MainThreadWebSocketChannel.h"
#include "modules/websockets/NewWebSocketChannelImpl.h"
#include "modules/websockets/ThreadableWebSocketChannelClientWrapper.h"
#include "public/platform/Platform.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"

namespace WebCore {

// Created and destroyed on the worker thread. All setters of this class are
// called on the main thread, while all getters are called on the worker
// thread. signalWorkerThread() must be called before any getters are called.
class ThreadableWebSocketChannelSyncHelper {
public:
    static PassOwnPtr<ThreadableWebSocketChannelSyncHelper> create(PassOwnPtr<blink::WebWaitableEvent> event)
    {
        return adoptPtr(new ThreadableWebSocketChannelSyncHelper(event));
    }

    // All setters are called on the main thread.
    void setConnectRequestResult(bool connectRequestResult)
    {
        m_connectRequestResult = connectRequestResult;
    }
    void setSendRequestResult(WebSocketChannel::SendResult sendRequestResult)
    {
        m_sendRequestResult = sendRequestResult;
    }
    void setBufferedAmount(unsigned long bufferedAmount)
    {
        m_bufferedAmount = bufferedAmount;
    }

    // All getter are called on the worker thread.
    bool connectRequestResult() const
    {
        return m_connectRequestResult;
    }
    WebSocketChannel::SendResult sendRequestResult() const
    {
        return m_sendRequestResult;
    }
    unsigned long bufferedAmount() const
    {
        return m_bufferedAmount;
    }

    // This should be called after all setters are called and before any
    // getters are called.
    void signalWorkerThread()
    {
        m_event->signal();
    }

    blink::WebWaitableEvent* event() const
    {
        return m_event.get();
    }

private:
    ThreadableWebSocketChannelSyncHelper(PassOwnPtr<blink::WebWaitableEvent> event)
        : m_event(event)
        , m_connectRequestResult(false)
        , m_sendRequestResult(WebSocketChannel::SendFail)
        , m_bufferedAmount(0)
    {
    }

    OwnPtr<blink::WebWaitableEvent> m_event;
    bool m_connectRequestResult;
    WebSocketChannel::SendResult m_sendRequestResult;
    unsigned long m_bufferedAmount;
};

WorkerThreadableWebSocketChannel::WorkerThreadableWebSocketChannel(WorkerGlobalScope& workerGlobalScope, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    : m_workerClientWrapper(ThreadableWebSocketChannelClientWrapper::create(client))
    , m_bridge(Bridge::create(m_workerClientWrapper, workerGlobalScope))
    , m_sourceURLAtConnection(sourceURL)
    , m_lineNumberAtConnection(lineNumber)
{
    ASSERT(m_workerClientWrapper.get());
    m_bridge->initialize(sourceURL, lineNumber);
}

WorkerThreadableWebSocketChannel::~WorkerThreadableWebSocketChannel()
{
    if (m_bridge)
        m_bridge->disconnect();
}

bool WorkerThreadableWebSocketChannel::connect(const KURL& url, const String& protocol)
{
    if (m_bridge)
        return m_bridge->connect(url, protocol);
    return false;
}

String WorkerThreadableWebSocketChannel::subprotocol()
{
    return m_workerClientWrapper->subprotocol();
}

String WorkerThreadableWebSocketChannel::extensions()
{
    return m_workerClientWrapper->extensions();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(const String& message)
{
    if (!m_bridge)
        return WebSocketChannel::SendFail;
    return m_bridge->send(message);
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!m_bridge)
        return WebSocketChannel::SendFail;
    return m_bridge->send(binaryData, byteOffset, byteLength);
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(PassRefPtr<BlobDataHandle> blobData)
{
    if (!m_bridge)
        return WebSocketChannel::SendFail;
    return m_bridge->send(blobData);
}

unsigned long WorkerThreadableWebSocketChannel::bufferedAmount() const
{
    if (!m_bridge)
        return 0;
    return m_bridge->bufferedAmount();
}

void WorkerThreadableWebSocketChannel::close(int code, const String& reason)
{
    if (m_bridge)
        m_bridge->close(code, reason);
}

void WorkerThreadableWebSocketChannel::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    if (!m_bridge)
        return;

    RefPtr<ScriptCallStack> callStack = createScriptCallStack(1, true);
    if (callStack && callStack->size())  {
        // In order to emulate the ConsoleMessage behavior,
        // we should ignore the specified url and line number if
        // we can get the JavaScript context.
        m_bridge->fail(reason, level, callStack->at(0).sourceURL(), callStack->at(0).lineNumber());
    } else if (sourceURL.isEmpty() && !lineNumber) {
        // No information is specified by the caller - use the url
        // and the line number at the connection.
        m_bridge->fail(reason, level, m_sourceURLAtConnection, m_lineNumberAtConnection);
    } else {
        // Use the specified information.
        m_bridge->fail(reason, level, sourceURL, lineNumber);
    }
}

void WorkerThreadableWebSocketChannel::disconnect()
{
    m_bridge->disconnect();
    m_bridge.clear();
}

void WorkerThreadableWebSocketChannel::suspend()
{
    m_workerClientWrapper->suspend();
    if (m_bridge)
        m_bridge->suspend();
}

void WorkerThreadableWebSocketChannel::resume()
{
    m_workerClientWrapper->resume();
    if (m_bridge)
        m_bridge->resume();
}

void WorkerThreadableWebSocketChannel::trace(Visitor* visitor)
{
    visitor->trace(m_workerClientWrapper);
    WebSocketChannel::trace(visitor);
}

WorkerThreadableWebSocketChannel::Peer::Peer(PassRefPtr<WeakReference<Peer> > reference, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, WorkerLoaderProxy& loaderProxy, ExecutionContext* context, const String& sourceURL, unsigned lineNumber, PassOwnPtr<ThreadableWebSocketChannelSyncHelper> syncHelper)
    : m_workerClientWrapper(clientWrapper)
    , m_loaderProxy(loaderProxy)
    , m_mainWebSocketChannel(nullptr)
    , m_syncHelper(syncHelper)
    , m_weakFactory(reference, this)
{
    ASSERT(isMainThread());
    ASSERT(m_workerClientWrapper.get());

    Document* document = toDocument(context);
    if (RuntimeEnabledFeatures::experimentalWebSocketEnabled()) {
        m_mainWebSocketChannel = NewWebSocketChannelImpl::create(document, this, sourceURL, lineNumber);
    } else {
        m_mainWebSocketChannel = MainThreadWebSocketChannel::create(document, this, sourceURL, lineNumber);
    }

    m_syncHelper->signalWorkerThread();
}

WorkerThreadableWebSocketChannel::Peer::~Peer()
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->disconnect();
}

void WorkerThreadableWebSocketChannel::Peer::initialize(ExecutionContext* context, PassRefPtr<WeakReference<Peer> > reference, WorkerLoaderProxy* loaderProxy, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, const String& sourceURLAtConnection, unsigned lineNumberAtConnection, PassOwnPtr<ThreadableWebSocketChannelSyncHelper> syncHelper)
{
    // The caller must call destroy() to free the peer.
    new Peer(reference, clientWrapper, *loaderProxy, context, sourceURLAtConnection, lineNumberAtConnection, syncHelper);
}

void WorkerThreadableWebSocketChannel::Peer::destroy()
{
    ASSERT(isMainThread());
    delete this;
}

void WorkerThreadableWebSocketChannel::Peer::connect(const KURL& url, const String& protocol)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setConnectRequestResult(false);
    } else {
        bool connectRequestResult = m_mainWebSocketChannel->connect(url, protocol);
        m_syncHelper->setConnectRequestResult(connectRequestResult);
    }
    m_syncHelper->signalWorkerThread();
}

void WorkerThreadableWebSocketChannel::Peer::send(const String& message)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setSendRequestResult(WebSocketChannel::SendFail);
    } else {
        WebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(message);
        m_syncHelper->setSendRequestResult(sendRequestResult);
    }
    m_syncHelper->signalWorkerThread();
}

void WorkerThreadableWebSocketChannel::Peer::sendArrayBuffer(PassOwnPtr<Vector<char> > data)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setSendRequestResult(WebSocketChannel::SendFail);
    } else {
        WebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(data);
        m_syncHelper->setSendRequestResult(sendRequestResult);
    }
    m_syncHelper->signalWorkerThread();
}

void WorkerThreadableWebSocketChannel::Peer::sendBlob(PassRefPtr<BlobDataHandle> blobData)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setSendRequestResult(WebSocketChannel::SendFail);
    } else {
        WebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(blobData);
        m_syncHelper->setSendRequestResult(sendRequestResult);
    }
    m_syncHelper->signalWorkerThread();
}

void WorkerThreadableWebSocketChannel::Peer::bufferedAmount()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setBufferedAmount(0);
    } else {
        unsigned long bufferedAmount = m_mainWebSocketChannel->bufferedAmount();
        m_syncHelper->setBufferedAmount(bufferedAmount);
    }
    m_syncHelper->signalWorkerThread();
}

void WorkerThreadableWebSocketChannel::Peer::close(int code, const String& reason)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->close(code, reason);
}

void WorkerThreadableWebSocketChannel::Peer::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->fail(reason, level, sourceURL, lineNumber);
}

void WorkerThreadableWebSocketChannel::Peer::disconnect()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->disconnect();
    m_mainWebSocketChannel = nullptr;
}

void WorkerThreadableWebSocketChannel::Peer::suspend()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->suspend();
}

void WorkerThreadableWebSocketChannel::Peer::resume()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->resume();
}

static void workerGlobalScopeDidConnect(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, const String& subprotocol, const String& extensions)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->setSubprotocol(subprotocol);
    workerClientWrapper->setExtensions(extensions);
    workerClientWrapper->didConnect();
}

void WorkerThreadableWebSocketChannel::Peer::didConnect()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidConnect, m_workerClientWrapper.get(), m_mainWebSocketChannel->subprotocol(), m_mainWebSocketChannel->extensions()));
}

static void workerGlobalScopeDidReceiveMessage(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, const String& message)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveMessage(message);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessage(const String& message)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveMessage, m_workerClientWrapper.get(), message));
}

static void workerGlobalScopeDidReceiveBinaryData(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveBinaryData(binaryData);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveBinaryData, m_workerClientWrapper.get(), binaryData));
}

static void workerGlobalScopeDidUpdateBufferedAmount(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, unsigned long bufferedAmount)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didUpdateBufferedAmount(bufferedAmount);
}

void WorkerThreadableWebSocketChannel::Peer::didUpdateBufferedAmount(unsigned long bufferedAmount)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidUpdateBufferedAmount, m_workerClientWrapper.get(), bufferedAmount));
}

static void workerGlobalScopeDidStartClosingHandshake(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didStartClosingHandshake();
}

void WorkerThreadableWebSocketChannel::Peer::didStartClosingHandshake()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidStartClosingHandshake, m_workerClientWrapper.get()));
}

static void workerGlobalScopeDidClose(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, unsigned long unhandledBufferedAmount, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didClose(unhandledBufferedAmount, closingHandshakeCompletion, code, reason);
}

void WorkerThreadableWebSocketChannel::Peer::didClose(unsigned long unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT(isMainThread());
    m_mainWebSocketChannel = nullptr;
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidClose, m_workerClientWrapper.get(), unhandledBufferedAmount, closingHandshakeCompletion, code, reason));
}

static void workerGlobalScopeDidReceiveMessageError(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveMessageError();
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessageError()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveMessageError, m_workerClientWrapper.get()));
}

WorkerThreadableWebSocketChannel::Bridge::Bridge(PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, WorkerGlobalScope& workerGlobalScope)
    : m_workerClientWrapper(workerClientWrapper)
    , m_workerGlobalScope(workerGlobalScope)
    , m_loaderProxy(m_workerGlobalScope->thread()->workerLoaderProxy())
    , m_syncHelper(0)
{
    ASSERT(m_workerClientWrapper.get());
}

WorkerThreadableWebSocketChannel::Bridge::~Bridge()
{
    disconnect();
}

void WorkerThreadableWebSocketChannel::Bridge::initialize(const String& sourceURL, unsigned lineNumber)
{
    RefPtr<WeakReference<Peer> > reference = WeakReference<Peer>::createUnbound();
    m_peer = WeakPtr<Peer>(reference);

    OwnPtr<ThreadableWebSocketChannelSyncHelper> syncHelper = ThreadableWebSocketChannelSyncHelper::create(adoptPtr(blink::Platform::current()->createWaitableEvent()));
    // This pointer is guaranteed to be valid until we call terminatePeer.
    m_syncHelper = syncHelper.get();

    RefPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(createCallbackTask(&Peer::initialize, reference.release(), AllowCrossThreadAccess(&m_loaderProxy), m_workerClientWrapper.get(), sourceURL, lineNumber, syncHelper.release()))) {
        // The worker thread has been signalled to shutdown before method completion.
        terminatePeer();
    }
}

bool WorkerThreadableWebSocketChannel::Bridge::connect(const KURL& url, const String& protocol)
{
    if (hasTerminatedPeer())
        return false;

    RefPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(CallClosureTask::create(bind(&Peer::connect, m_peer, url.copy(), protocol.isolatedCopy()))))
        return false;

    return m_syncHelper->connectRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const String& message)
{
    if (hasTerminatedPeer())
        return WebSocketChannel::SendFail;

    RefPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(CallClosureTask::create(bind(&Peer::send, m_peer, message.isolatedCopy()))))
        return WebSocketChannel::SendFail;

    return m_syncHelper->sendRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (hasTerminatedPeer())
        return WebSocketChannel::SendFail;

    // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied into Vector<char>.
    OwnPtr<Vector<char> > data = adoptPtr(new Vector<char>(byteLength));
    if (binaryData.byteLength())
        memcpy(data->data(), static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);

    RefPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(CallClosureTask::create(bind(&Peer::sendArrayBuffer, m_peer, data.release()))))
        return WebSocketChannel::SendFail;

    return m_syncHelper->sendRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(PassRefPtr<BlobDataHandle> data)
{
    if (hasTerminatedPeer())
        return WebSocketChannel::SendFail;

    RefPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(CallClosureTask::create(bind(&Peer::sendBlob, m_peer, data))))
        return WebSocketChannel::SendFail;

    return m_syncHelper->sendRequestResult();
}

unsigned long WorkerThreadableWebSocketChannel::Bridge::bufferedAmount()
{
    if (hasTerminatedPeer())
        return 0;

    RefPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(CallClosureTask::create(bind(&Peer::bufferedAmount, m_peer))))
        return 0;

    return m_syncHelper->bufferedAmount();
}

void WorkerThreadableWebSocketChannel::Bridge::close(int code, const String& reason)
{
    if (hasTerminatedPeer())
        return;

    m_loaderProxy.postTaskToLoader(CallClosureTask::create(bind(&Peer::close, m_peer, code, reason.isolatedCopy())));
}

void WorkerThreadableWebSocketChannel::Bridge::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    if (hasTerminatedPeer())
        return;

    m_loaderProxy.postTaskToLoader(CallClosureTask::create(bind(&Peer::fail, m_peer, reason.isolatedCopy(), level, sourceURL.isolatedCopy(), lineNumber)));
}

void WorkerThreadableWebSocketChannel::Bridge::disconnect()
{
    clearClientWrapper();
    terminatePeer();
}

void WorkerThreadableWebSocketChannel::Bridge::suspend()
{
    if (hasTerminatedPeer())
        return;

    m_loaderProxy.postTaskToLoader(CallClosureTask::create(bind(&Peer::suspend, m_peer)));
}

void WorkerThreadableWebSocketChannel::Bridge::resume()
{
    if (hasTerminatedPeer())
        return;

    m_loaderProxy.postTaskToLoader(CallClosureTask::create(bind(&Peer::resume, m_peer)));
}

void WorkerThreadableWebSocketChannel::Bridge::clearClientWrapper()
{
    m_workerClientWrapper->clearClient();
}

// Caller of this function should hold a reference to the bridge, because this function may call WebSocket::didClose() in the end,
// which causes the bridge to get disconnected from the WebSocket and deleted if there is no other reference.
bool WorkerThreadableWebSocketChannel::Bridge::waitForMethodCompletion(PassOwnPtr<ExecutionContextTask> task)
{
    ASSERT(m_workerGlobalScope);
    ASSERT(m_syncHelper);

    m_loaderProxy.postTaskToLoader(task);

    blink::WebWaitableEvent* shutdownEvent = m_workerGlobalScope->thread()->shutdownEvent();
    Vector<blink::WebWaitableEvent*> events;
    events.append(shutdownEvent);
    events.append(m_syncHelper->event());

    ThreadState::SafePointScope scope(ThreadState::HeapPointersOnStack);
    blink::WebWaitableEvent* signalled = blink::Platform::current()->waitMultipleEvents(events);
    return signalled != shutdownEvent;
}

void WorkerThreadableWebSocketChannel::Bridge::terminatePeer()
{
    m_loaderProxy.postTaskToLoader(CallClosureTask::create(bind(&Peer::destroy, m_peer)));
    // Peer::destroy() deletes m_peer and then m_syncHelper will be released.
    // We must not touch m_syncHelper any more.
    m_syncHelper = 0;

    // We won't use this any more.
    m_workerGlobalScope = nullptr;
}

} // namespace WebCore
