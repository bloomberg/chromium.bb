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

#ifndef WebSocketHandleClient_h
#define WebSocketHandleClient_h

#include "WebCommon.h"
#include "public/platform/WebSocketHandle.h"

namespace WebKit {

class WebString;
class WebURL;

// FIXME: This class should replace WebSocketStreamHandleClient.
class WebSocketHandleClient {
public:
    // Called when the handle is opened.
    virtual void didConnect(WebSocketHandle*, bool fail, const WebString& selectedProtocol, const WebString& extensions) = 0;

    // Called when data are received.
    virtual void didReceiveData(WebSocketHandle*, bool fin, WebSocketHandle::MessageType, const char* data, size_t /* size */) = 0;

    // Called when the handle is closed.
    virtual void didClose(WebSocketHandle*, unsigned short code, const WebString& reason) = 0;

    virtual void didReceiveFlowControl(WebSocketHandle*, int64_t quota) = 0;
};

} // namespace WebKit

#endif // WebSocketHandleClient_h
