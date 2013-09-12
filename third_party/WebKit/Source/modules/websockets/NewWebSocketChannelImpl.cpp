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
#include "modules/websockets/NewWebSocketChannelImpl.h"

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/platform/NotImplemented.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSocketHandle.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

// FIXME: We should implement Inspector notification.
// FIXME: We should add log messages.

using WebKit::WebSocketHandle;

namespace WebCore {

namespace {

struct PendingEvent {
    enum Type {
        DidConnectComplete,
        DidReceiveTextMessage,
        DidReceiveBinaryMessage,
        DidReceiveError,
        DidClose,
    };
    Type type;
    Vector<char> message; // for DidReceiveTextMessage / DidReceiveBinaryMessage
    int closingCode; // for DidClose
    String closingReason; // for DidClose

    explicit PendingEvent(Type type) : type(type), closingCode(0) { }
    PendingEvent(Type, Vector<char>*);
    PendingEvent(int code, const String& reason) : type(DidClose), closingCode(code), closingReason(reason) { }
};

class PendingEventProcessor : public RefCounted<PendingEventProcessor> {
public:
    PendingEventProcessor() : m_isAborted(false) { }
    virtual ~PendingEventProcessor() { }
    void abort() { m_isAborted = true; }
    void append(const PendingEvent& e) { m_events.append(e); }
    void process(NewWebSocketChannelImpl*);

private:
    bool m_isAborted;
    Vector<PendingEvent> m_events;
};

PendingEvent::PendingEvent(Type type, Vector<char>* data)
    : type(type)
    , closingCode(0)
{
    ASSERT(type == DidReceiveTextMessage || type == DidReceiveBinaryMessage);
    message.swap(*data);
}

void PendingEventProcessor::process(NewWebSocketChannelImpl* channel)
{
    RefPtr<PendingEventProcessor> protect(this);
    for (size_t i = 0; i < m_events.size() && !m_isAborted; ++i) {
        PendingEvent& event = m_events[i];
        switch (event.type) {
        case PendingEvent::DidConnectComplete:
            channel->handleDidConnect();
            // |this| can be detached here.
            break;
        case PendingEvent::DidReceiveTextMessage:
            channel->handleTextMessage(&event.message);
            // |this| can be detached here.
            break;
        case PendingEvent::DidReceiveBinaryMessage:
            channel->handleBinaryMessage(&event.message);
            // |this| can be detached here.
            break;
        case PendingEvent::DidReceiveError:
            channel->handleDidReceiveMessageError();
            // |this| can be detached here.
            break;
        case PendingEvent::DidClose:
            channel->handleDidClose(event.closingCode, event.closingReason);
            // |this| can be detached here.
            break;
        }
    }
    m_events.clear();
}

bool isClean(int code)
{
    return code == WebSocketChannel::CloseEventCodeNormalClosure
        || (WebSocketChannel::CloseEventCodeMinimumUserDefined <= code
        && code <= WebSocketChannel::CloseEventCodeMaximumUserDefined);
}

} // namespace

class NewWebSocketChannelImpl::BlobLoader : public FileReaderLoaderClient {
public:
    BlobLoader(const Blob&, NewWebSocketChannelImpl*);
    virtual ~BlobLoader() { }

    void cancel();

    // FileReaderLoaderClient functions.
    virtual void didStartLoading() OVERRIDE { }
    virtual void didReceiveData() OVERRIDE { }
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

private:
    NewWebSocketChannelImpl* m_channel;
    FileReaderLoader m_loader;
};

NewWebSocketChannelImpl::BlobLoader::BlobLoader(const Blob& blob, NewWebSocketChannelImpl* channel)
    : m_channel(channel)
    , m_loader(FileReaderLoader::ReadAsArrayBuffer, this)
{
    m_loader.start(channel->scriptExecutionContext(), blob);
}

void NewWebSocketChannelImpl::BlobLoader::cancel()
{
    m_loader.cancel();
    // didFail will be called immediately.
    // |this| is deleted here.
}

void NewWebSocketChannelImpl::BlobLoader::didFinishLoading()
{
    m_channel->didFinishLoadingBlob(m_loader.arrayBufferResult());
    // |this| is deleted here.
}

void NewWebSocketChannelImpl::BlobLoader::didFail(FileError::ErrorCode errorCode)
{
    m_channel->didFailLoadingBlob(errorCode);
    // |this| is deleted here.
}

class NewWebSocketChannelImpl::Resumer {
public:
    explicit Resumer(NewWebSocketChannelImpl*);
    ~Resumer();

    void append(const PendingEvent&);
    void suspend();
    void resumeLater();
    void abort();

private:
    void resumeNow(Timer<Resumer>*);

    NewWebSocketChannelImpl* m_channel;
    RefPtr<PendingEventProcessor> m_pendingEventProcessor;
    Timer<Resumer> m_timer;
    bool m_isAborted;
};

NewWebSocketChannelImpl::Resumer::Resumer(NewWebSocketChannelImpl* channel)
    : m_channel(channel)
    , m_pendingEventProcessor(adoptRef(new PendingEventProcessor))
    , m_timer(this, &Resumer::resumeNow)
    , m_isAborted(false) { }

NewWebSocketChannelImpl::Resumer::~Resumer()
{
    abort();
}

void NewWebSocketChannelImpl::Resumer::append(const PendingEvent& event)
{
    if (m_isAborted)
        return;
    m_pendingEventProcessor->append(event);
}

void NewWebSocketChannelImpl::Resumer::suspend()
{
    if (m_isAborted)
        return;
    m_timer.stop();
    m_channel->m_isSuspended = true;
}

void NewWebSocketChannelImpl::Resumer::resumeLater()
{
    if (m_isAborted)
        return;
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void NewWebSocketChannelImpl::Resumer::abort()
{
    if (m_isAborted)
        return;
    m_isAborted = true;
    m_timer.stop();
    m_pendingEventProcessor->abort();
    m_pendingEventProcessor = 0;
}

void NewWebSocketChannelImpl::Resumer::resumeNow(Timer<Resumer>*)
{
    ASSERT(!m_isAborted);
    m_channel->m_isSuspended = false;

    ASSERT(m_channel->m_client);
    m_pendingEventProcessor->process(m_channel);
    // |this| can be aborted here.
    // |this| can be deleted here.
}

NewWebSocketChannelImpl::NewWebSocketChannelImpl(ScriptExecutionContext* context, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    : ContextLifecycleObserver(context)
    , m_handle(adoptPtr(WebKit::Platform::current()->createWebSocketHandle()))
    , m_client(client)
    , m_resumer(adoptPtr(new Resumer(this)))
    , m_isSuspended(false)
    , m_sendingQuota(0)
    , m_receivedDataSizeForFlowControl(receivedDataSizeForFlowControlHighWaterMark * 2) // initial quota
    , m_bufferedAmount(0)
    , m_sentSizeOfTopMessage(0)
{
}

NewWebSocketChannelImpl::~NewWebSocketChannelImpl()
{
    abortAsyncOperations();
}

void NewWebSocketChannelImpl::connect(const KURL& url, const String& protocol)
{
    if (!m_handle)
        return;
    m_url = url;
    Vector<String> protocols;
    // Since protocol is already verified and escaped, we can simply split it.
    protocol.split(", ", true, protocols);
    WebKit::WebVector<WebKit::WebString> webProtocols(protocols.size());
    for (size_t i = 0; i < protocols.size(); ++i) {
        webProtocols[i] = protocols[i];
    }
    String origin = scriptExecutionContext()->securityOrigin()->toString();
    m_handle->connect(url, webProtocols, origin, this);
    flowControlIfNecessary();
}

String NewWebSocketChannelImpl::subprotocol()
{
    return m_subprotocol;
}

String NewWebSocketChannelImpl::extensions()
{
    return m_extensions;
}

WebSocketChannel::SendResult NewWebSocketChannelImpl::send(const String& message)
{
    m_messages.append(Message(message));
    sendInternal();
    return SendSuccess;
}

WebSocketChannel::SendResult NewWebSocketChannelImpl::send(const Blob& blob)
{
    m_messages.append(Message(blob));
    sendInternal();
    return SendSuccess;
}

WebSocketChannel::SendResult NewWebSocketChannelImpl::send(const ArrayBuffer& buffer, unsigned byteOffset, unsigned byteLength)
{
    // buffer.slice copies its contents.
    m_messages.append(buffer.slice(byteOffset, byteOffset + byteLength));
    sendInternal();
    return SendSuccess;
}

unsigned long NewWebSocketChannelImpl::bufferedAmount() const
{
    return m_bufferedAmount;
}

void NewWebSocketChannelImpl::close(int code, const String& reason)
{
    ASSERT(m_handle);
    m_handle->close(static_cast<unsigned short>(code), reason);
}

void NewWebSocketChannelImpl::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    // m_handle and m_client can be null here.
    if (m_client)
        m_client->didReceiveMessageError();
    if (m_isSuspended) {
        m_resumer->append(PendingEvent(CloseEventCodeAbnormalClosure, reason));
    } else {
        handleDidClose(CloseEventCodeAbnormalClosure, reason);
        // handleDidClose may delete this object.
    }
}

void NewWebSocketChannelImpl::disconnect()
{
    abortAsyncOperations();
    if (m_handle)
        m_handle->close(CloseEventCodeAbnormalClosure, "");
    m_handle.clear();
    m_client = 0;
}

void NewWebSocketChannelImpl::suspend()
{
    m_resumer->suspend();
}

void NewWebSocketChannelImpl::resume()
{
    m_resumer->resumeLater();
}

void NewWebSocketChannelImpl::handleDidConnect()
{
    ASSERT(m_client);
    ASSERT(!m_isSuspended);
    m_client->didConnect();
}

void NewWebSocketChannelImpl::handleTextMessage(Vector<char>* messageData)
{
    ASSERT(m_client);
    ASSERT(!m_isSuspended);
    ASSERT(messageData);
    String message = String::fromUTF8(messageData->data(), messageData->size());
    // For consistency with handleBinaryMessage, we clear |messageData|.
    messageData->clear();
    if (message.isNull()) {
        failAsError("Could not decode a text frame as UTF-8.");
        // failAsError may delete this object.
    } else {
        m_client->didReceiveMessage(message);
    }
}

void NewWebSocketChannelImpl::handleBinaryMessage(Vector<char>* messageData)
{
    ASSERT(m_client);
    ASSERT(!m_isSuspended);
    ASSERT(messageData);
    OwnPtr<Vector<char> > binaryData = adoptPtr(new Vector<char>);
    messageData->swap(*binaryData);
    m_client->didReceiveBinaryData(binaryData.release());
}

void NewWebSocketChannelImpl::handleDidReceiveMessageError()
{
    ASSERT(m_client);
    ASSERT(!m_isSuspended);
    m_client->didReceiveMessageError();
}

void NewWebSocketChannelImpl::handleDidClose(unsigned short code, const String& reason)
{
    ASSERT(!m_isSuspended);
    m_handle.clear();
    abortAsyncOperations();
    if (!m_client) {
        return;
    }
    WebSocketChannelClient* client = m_client;
    m_client = 0;
    WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
        isClean(code) ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete;
    client->didClose(m_bufferedAmount, status, code, reason);
    // client->didClose may delete this object.
}


NewWebSocketChannelImpl::Message::Message(const String& text)
    : type(MessageTypeText)
    , text(text.utf8(String::StrictConversionReplacingUnpairedSurrogatesWithFFFD)) { }

NewWebSocketChannelImpl::Message::Message(const Blob& blob)
    : type(MessageTypeBlob)
    , blob(Blob::create(blob.url(), blob.type(), blob.size())) { }

NewWebSocketChannelImpl::Message::Message(PassRefPtr<ArrayBuffer> arrayBuffer)
    : type(MessageTypeArrayBuffer)
    , arrayBuffer(arrayBuffer) { }

void NewWebSocketChannelImpl::sendInternal()
{
    ASSERT(m_handle);
    unsigned long bufferedAmount = m_bufferedAmount;
    while (!m_messages.isEmpty() && m_sendingQuota > 0 && !m_blobLoader) {
        bool final = false;
        const Message& message = m_messages.first();
        switch (message.type) {
        case MessageTypeText: {
            WebSocketHandle::MessageType type =
                m_sentSizeOfTopMessage ? WebSocketHandle::MessageTypeContinuation : WebSocketHandle::MessageTypeText;
            size_t size = std::min(static_cast<size_t>(m_sendingQuota), message.text.length() - m_sentSizeOfTopMessage);
            final = (m_sendingQuota == size);
            m_handle->send(type, message.text.data() + m_sentSizeOfTopMessage, size, final);
            m_sentSizeOfTopMessage += size;
            m_sendingQuota -= size;
            break;
        }
        case MessageTypeBlob:
            ASSERT(!m_blobLoader);
            m_blobLoader = adoptPtr(new BlobLoader(*message.blob, this));
            break;
        case MessageTypeArrayBuffer: {
            WebSocketHandle::MessageType type =
                m_sentSizeOfTopMessage ? WebSocketHandle::MessageTypeContinuation : WebSocketHandle::MessageTypeBinary;
            size_t size = std::min(static_cast<size_t>(m_sendingQuota), message.arrayBuffer->byteLength() - m_sentSizeOfTopMessage);
            final = (m_sendingQuota == size);
            m_handle->send(type, static_cast<const char*>(message.arrayBuffer->data()) + m_sentSizeOfTopMessage, size, final);
            m_sentSizeOfTopMessage += size;
            m_sendingQuota -= size;
            break;
        }
        }
        if (final) {
            m_messages.removeFirst();
            m_sentSizeOfTopMessage = 0;
        }
    }
    if (m_client && m_bufferedAmount != bufferedAmount) {
        m_client->didUpdateBufferedAmount(m_bufferedAmount);
    }
}

void NewWebSocketChannelImpl::flowControlIfNecessary()
{
    if (!m_handle || m_receivedDataSizeForFlowControl < receivedDataSizeForFlowControlHighWaterMark) {
        return;
    }
    m_handle->flowControl(m_receivedDataSizeForFlowControl);
    m_receivedDataSizeForFlowControl = 0;
}

void NewWebSocketChannelImpl::abortAsyncOperations()
{
    if (m_blobLoader) {
        m_blobLoader->cancel();
        m_blobLoader.clear();
    }
    m_resumer->abort();
}

void NewWebSocketChannelImpl::didConnect(WebSocketHandle* handle, bool succeed, const WebKit::WebString& selectedProtocol, const WebKit::WebString& extensions)
{
    ASSERT(m_handle);
    ASSERT(handle == m_handle);
    ASSERT(m_client);
    if (!succeed) {
        failAsError("Cannot connect to " + m_url.string() + ".");
        // failAsError may delete this object.
        return;
    }
    m_subprotocol = selectedProtocol;
    m_extensions = extensions;
    if (m_isSuspended)
        m_resumer->append(PendingEvent(PendingEvent::DidConnectComplete));
    else
        m_client->didConnect();
}

void NewWebSocketChannelImpl::didReceiveData(WebSocketHandle* handle, WebSocketHandle::MessageType type, const char* data, size_t size, bool fin)
{
    ASSERT(m_handle);
    ASSERT(handle == m_handle);
    ASSERT(m_client);
    // Non-final frames cannot be empty.
    ASSERT(fin || size);
    switch (type) {
    case WebSocketHandle::MessageTypeText:
        ASSERT(m_receivingMessageData.isEmpty());
        m_receivingMessageTypeIsText = true;
        break;
    case WebSocketHandle::MessageTypeBinary:
        ASSERT(m_receivingMessageData.isEmpty());
        m_receivingMessageTypeIsText = false;
        break;
    case WebSocketHandle::MessageTypeContinuation:
        ASSERT(!m_receivingMessageData.isEmpty());
        break;
    }
    m_receivingMessageData.append(data, size);
    m_receivedDataSizeForFlowControl += size;
    flowControlIfNecessary();
    if (!fin) {
        return;
    }
    if (m_isSuspended) {
        PendingEvent::Type type =
            m_receivingMessageTypeIsText ? PendingEvent::DidReceiveTextMessage : PendingEvent::DidReceiveBinaryMessage;
        m_resumer->append(PendingEvent(type, &m_receivingMessageData));
        return;
    }
    Vector<char> messageData;
    messageData.swap(m_receivingMessageData);
    if (m_receivingMessageTypeIsText) {
        handleTextMessage(&messageData);
        // handleTextMessage may delete this object.
    } else {
        handleBinaryMessage(&messageData);
    }
}


void NewWebSocketChannelImpl::didClose(WebSocketHandle* handle, unsigned short code, const WebKit::WebString& reason)
{
    ASSERT(m_handle);
    m_handle.clear();
    // FIXME: Maybe we should notify an error to m_client for some didClose messages.
    if (m_isSuspended) {
        m_resumer->append(PendingEvent(code, reason));
    } else {
        handleDidClose(code, reason);
        // handleDidClose may delete this object.
    }
}

void NewWebSocketChannelImpl::didFinishLoadingBlob(PassRefPtr<ArrayBuffer> buffer)
{
    m_blobLoader.clear();
    ASSERT(m_handle);
    // The loaded blob is always placed on m_messages[0].
    ASSERT(m_messages.size() > 0 && m_messages.first().type == MessageTypeBlob);
    // We replace it with the loaded blob.
    m_messages.first() = Message(buffer);
    sendInternal();
}

void NewWebSocketChannelImpl::didFailLoadingBlob(FileError::ErrorCode errorCode)
{
    m_blobLoader.clear();
    if (errorCode == FileError::ABORT_ERR) {
        // The error is caused by cancel().
        return;
    }
    // FIXME: Generate human-friendly reason message.
    failAsError("Failed to load Blob: error code = " + String::number(errorCode));
    // |this| can be deleted here.
}

} // namespace WebCore
