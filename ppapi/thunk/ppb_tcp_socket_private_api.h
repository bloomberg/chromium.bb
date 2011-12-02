// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TCP_SOCKET_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_TCP_SOCKET_PRIVATE_API_H_

#include "ppapi/c/private/ppb_tcp_socket_private.h"

namespace ppapi {
namespace thunk {

class PPB_TCPSocket_Private_API {
 public:
  virtual ~PPB_TCPSocket_Private_API() {}

  virtual int32_t Connect(const char* host,
                          uint16_t port,
                          PP_CompletionCallback callback) = 0;
  virtual int32_t ConnectWithNetAddress(const PP_NetAddress_Private* addr,
                                        PP_CompletionCallback callback) = 0;
  virtual PP_Bool GetLocalAddress(PP_NetAddress_Private* local_addr) = 0;
  virtual PP_Bool GetRemoteAddress(PP_NetAddress_Private* remote_addr) = 0;
  virtual int32_t SSLHandshake(const char* server_name,
                               uint16_t server_port,
                               PP_CompletionCallback callback) = 0;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback) = 0;
  virtual void Disconnect() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TCP_SOCKET_PRIVATE_API_H_
