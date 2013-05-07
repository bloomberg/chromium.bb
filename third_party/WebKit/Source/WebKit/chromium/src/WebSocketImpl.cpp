/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "WebSocketImpl.h"

#include "RuntimeEnabledFeatures.h"
#include "WebArrayBuffer.h"
#include "WebDocument.h"
#include "WebSocketClient.h"
#include "core/dom/Document.h"
#include "core/page/ConsoleTypes.h"
#include "core/platform/KURL.h"
#include "modules/websockets/MainThreadWebSocketChannel.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"

#include <public/WebString.h>
#include <public/WebURL.h>
#include <wtf/ArrayBuffer.h>

using namespace WebCore;

namespace WebKit {

WebSocketImpl::WebSocketImpl(const WebDocument& document, WebSocketClient* client)
    : m_client(client)
    , m_binaryType(BinaryTypeBlob)
{
    if (RuntimeEnabledFeatures::experimentalWebSocketEnabled()) {
        // FIXME: Create an "experimental" WebSocketChannel instead of a MainThreadWebSocketChannel.
        m_private = MainThreadWebSocketChannel::create(PassRefPtr<Document>(document).get(), this);
    } else
        m_private = MainThreadWebSocketChannel::create(PassRefPtr<Document>(document).get(), this);
}

WebSocketImpl::~WebSocketImpl()
{
    m_private->disconnect();
}

WebSocket::BinaryType WebSocketImpl::binaryType() const
{
    return m_binaryType;
}

bool WebSocketImpl::setBinaryType(BinaryType binaryType)
{
    if (binaryType > BinaryTypeArrayBuffer)
        return false;
    m_binaryType = binaryType;
    return true;
}

void WebSocketImpl::connect(const WebURL& url, const WebString& protocol)
{
    m_private->connect(url, protocol);
}

WebString WebSocketImpl::subprotocol()
{
    return m_private->subprotocol();
}

WebString WebSocketImpl::extensions()
{
    return m_private->extensions();
}

bool WebSocketImpl::sendText(const WebString& message)
{
    return m_private->send(message) == WebSocketChannel::SendSuccess;
}

bool WebSocketImpl::sendArrayBuffer(const WebArrayBuffer& webArrayBuffer)
{
    return m_private->send(*PassRefPtr<ArrayBuffer>(webArrayBuffer), 0, webArrayBuffer.byteLength()) == WebSocketChannel::SendSuccess;
}

unsigned long WebSocketImpl::bufferedAmount() const
{
    return m_private->bufferedAmount();
}

void WebSocketImpl::close(int code, const WebString& reason)
{
    m_private->close(code, reason);
}

void WebSocketImpl::fail(const WebString& reason)
{
    m_private->fail(reason, ErrorMessageLevel);
}

void WebSocketImpl::disconnect()
{
    m_private->disconnect();
    m_client = 0;
}

void WebSocketImpl::didConnect()
{
    m_client->didConnect();
}

void WebSocketImpl::didReceiveMessage(const String& message)
{
    m_client->didReceiveMessage(WebString(message));
}

void WebSocketImpl::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    switch (m_binaryType) {
    case BinaryTypeBlob:
        // FIXME: Handle Blob after supporting WebBlob.
        break;
    case BinaryTypeArrayBuffer:
        m_client->didReceiveArrayBuffer(WebArrayBuffer(ArrayBuffer::create(binaryData->data(), binaryData->size())));
        break;
    }
}

void WebSocketImpl::didReceiveMessageError()
{
    m_client->didReceiveMessageError();
}

void WebSocketImpl::didUpdateBufferedAmount(unsigned long bufferedAmount)
{
    m_client->didUpdateBufferedAmount(bufferedAmount);
}

void WebSocketImpl::didStartClosingHandshake()
{
    m_client->didStartClosingHandshake();
}

void WebSocketImpl::didClose(unsigned long bufferedAmount, ClosingHandshakeCompletionStatus status, unsigned short code, const String& reason)
{
    m_client->didClose(bufferedAmount, static_cast<WebSocketClient::ClosingHandshakeCompletionStatus>(status), code, WebString(reason));
}

} // namespace WebKit
