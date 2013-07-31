/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/platform/mediastream/RTCDataChannelHandler.h"

#include "core/platform/mediastream/RTCDataChannelHandlerClient.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

PassOwnPtr<RTCDataChannelHandler> RTCDataChannelHandler::create(WebKit::WebRTCDataChannelHandler* webHandler)
{
    return adoptPtr(new RTCDataChannelHandler(webHandler));
}

RTCDataChannelHandler::RTCDataChannelHandler(WebKit::WebRTCDataChannelHandler* webHandler)
    : m_webHandler(adoptPtr(webHandler))
    , m_client(0)
{
}

RTCDataChannelHandler::~RTCDataChannelHandler()
{
}

void RTCDataChannelHandler::setClient(RTCDataChannelHandlerClient* client)
{
    m_client = client;
    m_webHandler->setClient(m_client ? this : 0);
}

String RTCDataChannelHandler::label() const
{
    return m_webHandler->label();
}

bool RTCDataChannelHandler::isReliable() const
{
    return m_webHandler->isReliable();
}

bool RTCDataChannelHandler::ordered() const
{
    return m_webHandler->ordered();
}

unsigned short RTCDataChannelHandler::maxRetransmitTime() const
{
    return m_webHandler->maxRetransmitTime();
}

unsigned short RTCDataChannelHandler::maxRetransmits() const
{
    return m_webHandler->maxRetransmits();
}

String RTCDataChannelHandler::protocol() const
{
    return m_webHandler->protocol();
}

bool RTCDataChannelHandler::negotiated() const
{
    return m_webHandler->negotiated();
}

unsigned short RTCDataChannelHandler::id() const
{
    return m_webHandler->id();
}

unsigned long RTCDataChannelHandler::bufferedAmount()
{
    return m_webHandler->bufferedAmount();
}

bool RTCDataChannelHandler::sendStringData(const String& data)
{
    return m_webHandler->sendStringData(data);
}

bool RTCDataChannelHandler::sendRawData(const char* data, size_t size)
{
    return m_webHandler->sendRawData(data, size);
}

void RTCDataChannelHandler::close()
{
    m_webHandler->close();
}

void RTCDataChannelHandler::didChangeReadyState(WebRTCDataChannelHandlerClient::ReadyState state) const
{
    if (m_client)
        m_client->didChangeReadyState(static_cast<RTCDataChannelHandlerClient::ReadyState>(state));
}

void RTCDataChannelHandler::didReceiveStringData(const WebKit::WebString& data) const
{
    if (m_client)
        m_client->didReceiveStringData(data);
}

void RTCDataChannelHandler::didReceiveRawData(const char* data, size_t size) const
{
    if (m_client)
        m_client->didReceiveRawData(data, size);
}

void RTCDataChannelHandler::didDetectError() const
{
    if (m_client)
        m_client->didDetectError();
}

} // namespace WebCore
