// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(yzshen): This (and the .cc file) contain C++ wrappers for things
// in ppapi/c/private/ppb_flash_tcp_socket.h. This is currently not used in
// (or even compiled with) Chromium.

#ifndef PPAPI_CPP_PRIVATE_FLASH_TCP_SOCKET_H_
#define PPAPI_CPP_PRIVATE_FLASH_TCP_SOCKET_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;

namespace flash {

class TCPSocket : public Resource {
 public:
  explicit TCPSocket(Instance* instance);

  int32_t Connect(const char* host,
                  uint16_t port,
                  const CompletionCallback& callback);
  int32_t ConnectWithNetAddress(const PP_Flash_NetAddress* addr,
                                const CompletionCallback& callback);
  bool GetLocalAddress(PP_Flash_NetAddress* local_addr);
  bool GetRemoteAddress(PP_Flash_NetAddress* remote_addr);
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

}  // namespace flash
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FLASH_TCP_SOCKET_H_
