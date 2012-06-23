// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_

#include <queue>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_websocket_api.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSocket.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSocketClient.h"

namespace ppapi {
class StringVar;
class Var;
}  // namespace ppapi

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
  // Returns the pointer to the thunk::PPB_WebSocket_API if it's supported.
  virtual ::ppapi::thunk::PPB_WebSocket_API* AsPPB_WebSocket_API() OVERRIDE;

  // PPB_WebSocket_API implementation.
  virtual int32_t Connect(
      PP_Var url,
      const PP_Var protocols[],
      uint32_t protocol_count,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t Close(
      uint16_t code,
      PP_Var reason,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t ReceiveMessage(
      PP_Var* message,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t SendMessage(PP_Var message) OVERRIDE;
  virtual uint64_t GetBufferedAmount() OVERRIDE;
  virtual uint16_t GetCloseCode() OVERRIDE;
  virtual PP_Var GetCloseReason() OVERRIDE;
  virtual PP_Bool GetCloseWasClean() OVERRIDE;
  virtual PP_Var GetExtensions() OVERRIDE;
  virtual PP_Var GetProtocol() OVERRIDE;
  virtual PP_WebSocketReadyState GetReadyState() OVERRIDE;
  virtual PP_Var GetURL() OVERRIDE;

  // WebSocketClient implementation.
  virtual void didConnect();
  virtual void didReceiveMessage(const WebKit::WebString& message);
  virtual void didReceiveArrayBuffer(const WebKit::WebArrayBuffer& binaryData);
  virtual void didReceiveMessageError();
  virtual void didUpdateBufferedAmount(unsigned long buffered_amount);
  virtual void didStartClosingHandshake();
  virtual void didClose(unsigned long unhandled_buffered_amount,
                        ClosingHandshakeCompletionStatus status,
                        unsigned short code,
                        const WebKit::WebString& reason);
 private:
  // Picks up a received message and moves it to user receiving buffer. This
  // function is used in both ReceiveMessage for fast returning path and
  // didReceiveMessage callback.
  int32_t DoReceive();

  // Keeps the WebKit side WebSocket object. This is used for calling WebKit
  // side functions via WebKit API.
  scoped_ptr<WebKit::WebSocket> websocket_;

  // Represents readyState described in the WebSocket API specification. It can
  // be read via GetReadyState().
  PP_WebSocketReadyState state_;

  // Becomes true if any error is detected. Incoming data will be disposed
  // if this variable is true, then ReceiveMessage() returns PP_ERROR_FAILED
  // after returning all received data.
  bool error_was_received_;

  // Keeps a completion callback for asynchronous Connect().
  scoped_refptr< ::ppapi::TrackedCallback> connect_callback_;

  // Keeps a completion callback for asynchronous ReceiveMessage().
  scoped_refptr< ::ppapi::TrackedCallback> receive_callback_;

  // Keeps a pointer to PP_Var which is provided via ReceiveMessage().
  // Received data will be copied to this PP_Var on ready.
  PP_Var* receive_callback_var_;

  // Becomes true when asynchronous ReceiveMessage() is processed.
  bool wait_for_receive_;

  // Keeps received data until ReceiveMessage() requests.
  std::queue< scoped_refptr< ::ppapi::Var> > received_messages_;

  // Keeps a completion callback for asynchronous Close().
  scoped_refptr< ::ppapi::TrackedCallback> close_callback_;

  // Keeps the status code field of closing handshake. It can be read via
  // GetCloseCode().
  uint16_t close_code_;

  // Keeps the reason field of closing handshake. It can be read via
  // GetCloseReason().
  scoped_refptr< ::ppapi::StringVar> close_reason_;

  // Becomes true when closing handshake is performed successfully. It can be
  // read via GetCloseWasClean().
  PP_Bool close_was_clean_;

  // Keeps empty string for functions to return empty string.
  scoped_refptr< ::ppapi::StringVar> empty_string_;

  // Represents extensions described in the WebSocket API specification. It can
  // be read via GetExtensions().
  scoped_refptr< ::ppapi::StringVar> extensions_;

  // Represents url described in the WebSocket API specification. It can be
  // read via GetURL().
  scoped_refptr< ::ppapi::StringVar> url_;

  // Keeps the number of bytes of application data that have been queued using
  // SendMessage(). WebKit side implementation calculates the actual amount.
  // This is a cached value which is notified through a WebKit callback.
  // This value is used to calculate bufferedAmount in the WebSocket API
  // specification. The calculated value can be read via GetBufferedAmount().
  uint64_t buffered_amount_;

  // Keeps the number of bytes of application data that have been ignored
  // because the connection was already closed.
  // This value is used to calculate bufferedAmount in the WebSocket API
  // specification. The calculated value can be read via GetBufferedAmount().
  uint64_t buffered_amount_after_close_;

  DISALLOW_COPY_AND_ASSIGN(PPB_WebSocket_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_
