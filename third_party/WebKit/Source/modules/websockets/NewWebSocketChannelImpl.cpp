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

#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/Blob.h"
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
#include "wtf/text/WTFString.h"

// FIXME: We should implement Inspector notification.
// FIXME: We should implement send(Blob).
// FIXME: We should implement suspend / resume.
// FIXME: We should add log messages.

using WebKit::WebSocketHandle;

namespace WebCore {

namespace {

bool isClean(int code)
{
    return code == WebSocketChannel::CloseEventCodeNormalClosure
        || (WebSocketChannel::CloseEventCodeMinimumUserDefined <= code
        && code <= WebSocketChannel::CloseEventCodeMaximumUserDefined);
}

} // namespace

NewWebSocketChannelImpl::NewWebSocketChannelImpl(ScriptExecutionContext* context, WebSocketChannelClient* client, const String& sourceURL, unsigned lineNumber)
    : m_handle(adoptPtr(WebKit::Platform::current()->createWebSocketHandle()))
    , m_client(client)
    , m_origin(context->securityOrigin()->toString())
    , m_sendingQuota(0)
    , m_receivedDataSizeForFlowControl(receivedDataSizeForFlowControlHighWaterMark * 2) // initial quota
    , m_bufferedAmount(0)
    , m_sentSizeOfTopMessage(0)
{
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
    m_handle->connect(url, webProtocols, m_origin, this);
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
    handleDidClose(CloseEventCodeAbnormalClosure, reason);
    // handleDidClose may delete this object.
}

void NewWebSocketChannelImpl::disconnect()
{
    if (m_handle)
        m_handle->close(CloseEventCodeAbnormalClosure, "");
    m_handle.clear();
    m_client = 0;
}

void NewWebSocketChannelImpl::suspend()
{
    notImplemented();
}

void NewWebSocketChannelImpl::resume()
{
    notImplemented();
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
    if (!m_handle || !m_sendingQuota) {
        return;
    }
    unsigned long bufferedAmount = m_bufferedAmount;
    while (!m_messages.isEmpty() && m_sendingQuota > 0) {
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
            notImplemented();
            final = true;
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

void NewWebSocketChannelImpl::didConnect(WebSocketHandle* handle, bool succeed, const WebKit::WebString& selectedProtocol, const WebKit::WebString& extensions)
{
    if (!m_handle) {
        return;
    }
    ASSERT(handle == m_handle);
    ASSERT(m_client);
    if (!succeed) {
        failAsError("Cannot connect to " + m_url.string() + ".");
        // failAsError may delete this object.
        return;
    }
    m_subprotocol = selectedProtocol;
    m_extensions = extensions;
    m_client->didConnect();
}

void NewWebSocketChannelImpl::didReceiveData(WebSocketHandle* handle, WebSocketHandle::MessageType type, const char* data, size_t size, bool fin)
{
    if (!m_handle) {
        return;
    }
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
    // FIXME: Maybe we should notify an error to m_client for some didClose messages.
    handleDidClose(code, reason);
    // handleDidClose may delete this object.
}

void NewWebSocketChannelImpl::handleTextMessage(Vector<char>* messageData)
{
    ASSERT(m_client);
    ASSERT(messageData);
    String message = "";
    if (m_receivingMessageData.size() > 0) {
        message = String::fromUTF8(messageData->data(), messageData->size());
    }
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
    ASSERT(messageData);
    OwnPtr<Vector<char> > binaryData = adoptPtr(new Vector<char>);
    messageData->swap(*binaryData);
    m_client->didReceiveBinaryData(binaryData.release());
}

void NewWebSocketChannelImpl::handleDidClose(unsigned short code, const String& reason)
{
    m_handle.clear();
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

} // namespace WebCore
