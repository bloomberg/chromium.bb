// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_

#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_websocket_api.h"

namespace webkit {
namespace ppapi {

// All implementation is in this class for now. We should move some common
// implementation to shared_impl when we implement proxy interfaces.
class PPB_WebSocket_Impl : public ::ppapi::Resource,
                           public ::ppapi::thunk::PPB_WebSocket_API {
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

  DISALLOW_COPY_AND_ASSIGN(PPB_WebSocket_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_WEBSOCKET_IMPL_H_
