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

#ifndef RTCDataChannelHandler_h
#define RTCDataChannelHandler_h

#include "core/platform/mediastream/RTCDataChannelHandler.h"
#include "core/platform/mediastream/RTCDataChannelHandlerClient.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelHandlerClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class RTCDataChannelHandlerClient;

class RTCDataChannelHandler : public WebKit::WebRTCDataChannelHandlerClient {
public:
    static PassOwnPtr<RTCDataChannelHandler> create(WebKit::WebRTCDataChannelHandler*);
    virtual ~RTCDataChannelHandler();

    void setClient(RTCDataChannelHandlerClient*);

    String label() const;

    // DEPRECATED
    bool isReliable() const;

    bool ordered() const;
    unsigned short maxRetransmitTime() const;
    unsigned short maxRetransmits() const;
    String protocol() const;
    bool negotiated() const;
    unsigned short id() const;

    unsigned long bufferedAmount();
    bool sendStringData(const String&);
    bool sendRawData(const char*, size_t);
    void close();

    // WebKit::WebRTCDataChannelHandlerClient implementation.
    virtual void didChangeReadyState(ReadyState) const OVERRIDE;
    virtual void didReceiveStringData(const WebKit::WebString&) const OVERRIDE;
    virtual void didReceiveRawData(const char*, size_t) const OVERRIDE;
    virtual void didDetectError() const OVERRIDE;

private:
    explicit RTCDataChannelHandler(WebKit::WebRTCDataChannelHandler*);

    OwnPtr<WebKit::WebRTCDataChannelHandler> m_webHandler;
    RTCDataChannelHandlerClient* m_client;
};

} // namespace WebCore

#endif // RTCDataChannelHandler_h
