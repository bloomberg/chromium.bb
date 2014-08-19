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
#include "core/dom/ExecutionContextTask.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/MainThreadWebSocketChannel.h"
#include "modules/websockets/NewWebSocketChannelImpl.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"
#include "wtf/text/WTFString.h"

namespace blink {

typedef WorkerThreadableWebSocketChannel::Bridge Bridge;
typedef WorkerThreadableWebSocketChannel::Peer Peer;

// Created and destroyed on the worker thread. All setters of this class are
// called on the main thread, while all getters are called on the worker
// thread. signalWorkerThread() must be called before any getters are called.
class ThreadableWebSocketChannelSyncHelper : public GarbageCollectedFinalized<ThreadableWebSocketChannelSyncHelper> {
public:
    static ThreadableWebSocketChannelSyncHelper* create(PassOwnPtr<WebWaitableEvent> event)
    {
        return new ThreadableWebSocketChannelSyncHelper(event);
    }

    ~ThreadableWebSocketChannelSyncHelper()
    {
    }

    // All setters are called on the main thread.
    void setConnectRequestResult(bool connectRequestResult)
    {
        m_connectRequestResult = connectRequestResult;
    }

    // All getter are called on the worker thread.
    bool connectRequestResult() const
    {
        return m_connectRequestResult;
    }

    // This should be called after all setters are called and before any
    // getters are called.
    void signalWorkerThread()
    {
        m_event->signal();
    }
    void wait()
    {
        m_event->wait();
    }

    void trace(Visitor* visitor) { }

private:
    explicit ThreadableWebSocketChannelSyncHelper(PassOwnPtr<WebWaitableEvent> event)
        : m_event(event)
        , m_connectRequestResult(false)
    {
    }

    OwnPtr<WebWaitableEvent> m_event;
    bool m_connectRequestResult;
};

WorkerThreadableWebSocketChannel::WorkerThreadableWebSocketChannel(WorkerGlobalScope& workerGlobalScope, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    : m_bridge(new Bridge(client, workerGlobalScope))
    , m_sourceURLAtConnection(sourceURL)
    , m_lineNumberAtConnection(lineNumber)
{
    m_bridge->initialize(sourceURL, lineNumber);
}

WorkerThreadableWebSocketChannel::~WorkerThreadableWebSocketChannel()
{
    ASSERT(!m_bridge);
}

bool WorkerThreadableWebSocketChannel::connect(const KURL& url, const String& protocol)
{
    ASSERT(m_bridge);
    return m_bridge->connect(url, protocol);
}

void WorkerThreadableWebSocketChannel::send(const String& message)
{
    ASSERT(m_bridge);
    m_bridge->send(message);
}

void WorkerThreadableWebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    ASSERT(m_bridge);
    m_bridge->send(binaryData, byteOffset, byteLength);
}

void WorkerThreadableWebSocketChannel::send(PassRefPtr<BlobDataHandle> blobData)
{
    ASSERT(m_bridge);
    m_bridge->send(blobData);
}

void WorkerThreadableWebSocketChannel::close(int code, const String& reason)
{
    ASSERT(m_bridge);
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
    WebSocketChannel::trace(visitor);
}

Peer::Peer(Bridge* bridge, WorkerLoaderProxy& loaderProxy, ThreadableWebSocketChannelSyncHelper* syncHelper)
    : m_bridge(bridge)
    , m_loaderProxy(loaderProxy)
    , m_mainWebSocketChannel(nullptr)
    , m_syncHelper(syncHelper)
{
    ASSERT(!isMainThread());
}

Peer::~Peer()
{
    ASSERT(!isMainThread());
}

void Peer::initializeInternal(ExecutionContext* context, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(isMainThread());
    Document* document = toDocument(context);
    if (RuntimeEnabledFeatures::experimentalWebSocketEnabled()) {
        m_mainWebSocketChannel = NewWebSocketChannelImpl::create(document, this, sourceURL, lineNumber);
    } else {
        m_mainWebSocketChannel = MainThreadWebSocketChannel::create(document, this, sourceURL, lineNumber);
    }
    m_syncHelper->signalWorkerThread();
}

void Peer::connect(const KURL& url, const String& protocol)
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setConnectRequestResult(false);
    } else {
        bool connectRequestResult = m_mainWebSocketChannel->connect(url, protocol);
        m_syncHelper->setConnectRequestResult(connectRequestResult);
    }
    m_syncHelper->signalWorkerThread();
}

void Peer::send(const String& message)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->send(message);
}

void Peer::sendArrayBuffer(PassOwnPtr<Vector<char> > data)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->send(data);
}

void Peer::sendBlob(PassRefPtr<BlobDataHandle> blobData)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->send(blobData);
}

void Peer::close(int code, const String& reason)
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->close(code, reason);
}

void Peer::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->fail(reason, level, sourceURL, lineNumber);
}

void Peer::disconnect()
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (m_mainWebSocketChannel) {
        m_mainWebSocketChannel->disconnect();
        m_mainWebSocketChannel = nullptr;
    }
    m_syncHelper->signalWorkerThread();
}

static void workerGlobalScopeDidConnect(ExecutionContext* context, Bridge* bridge, const String& subprotocol, const String& extensions)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didConnect(subprotocol, extensions);
}

void Peer::didConnect(const String& subprotocol, const String& extensions)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidConnect, m_bridge, subprotocol, extensions));
}

static void workerGlobalScopeDidReceiveMessage(ExecutionContext* context, Bridge* bridge, const String& message)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didReceiveMessage(message);
}

void Peer::didReceiveMessage(const String& message)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidReceiveMessage, m_bridge, message));
}

static void workerGlobalScopeDidReceiveBinaryData(ExecutionContext* context, Bridge* bridge, PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didReceiveBinaryData(binaryData);
}

void Peer::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidReceiveBinaryData, m_bridge, binaryData));
}

static void workerGlobalScopeDidConsumeBufferedAmount(ExecutionContext* context, Bridge* bridge, unsigned long consumed)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didConsumeBufferedAmount(consumed);
}

void Peer::didConsumeBufferedAmount(unsigned long consumed)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidConsumeBufferedAmount, m_bridge, consumed));
}

static void workerGlobalScopeDidStartClosingHandshake(ExecutionContext* context, Bridge* bridge)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didStartClosingHandshake();
}

void Peer::didStartClosingHandshake()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidStartClosingHandshake, m_bridge));
}

static void workerGlobalScopeDidClose(ExecutionContext* context, Bridge* bridge, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didClose(closingHandshakeCompletion, code, reason);
}

void Peer::didClose(ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel) {
        m_mainWebSocketChannel->disconnect();
        m_mainWebSocketChannel = nullptr;
    }
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidClose, m_bridge, closingHandshakeCompletion, code, reason));
}

static void workerGlobalScopeDidReceiveMessageError(ExecutionContext* context, Bridge* bridge)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge->client())
        bridge->client()->didReceiveMessageError();
}

void Peer::didReceiveMessageError()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidReceiveMessageError, m_bridge));
}

void Peer::trace(Visitor* visitor)
{
    visitor->trace(m_bridge);
    visitor->trace(m_mainWebSocketChannel);
    visitor->trace(m_syncHelper);
    WebSocketChannelClient::trace(visitor);
}

Bridge::Bridge(WebSocketChannelClient* client, WorkerGlobalScope& workerGlobalScope)
    : m_client(client)
    , m_workerGlobalScope(workerGlobalScope)
    , m_loaderProxy(m_workerGlobalScope->thread()->workerLoaderProxy())
    , m_syncHelper(ThreadableWebSocketChannelSyncHelper::create(adoptPtr(Platform::current()->createWaitableEvent())))
    , m_peer(new Peer(this, m_loaderProxy, m_syncHelper))
{
}

Bridge::~Bridge()
{
    ASSERT(!m_peer);
}

void Bridge::initialize(const String& sourceURL, unsigned lineNumber)
{
    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::initialize, AllowCrossThreadAccess(m_peer.get()), sourceURL, lineNumber))) {
        // The worker thread has been signalled to shutdown before method completion.
        disconnect();
    }
}

bool Bridge::connect(const KURL& url, const String& protocol)
{
    if (!m_peer)
        return false;

    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::connect, m_peer.get(), url, protocol)))
        return false;

    return m_syncHelper->connectRequestResult();
}

void Bridge::send(const String& message)
{
    ASSERT(m_peer);
    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::send, m_peer.get(), message));
}

void Bridge::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    ASSERT(m_peer);
    // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied into Vector<char>.
    OwnPtr<Vector<char> > data = adoptPtr(new Vector<char>(byteLength));
    if (binaryData.byteLength())
        memcpy(data->data(), static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);

    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::sendArrayBuffer, m_peer.get(), data.release()));
}

void Bridge::send(PassRefPtr<BlobDataHandle> data)
{
    ASSERT(m_peer);
    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::sendBlob, m_peer.get(), data));
}

void Bridge::close(int code, const String& reason)
{
    ASSERT(m_peer);
    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::close, m_peer.get(), code, reason));
}

void Bridge::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(m_peer);
    m_loaderProxy.postTaskToLoader(createCrossThreadTask(&Peer::fail, m_peer.get(), reason, level, sourceURL, lineNumber));
}

void Bridge::disconnect()
{
    if (!m_peer)
        return;

    waitForMethodCompletion(createCrossThreadTask(&Peer::disconnect, m_peer.get()));
    // Here |m_peer| is detached from the main thread and we can delete it.

    m_client = nullptr;
    m_peer = nullptr;
    m_syncHelper = nullptr;
    // We won't use this any more.
    m_workerGlobalScope.clear();
}

// Caller of this function should hold a reference to the bridge, because this function may call WebSocket::didClose() in the end,
// which causes the bridge to get disconnected from the WebSocket and deleted if there is no other reference.
bool Bridge::waitForMethodCompletion(PassOwnPtr<ExecutionContextTask> task)
{
    ASSERT(m_workerGlobalScope);
    ASSERT(m_syncHelper);

    m_loaderProxy.postTaskToLoader(task);

    // We wait for the syncHelper event even if a shutdown event is fired.
    // See https://codereview.chromium.org/267323004/#msg43 for why we need to wait this.
    ThreadState::SafePointScope scope(ThreadState::HeapPointersOnStack);
    m_syncHelper->wait();
    // This is checking whether a shutdown event is fired or not.
    return !m_workerGlobalScope->thread()->terminated();
}

void Bridge::trace(Visitor* visitor)
{
    visitor->trace(m_client);
    visitor->trace(m_workerGlobalScope);
    visitor->trace(m_syncHelper);
    visitor->trace(m_peer);
}

} // namespace blink
