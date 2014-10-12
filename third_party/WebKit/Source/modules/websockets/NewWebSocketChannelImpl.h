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

#ifndef NewWebSocketChannelImpl_h
#define NewWebSocketChannelImpl_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/frame/ConsoleTypes.h"
#include "modules/websockets/WebSocketChannel.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebSocketHandle.h"
#include "public/platform/WebSocketHandleClient.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Deque.h"
#include "wtf/FastAllocBase.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class WebSocketHandshakeRequest;
class WebSocketHandshakeRequestInfo;
class WebSocketHandshakeResponseInfo;

// This class may replace MainThreadWebSocketChannel.
class NewWebSocketChannelImpl final : public WebSocketChannel, public WebSocketHandleClient, public ContextLifecycleObserver {
public:
    // You can specify the source file and the line number information
    // explicitly by passing the last parameter.
    // In the usual case, they are set automatically and you don't have to
    // pass it.
    // Specify handle explicitly only in tests.
    static NewWebSocketChannelImpl* create(ExecutionContext* context, WebSocketChannelClient* client, const String& sourceURL = String(), unsigned lineNumber = 0, WebSocketHandle *handle = 0)
    {
        return new NewWebSocketChannelImpl(context, client, sourceURL, lineNumber, handle);
    }
    virtual ~NewWebSocketChannelImpl();

    // WebSocketChannel functions.
    virtual bool connect(const KURL&, const String& protocol) override;
    virtual void send(const String& message) override;
    virtual void send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) override;
    virtual void send(PassRefPtr<BlobDataHandle>) override;
    virtual void send(PassOwnPtr<Vector<char> > data) override;
    // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
    // argument to omit payload.
    virtual void close(int code, const String& reason) override;
    virtual void fail(const String& reason, MessageLevel, const String&, unsigned lineNumber) override;
    virtual void disconnect() override;

    virtual void suspend() override;
    virtual void resume() override;

    virtual void trace(Visitor*) override;

private:
    enum MessageType {
        MessageTypeText,
        MessageTypeBlob,
        MessageTypeArrayBuffer,
        MessageTypeVector,
        MessageTypeClose,
    };

    struct Message {
        explicit Message(const String&);
        explicit Message(PassRefPtr<BlobDataHandle>);
        explicit Message(PassRefPtr<ArrayBuffer>);
        explicit Message(PassOwnPtr<Vector<char> >);
        Message(unsigned short code, const String& reason);

        MessageType type;

        CString text;
        RefPtr<BlobDataHandle> blobDataHandle;
        RefPtr<ArrayBuffer> arrayBuffer;
        OwnPtr<Vector<char> > vectorData;
        unsigned short code;
        String reason;
    };

    struct ReceivedMessage {
        bool isMessageText;
        Vector<char> data;
    };

    class BlobLoader;

    NewWebSocketChannelImpl(ExecutionContext*, WebSocketChannelClient*, const String&, unsigned, WebSocketHandle*);
    void sendInternal();
    void flowControlIfNecessary();
    void failAsError(const String& reason) { fail(reason, ErrorMessageLevel, m_sourceURLAtConstruction, m_lineNumberAtConstruction); }
    void abortAsyncOperations();
    void handleDidClose(bool wasClean, unsigned short code, const String& reason);
    Document* document(); // can be called only when m_identifier > 0.

    // WebSocketHandleClient functions.
    virtual void didConnect(WebSocketHandle*, bool fail, const WebString& selectedProtocol, const WebString& extensions) override;
    virtual void didStartOpeningHandshake(WebSocketHandle*, const WebSocketHandshakeRequestInfo&) override;
    virtual void didFinishOpeningHandshake(WebSocketHandle*, const WebSocketHandshakeResponseInfo&) override;
    virtual void didFail(WebSocketHandle*, const WebString& message) override;
    virtual void didReceiveData(WebSocketHandle*, bool fin, WebSocketHandle::MessageType, const char* data, size_t /* size */) override;
    virtual void didClose(WebSocketHandle*, bool wasClean, unsigned short code, const WebString& reason) override;
    virtual void didReceiveFlowControl(WebSocketHandle*, int64_t quota) override;
    virtual void didStartClosingHandshake(WebSocketHandle*) override;

    // Methods for BlobLoader.
    void didFinishLoadingBlob(PassRefPtr<ArrayBuffer>);
    void didFailLoadingBlob(FileError::ErrorCode);

    // LifecycleObserver functions.
    virtual void contextDestroyed() override
    {
        // In oilpan we cannot assume this channel's finalizer has been called
        // before the document it is observing is dead and finalized since there
        // is no eager finalization. Instead the finalization happens at the
        // next GC which could be long enough after the Peer::destroy call for
        // the context (ie. Document) to be dead too. If the context's finalizer
        // is run first this method gets called. Instead we assert the channel
        // has been disconnected which happens in Peer::destroy.
        ASSERT(!m_handle);
        ASSERT(!m_client);
        ASSERT(!m_identifier);
        ContextLifecycleObserver::contextDestroyed();
    }

    // m_handle is a handle of the connection.
    // m_handle == 0 means this channel is closed.
    OwnPtr<WebSocketHandle> m_handle;

    // m_client can be deleted while this channel is alive, but this class
    // expects that disconnect() is called before the deletion.
    Member<WebSocketChannelClient> m_client;
    KURL m_url;
    // m_identifier > 0 means calling scriptContextExecution() returns a Document.
    unsigned long m_identifier;
    Member<BlobLoader> m_blobLoader;
    Deque<OwnPtr<Message> > m_messages;
    Vector<char> m_receivingMessageData;

    bool m_receivingMessageTypeIsText;
    int64_t m_sendingQuota;
    int64_t m_receivedDataSizeForFlowControl;
    size_t m_sentSizeOfTopMessage;

    String m_sourceURLAtConstruction;
    unsigned m_lineNumberAtConstruction;
    RefPtr<WebSocketHandshakeRequest> m_handshakeRequest;

    static const int64_t receivedDataSizeForFlowControlHighWaterMark = 1 << 15;
};

} // namespace blink

#endif // NewWebSocketChannelImpl_h
