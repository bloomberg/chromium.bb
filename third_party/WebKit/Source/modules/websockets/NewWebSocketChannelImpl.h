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
#include "core/page/ConsoleTypes.h"
#include "modules/websockets/WebSocketChannel.h"
#include "public/platform/WebSocketHandle.h"
#include "public/platform/WebSocketHandleClient.h"
#include "weborigin/KURL.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Deque.h"
#include "wtf/FastAllocBase.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

// This class may replace MainThreadWebSocketChannel.
class NewWebSocketChannelImpl : public WebSocketChannel, public RefCounted<NewWebSocketChannelImpl>, public WebKit::WebSocketHandleClient, public ContextLifecycleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // You can specify the source file and the line number information
    // explicitly by passing the last parameter.
    // In the usual case, they are set automatically and you don't have to
    // pass it.
    static PassRefPtr<NewWebSocketChannelImpl> create(ScriptExecutionContext* context, WebSocketChannelClient* client, const String& sourceURL = String(), unsigned lineNumber = 0)
    {
        return adoptRef(new NewWebSocketChannelImpl(context, client, sourceURL, lineNumber));
    }
    virtual ~NewWebSocketChannelImpl();

    // WebSocketChannel functions.
    virtual void connect(const KURL&, const String& protocol) OVERRIDE;
    virtual String subprotocol() OVERRIDE;
    virtual String extensions() OVERRIDE;
    virtual WebSocketChannel::SendResult send(const String& message) OVERRIDE;
    virtual WebSocketChannel::SendResult send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) OVERRIDE;
    virtual WebSocketChannel::SendResult send(const Blob&) OVERRIDE;
    virtual unsigned long bufferedAmount() const OVERRIDE;
    // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
    // argument to omit payload.
    virtual void close(int code, const String& reason) OVERRIDE;
    virtual void fail(const String& reason, MessageLevel, const String&, unsigned lineNumber) OVERRIDE;
    using WebSocketChannel::fail;
    virtual void disconnect() OVERRIDE;

    using RefCounted<NewWebSocketChannelImpl>::ref;
    using RefCounted<NewWebSocketChannelImpl>::deref;

    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;

private:
    enum MessageType {
        MessageTypeText,
        MessageTypeBlob,
        MessageTypeArrayBuffer,
    };
    struct Message {
        explicit Message(const String&);
        explicit Message(const Blob&);
        explicit Message(PassRefPtr<ArrayBuffer>);
        MessageType type;
        CString text;
        RefPtr<Blob> blob;
        RefPtr<ArrayBuffer> arrayBuffer;
    };

    struct ReceivedMessage {
        bool isMessageText;
        Vector<char> data;
    };
    class BlobLoader;

    NewWebSocketChannelImpl(ScriptExecutionContext*, WebSocketChannelClient*, const String&, unsigned);
    void sendInternal();
    void flowControlIfNecessary();
    void failAsError(const String& reason) { fail(reason, ErrorMessageLevel, "", 0); }
    void abortAsyncOperations();

    // WebSocketHandleClient functions.
    virtual void didConnect(WebKit::WebSocketHandle*, bool succeed, const WebKit::WebString& selectedProtocol, const WebKit::WebString& extensions) OVERRIDE;
    virtual void didReceiveData(WebKit::WebSocketHandle*, WebKit::WebSocketHandle::MessageType, const char* data, size_t /* size */, bool fin) OVERRIDE;
    virtual void didClose(WebKit::WebSocketHandle*, unsigned short code, const WebKit::WebString& reason) OVERRIDE;
    virtual void didReceiveFlowControl(WebKit::WebSocketHandle*, int64_t quota) OVERRIDE { m_sendingQuota += quota; }

    void handleTextMessage(Vector<char>*);
    void handleBinaryMessage(Vector<char>*);
    void handleDidClose(unsigned short code, const String& reason);

    // Methods for BlobLoader.
    void didFinishLoadingBlob(PassRefPtr<ArrayBuffer>);
    void didFailLoadingBlob(FileError::ErrorCode);

    // WebSocketChannel functions.
    virtual void refWebSocketChannel() OVERRIDE { ref(); }
    virtual void derefWebSocketChannel() OVERRIDE { deref(); }

    // LifecycleObserver functions.
    // This object must be destroyed before the context.
    virtual void contextDestroyed() OVERRIDE { ASSERT_NOT_REACHED(); }

    // m_handle is a handle of the connection.
    // m_handle == 0 means this channel is closed.
    OwnPtr<WebKit::WebSocketHandle> m_handle;

    // m_client can be deleted while this channel is alive, but this class
    // expects that disconnect() is called before the deletion.
    WebSocketChannelClient* m_client;
    KURL m_url;
    OwnPtr<BlobLoader> m_blobLoader;
    Deque<Message> m_messages;
    Vector<char> m_receivingMessageData;

    bool m_receivingMessageTypeIsText;
    int64_t m_sendingQuota;
    int64_t m_receivedDataSizeForFlowControl;
    unsigned long m_bufferedAmount;
    size_t m_sentSizeOfTopMessage;
    String m_subprotocol;
    String m_extensions;

    static const int64_t receivedDataSizeForFlowControlHighWaterMark = 1 << 15;
};

} // namespace WebCore

#endif // NewWebSocketChannelImpl_h
