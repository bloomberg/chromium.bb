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

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/MainThreadWebSocketChannel.h"
#include "modules/websockets/NewWebSocketChannelImpl.h"
#include "modules/websockets/ThreadableWebSocketChannelClientWrapper.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"

namespace blink {

// Created and destroyed on the worker thread. All setters of this class are
// called on the main thread, while all getters are called on the worker
// thread. signalWorkerThread() must be called before any getters are called.
class ThreadableWebSocketChannelSyncHelper : public ThreadSafeRefCountedWillBeGarbageCollectedFinalized<ThreadableWebSocketChannelSyncHelper> {
public:
    static PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelSyncHelper> create(PassOwnPtr<blink::WebWaitableEvent> event)
    {
        return adoptRefWillBeNoop(new ThreadableWebSocketChannelSyncHelper(event));
    }

    ~ThreadableWebSocketChannelSyncHelper()
    {
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

    // All getter are called on the worker thread.
    bool connectRequestResult() const
    {
        return m_connectRequestResult;
    }
    WebSocketChannel::SendResult sendRequestResult() const
    {
        return m_sendRequestResult;
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

    void trace(Visitor* visitor) { }

private:
    explicit ThreadableWebSocketChannelSyncHelper(PassOwnPtr<blink::WebWaitableEvent> event)
        : m_event(event)
        , m_connectRequestResult(false)
        , m_sendRequestResult(WebSocketChannel::SendFail)
    {
    }

    OwnPtr<blink::WebWaitableEvent> m_event;
    bool m_connectRequestResult;
    WebSocketChannel::SendResult m_sendRequestResult;
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
    ASSERT(!m_bridge);
}

bool WorkerThreadableWebSocketChannel::connect(const KURL& url, const String& protocol)
{
    if (m_bridge)
        return m_bridge->connect(url, protocol);
    return false;
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

void WorkerThreadableWebSocketChannel::close(int code, const String& reason)
{
    if (m_bridge)
        m_bridge->close(code, reason);
}

void WorkerThreadableWebSocketChannel::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    if (!m_bridge)
        return;

    RefPtrWillBeRawPtr<ScriptCallStack> callStack = createScriptCallStack(1, true);
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

void WorkerThreadableWebSocketChannel::trace(Visitor* visitor)
{
    visitor->trace(m_bridge);
    visitor->trace(m_workerClientWrapper);
    WebSocketChannel::trace(visitor);
}

#if ENABLE(OILPAN)
WorkerThreadableWebSocketChannel::Peer::Peer(RawPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, WorkerLoaderProxy& loaderProxy, ExecutionContext* context, const String& sourceURL, unsigned lineNumber, RawPtr<ThreadableWebSocketChannelSyncHelper> syncHelper)
#else
WorkerThreadableWebSocketChannel::Peer::Peer(PassRefPtr<WeakReference<Peer> > reference, PassRefPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, WorkerLoaderProxy& loaderProxy, ExecutionContext* context, const String& sourceURL, unsigned lineNumber, PassRefPtr<ThreadableWebSocketChannelSyncHelper> syncHelper)
#endif
    : m_workerClientWrapper(clientWrapper)
    , m_loaderProxy(loaderProxy)
    , m_mainWebSocketChannel(nullptr)
    , m_syncHelper(syncHelper)
#if ENABLE(OILPAN)
    , m_keepAlive(this)
#else
    , m_weakFactory(reference, this)
#endif
{
    ASSERT(isMainThread());
    ASSERT(m_workerClientWrapper.get());

    Document* document = toDocument(context);
    if (RuntimeEnabledFeatures::experimentalWebSocketEnabled()) {
        m_mainWebSocketChannel = NewWebSocketChannelImpl::create(document, this, sourceURL, lineNumber);
    } else {
        m_mainWebSocketChannel = MainThreadWebSocketChannel::create(document, this, sourceURL, lineNumber);
    }
}

WorkerThreadableWebSocketChannel::Peer::~Peer()
{
    ASSERT(isMainThread());
}

#if ENABLE(OILPAN)
void WorkerThreadableWebSocketChannel::Peer::initialize(ExecutionContext* context, WeakMember<Peer>* reference, WorkerLoaderProxy* loaderProxy, RawPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, const String& sourceURLAtConnection, unsigned lineNumberAtConnection, RawPtr<ThreadableWebSocketChannelSyncHelper> syncHelper)
{
    // The caller must call destroy() to free the peer.
    *reference = new Peer(clientWrapper, *loaderProxy, context, sourceURLAtConnection, lineNumberAtConnection, syncHelper);
    syncHelper->signalWorkerThread();
}
#else
void WorkerThreadableWebSocketChannel::Peer::initialize(ExecutionContext* context, PassRefPtr<WeakReference<Peer> > reference, WorkerLoaderProxy* loaderProxy, PassRefPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, const String& sourceURLAtConnection, unsigned lineNumberAtConnection, PassRefPtr<ThreadableWebSocketChannelSyncHelper> prpSyncHelper)
{
    RefPtr<ThreadableWebSocketChannelSyncHelper> syncHelper = prpSyncHelper;
    // The caller must call destroy() to free the peer.
    new Peer(reference, clientWrapper, *loaderProxy, context, sourceURLAtConnection, lineNumberAtConnection, syncHelper);
    syncHelper->signalWorkerThread();
}
#endif

void WorkerThreadableWebSocketChannel::Peer::destroy()
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->disconnect();

#if ENABLE(OILPAN)
    m_keepAlive = nullptr;
    m_syncHelper->signalWorkerThread();
#else
    delete this;
#endif
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

static void workerGlobalScopeDidConnect(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, const String& subprotocol, const String& extensions)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didConnect(subprotocol, extensions);
}

void WorkerThreadableWebSocketChannel::Peer::didConnect(const String& subprotocol, const String& extensions)
{
    ASSERT(isMainThread());
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidConnect, m_workerClientWrapper.get(), subprotocol, extensions);
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

static void workerGlobalScopeDidReceiveMessage(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, const String& message)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveMessage(message);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessage(const String& message)
{
    ASSERT(isMainThread());
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidReceiveMessage, m_workerClientWrapper.get(), message);
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

static void workerGlobalScopeDidReceiveBinaryData(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveBinaryData(binaryData);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT(isMainThread());
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidReceiveBinaryData, m_workerClientWrapper.get(), binaryData);
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

static void workerGlobalScopeDidConsumeBufferedAmount(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, unsigned long consumed)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didConsumeBufferedAmount(consumed);
}

void WorkerThreadableWebSocketChannel::Peer::didConsumeBufferedAmount(unsigned long consumed)
{
    ASSERT(isMainThread());
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidConsumeBufferedAmount, m_workerClientWrapper.get(), consumed);
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

static void workerGlobalScopeDidStartClosingHandshake(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didStartClosingHandshake();
}

void WorkerThreadableWebSocketChannel::Peer::didStartClosingHandshake()
{
    ASSERT(isMainThread());
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidStartClosingHandshake, m_workerClientWrapper.get());
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

static void workerGlobalScopeDidClose(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didClose(closingHandshakeCompletion, code, reason);
}

void WorkerThreadableWebSocketChannel::Peer::didClose(ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT(isMainThread());
    m_mainWebSocketChannel = nullptr;
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidClose, m_workerClientWrapper.get(), closingHandshakeCompletion, code, reason);
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

static void workerGlobalScopeDidReceiveMessageError(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveMessageError();
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessageError()
{
    ASSERT(isMainThread());
    // It is important to seprate task creation from posting
    // the task. See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&workerGlobalScopeDidReceiveMessageError, m_workerClientWrapper.get());
    m_loaderProxy.postTaskToWorkerGlobalScope(task.release());
}

void WorkerThreadableWebSocketChannel::Peer::trace(Visitor* visitor)
{
    visitor->trace(m_workerClientWrapper);
    visitor->trace(m_mainWebSocketChannel);
    visitor->trace(m_syncHelper);
    WebSocketChannelClient::trace(visitor);
}

WorkerThreadableWebSocketChannel::Bridge::Bridge(PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, WorkerGlobalScope& workerGlobalScope)
    : m_workerClientWrapper(workerClientWrapper)
    , m_workerGlobalScope(workerGlobalScope)
    , m_loaderProxy(m_workerGlobalScope->thread()->workerLoaderProxy())
    , m_syncHelper(nullptr)
    , m_peer(nullptr)
{
    ASSERT(m_workerClientWrapper.get());
}

WorkerThreadableWebSocketChannel::Bridge::~Bridge()
{
    ASSERT(hasTerminatedPeer());
}

void WorkerThreadableWebSocketChannel::Bridge::initialize(const String& sourceURL, unsigned lineNumber)
{
#if !ENABLE(OILPAN)
    RefPtr<WeakReference<Peer> > reference = WeakReference<Peer>::createUnbound();
    m_peer = WeakPtr<Peer>(reference);
#endif

    RefPtrWillBeRawPtr<ThreadableWebSocketChannelSyncHelper> syncHelper = ThreadableWebSocketChannelSyncHelper::create(adoptPtr(blink::Platform::current()->createWaitableEvent()));
    // This pointer is guaranteed to be valid until we call terminatePeer.
    m_syncHelper = syncHelper.get();

    RefPtrWillBeRawPtr<Bridge> protect(this);
#if ENABLE(OILPAN)
    // In order to assure all temporary objects to be destroyed before
    // posting the task, we separate task creation and posting.
    // In other words, it is dangerous to have a complicated expression
    // as a waitForMethodCompletion argument.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&Peer::initialize, &m_peer, AllowCrossThreadAccess(&m_loaderProxy), m_workerClientWrapper.get(), sourceURL, lineNumber, syncHelper.get());
#else
    // See the above comment.
    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&Peer::initialize, reference, AllowCrossThreadAccess(&m_loaderProxy), m_workerClientWrapper.get(), sourceURL, lineNumber, syncHelper.get());
#endif
    if (!waitForMethodCompletion(task.release())) {
        // The worker thread has been signalled to shutdown before method completion.
        disconnect();
    }
}

bool WorkerThreadableWebSocketChannel::Bridge::connect(const KURL& url, const String& protocol)
{
    if (hasTerminatedPeer())
        return false;

    RefPtrWillBeRawPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::connect, m_peer, url, protocol)))
        return false;

    return m_syncHelper->connectRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const String& message)
{
    if (hasTerminatedPeer())
        return WebSocketChannel::SendFail;

    RefPtrWillBeRawPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::send, m_peer, message)))
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

    RefPtrWillBeRawPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::sendArrayBuffer, m_peer, data.release())))
        return WebSocketChannel::SendFail;

    return m_syncHelper->sendRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(PassRefPtr<BlobDataHandle> data)
{
    if (hasTerminatedPeer())
        return WebSocketChannel::SendFail;

    RefPtrWillBeRawPtr<Bridge> protect(this);
    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::sendBlob, m_peer, data)))
        return WebSocketChannel::SendFail;

    return m_syncHelper->sendRequestResult();
}

void WorkerThreadableWebSocketChannel::Bridge::close(int code, const String& reason)
{
    if (hasTerminatedPeer())
        return;

    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::close, m_peer, code, reason));
}

void WorkerThreadableWebSocketChannel::Bridge::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    if (hasTerminatedPeer())
        return;

    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::fail, m_peer, reason, level, sourceURL, lineNumber));
}

void WorkerThreadableWebSocketChannel::Bridge::disconnect()
{
    if (hasTerminatedPeer())
        return;

    clearClientWrapper();
    terminatePeer();
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

    // We wait for the syncHelper event even if a shutdown event is fired.
    // See https://codereview.chromium.org/267323004/#msg43 for why we need to wait this.
    Vector<blink::WebWaitableEvent*> events;
    events.append(m_syncHelper->event());
    ThreadState::SafePointScope scope(ThreadState::HeapPointersOnStack);
    blink::Platform::current()->waitMultipleEvents(events);
    // This is checking whether a shutdown event is fired or not.
    return !m_workerGlobalScope->thread()->terminated();
}

void WorkerThreadableWebSocketChannel::Bridge::terminatePeer()
{
    ASSERT(!hasTerminatedPeer());

#if ENABLE(OILPAN)
    // The worker thread has to wait for the main thread to complete Peer::destroy,
    // because the worker thread has to make sure that the main thread does not have any
    // references to on-heap objects allocated in the thread heap of the worker thread
    // before the worker thread shuts down.
    waitForMethodCompletion(createCrossThreadTask(&Peer::destroy, m_peer));
#else
    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::destroy, m_peer));
#endif

    // Peer::destroy() deletes m_peer and then m_syncHelper will be released.
    // We must not touch m_syncHelper any more.
    m_syncHelper = nullptr;

    // We won't use this any more.
    m_workerGlobalScope = nullptr;
}

void WorkerThreadableWebSocketChannel::Bridge::trace(Visitor* visitor)
{
    visitor->trace(m_workerClientWrapper);
    visitor->trace(m_workerGlobalScope);
    visitor->trace(m_syncHelper);
    visitor->trace(m_peer);
}

} // namespace blink
