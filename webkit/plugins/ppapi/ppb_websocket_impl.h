// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_

#include <queue>

#include "base/memory/scoped_ptr.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_websocket_api.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSocketClient.h"

struct PPB_Var;

namespace ppapi {
class StringVar;
}

namespace WebKit {
class WebSocket;
}

namespace webkit {
namespace ppapi {

// All implementation is in this class for now. We should move some common
// implementation to shared_impl when we implement proxy interfaces.
class PPB_WebSocket_Impl : public ::ppapi::Resource,
                           public ::ppapi::thunk::PPB_WebSocket_API,
                           public ::WebKit::WebSocketClient {
 public:
  explicit PPB_WebSocket_Impl(PP_Instance instance);
  virtual ~PPB_WebSocket_Impl();

  static PP_Resource Create(PP_Instance instance);

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_WebSocket_API* AsPPB_WebSocket_API() OVERRIDE;

  // PPB_WebSocket_API implementation.
  virtual int32_t Connect(PP_Var url,
                          const PP_Var protocols[],
                          uint32_t protocol_count,
                          PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Close(uint16_t code,
                        PP_Var reason,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t ReceiveMessage(PP_Var* message,
                                 PP_CompletionCallback callbac) OVERRIDE;
  virtual int32_t SendMessage(PP_Var message) OVERRIDE;
  virtual uint64_t GetBufferedAmount() OVERRIDE;
  virtual uint16_t GetCloseCode() OVERRIDE;
  virtual PP_Var GetCloseReason() OVERRIDE;
  virtual PP_Bool GetCloseWasClean() OVERRIDE;
  virtual PP_Var GetExtensions() OVERRIDE;
  virtual PP_Var GetProtocol() OVERRIDE;
  virtual PP_WebSocketReadyState_Dev GetReadyState() OVERRIDE;
  virtual PP_Var GetURL() OVERRIDE;

  // WebSocketClient implementation.
  virtual void didConnect() OVERRIDE;
  virtual void didReceiveMessage(const WebKit::WebString& message) OVERRIDE;
  virtual void didReceiveBinaryData(
      const WebKit::WebData& binaryData) OVERRIDE;
  virtual void didReceiveMessageError() OVERRIDE;
  // TODO(toyoshim): Add OVERRIDE after landing WebKit side change.
  virtual void didUpdateBufferedAmount(unsigned long buffered_amount);
  virtual void didStartClosingHandshake() OVERRIDE;
  virtual void didClose(unsigned long buffered_amount,
                        ClosingHandshakeCompletionStatus status,
                        unsigned short code,
                        const WebKit::WebString& reason) OVERRIDE;
 private:
  int32_t DoReceive();

  scoped_ptr<WebKit::WebSocket> websocket_;
  PP_WebSocketReadyState_Dev state_;

  PP_CompletionCallback connect_callback_;

  PP_CompletionCallback receive_callback_;
  PP_Var* receive_callback_var_;
  bool wait_for_receive_;
  // TODO(toyoshim): Use std::queue<Var> when it supports binary.
  std::queue<PP_Var> received_messages_;

  PP_CompletionCallback close_callback_;
  uint16_t close_code_;
  scoped_refptr< ::ppapi::StringVar> close_reason_;
  PP_Bool close_was_clean_;

  scoped_refptr< ::ppapi::StringVar> empty_string_;
  scoped_refptr< ::ppapi::StringVar> extensions_;
  scoped_refptr< ::ppapi::StringVar> url_;

  uint64_t buffered_amount_;
  uint64_t buffered_amount_after_close_;

  DISALLOW_COPY_AND_ASSIGN(PPB_WebSocket_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_
