// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_WEBSOCKET_API_H_
#define PPAPI_THUNK_WEBSOCKET_API_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/dev/ppb_websocket_dev.h"

namespace ppapi {
namespace thunk {

class PPB_WebSocket_API {
 public:
  virtual ~PPB_WebSocket_API() {}

  virtual int32_t Connect(PP_Var url,
                          const PP_Var protocols[],
                          uint32_t protocol_count,
                          PP_CompletionCallback callback) = 0;
  virtual int32_t Close(uint16_t code,
                        PP_Var reason,
                        PP_CompletionCallback callback) = 0;
  virtual int32_t ReceiveMessage(PP_Var* message,
                                 PP_CompletionCallback callback) = 0;
  virtual int32_t SendMessage(PP_Var message) = 0;
  virtual uint64_t GetBufferedAmount() = 0;
  virtual uint16_t GetCloseCode() = 0;
  virtual PP_Var GetCloseReason() = 0;
  virtual PP_Bool GetCloseWasClean() = 0;
  virtual PP_Var GetExtensions() = 0;
  virtual PP_Var GetProtocol() = 0;
  virtual PP_WebSocketReadyState_Dev GetReadyState() = 0;
  virtual PP_Var GetURL() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_WEBSOCKET_API_H_
