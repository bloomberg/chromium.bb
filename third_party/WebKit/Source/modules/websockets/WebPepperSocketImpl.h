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

#ifndef WebPepperSocketImpl_h
#define WebPepperSocketImpl_h

#include <memory>
#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebString.h"
#include "public/web/WebPepperSocket.h"
#include "public/web/WebPepperSocketClient.h"

namespace blink {

class WebDocument;
class WebPepperSocketChannelClientProxy;
class WebSocketChannel;
class WebURL;

class WebPepperSocketImpl final : public WebPepperSocket {
 public:
  WebPepperSocketImpl(const WebDocument&, WebPepperSocketClient*);
  ~WebPepperSocketImpl() override;

  // WebPepperSocket implementation.
  void Connect(const WebURL&, const WebString& protocol) override;
  WebString Subprotocol() override;
  bool SendText(const WebString&) override;
  bool SendArrayBuffer(const WebArrayBuffer&) override;
  void Close(int code, const WebString& reason) override;
  void Fail(const WebString& reason) override;
  void Disconnect() override;

  // WebSocketChannelClient methods proxied by
  // WebPepperSocketChannelClientProxy.
  void DidConnect(const String& subprotocol, const String& extensions);
  void DidReceiveTextMessage(const String& payload);
  void DidReceiveBinaryMessage(std::unique_ptr<Vector<char>> payload);
  void DidError();
  void DidConsumeBufferedAmount(unsigned long consumed);
  void DidStartClosingHandshake();
  void DidClose(WebSocketChannelClient::ClosingHandshakeCompletionStatus,
                unsigned short code,
                const String& reason);

 private:
  Persistent<WebSocketChannel> private_;
  WebPepperSocketClient* client_;
  Persistent<WebPepperSocketChannelClientProxy> channel_proxy_;
  WebString subprotocol_;
  bool is_closing_or_closed_;
  // m_bufferedAmount includes m_bufferedAmountAfterClose.
  unsigned long buffered_amount_;
  unsigned long buffered_amount_after_close_;
};

}  // namespace blink

#endif  // WebPepperSocketImpl_h
