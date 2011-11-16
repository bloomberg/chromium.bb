// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_TCP_SOCKET_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_TCP_SOCKET_PRIVATE_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;

class TCPSocketPrivate : public Resource {
 public:
  explicit TCPSocketPrivate(Instance* instance);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t Connect(const char* host,
                  uint16_t port,
                  const CompletionCallback& callback);
  int32_t ConnectWithNetAddress(const PP_NetAddress_Private* addr,
                                const CompletionCallback& callback);
  bool GetLocalAddress(PP_NetAddress_Private* local_addr);
  bool GetRemoteAddress(PP_NetAddress_Private* remote_addr);
  int32_t SSLHandshake(const char* server_name,
                       uint16_t server_port,
                       const CompletionCallback& callback);
  int32_t Read(char* buffer,
               int32_t bytes_to_read,
               const CompletionCallback& callback);
  int32_t Write(const char* buffer,
                int32_t bytes_to_write,
                const CompletionCallback& callback);
  void Disconnect();
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_TCP_SOCKET_PRIVATE_H_
