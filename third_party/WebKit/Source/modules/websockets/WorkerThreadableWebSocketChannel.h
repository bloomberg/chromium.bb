/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef WorkerThreadableWebSocketChannel_h
#define WorkerThreadableWebSocketChannel_h

#include "core/frame/ConsoleTypes.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BlobDataHandle;
class KURL;
class ExecutionContext;
class ExecutionContextTask;
class ThreadableWebSocketChannelSyncHelper;
class WorkerGlobalScope;
class WorkerLoaderProxy;

class WorkerThreadableWebSocketChannel FINAL : public WebSocketChannel {
    WTF_MAKE_NONCOPYABLE(WorkerThreadableWebSocketChannel);
public:
    static WebSocketChannel* create(WorkerGlobalScope& workerGlobalScope, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    {
        return adoptRefCountedGarbageCollected(new WorkerThreadableWebSocketChannel(workerGlobalScope, client, sourceURL, lineNumber));
    }
    virtual ~WorkerThreadableWebSocketChannel();

    // WebSocketChannel functions.
    virtual bool connect(const KURL&, const String& protocol) OVERRIDE;
    virtual void send(const String& message) OVERRIDE;
    virtual void send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) OVERRIDE;
    virtual void send(PassRefPtr<BlobDataHandle>) OVERRIDE;
    virtual void send(PassOwnPtr<Vector<char> >) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }
    virtual void close(int code, const String& reason) OVERRIDE;
    virtual void fail(const String& reason, MessageLevel, const String&, unsigned) OVERRIDE;
    virtual void disconnect() OVERRIDE; // Will suppress didClose().
    virtual void suspend() OVERRIDE { }
    virtual void resume() OVERRIDE { }

    virtual void trace(Visitor*) OVERRIDE;

    class Bridge;
    // Allocated in the worker thread, but used in the main thread.
    class Peer FINAL : public GarbageCollectedFinalized<Peer>, public WebSocketChannelClient {
        USING_GARBAGE_COLLECTED_MIXIN(Peer);
        WTF_MAKE_NONCOPYABLE(Peer);
    public:
        Peer(Bridge*, WorkerLoaderProxy&, ThreadableWebSocketChannelSyncHelper*);
        virtual ~Peer();

        // sourceURLAtConnection and lineNumberAtConnection parameters may
        // be shown when the connection fails.
        static void initialize(ExecutionContext* executionContext, Peer* peer, const String& sourceURLAtConnection, unsigned lineNumberAtConnection)
        {
            peer->initializeInternal(executionContext, sourceURLAtConnection, lineNumberAtConnection);
        }

        void connect(const KURL&, const String& protocol);
        void send(const String& message);
        void sendArrayBuffer(PassOwnPtr<Vector<char> >);
        void sendBlob(PassRefPtr<BlobDataHandle>);
        void bufferedAmount();
        void close(int code, const String& reason);
        void fail(const String& reason, MessageLevel, const String& sourceURL, unsigned lineNumber);
        void disconnect();

        virtual void trace(Visitor*) OVERRIDE;

        // WebSocketChannelClient functions.
        virtual void didConnect(const String& subprotocol, const String& extensions) OVERRIDE;
        virtual void didReceiveMessage(const String& message) OVERRIDE;
        virtual void didReceiveBinaryData(PassOwnPtr<Vector<char> >) OVERRIDE;
        virtual void didConsumeBufferedAmount(unsigned long) OVERRIDE;
        virtual void didStartClosingHandshake() OVERRIDE;
        virtual void didClose(ClosingHandshakeCompletionStatus, unsigned short code, const String& reason) OVERRIDE;
        virtual void didReceiveMessageError() OVERRIDE;

    private:
        void initializeInternal(ExecutionContext*, const String& sourceURLAtConnection, unsigned lineNumberAtConnection);

        Member<Bridge> m_bridge;
        WorkerLoaderProxy& m_loaderProxy;
        Member<WebSocketChannel> m_mainWebSocketChannel;
        Member<ThreadableWebSocketChannelSyncHelper> m_syncHelper;
    };

    // Bridge for Peer. Running on the worker thread.
    class Bridge FINAL : public GarbageCollectedFinalized<Bridge> {
        WTF_MAKE_NONCOPYABLE(Bridge);
    public:
        Bridge(WebSocketChannelClient*, WorkerGlobalScope&);
        ~Bridge();
        // sourceURLAtConnection and lineNumberAtConnection parameters may
        // be shown when the connection fails.
        void initialize(const String& sourceURLAtConnection, unsigned lineNumberAtConnection);
        bool connect(const KURL&, const String& protocol);
        void send(const String& message);
        void send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength);
        void send(PassRefPtr<BlobDataHandle>);
        unsigned long bufferedAmount();
        void close(int code, const String& reason);
        void fail(const String& reason, MessageLevel, const String& sourceURL, unsigned lineNumber);
        void disconnect();

        // Returns null when |disconnect| has already been called.
        WebSocketChannelClient* client() { return m_client; }

        void trace(Visitor*);

    private:
        // Returns false if shutdown event is received before method completion.
        bool waitForMethodCompletion(PassOwnPtr<ExecutionContextTask>);

        Member<WebSocketChannelClient> m_client;
        RefPtrWillBeMember<WorkerGlobalScope> m_workerGlobalScope;
        WorkerLoaderProxy& m_loaderProxy;
        Member<ThreadableWebSocketChannelSyncHelper> m_syncHelper;
        Member<Peer> m_peer;
    };

private:
    WorkerThreadableWebSocketChannel(WorkerGlobalScope&, WebSocketChannelClient*, const String& sourceURL, unsigned lineNumber);

    Member<Bridge> m_bridge;
    String m_sourceURLAtConnection;
    unsigned m_lineNumberAtConnection;
};

} // namespace blink

#endif // WorkerThreadableWebSocketChannel_h
