// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_FLASH_UDP_SOCKET_H_
#define PPAPI_CPP_PRIVATE_FLASH_UDP_SOCKET_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_flash_udp_socket.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;

namespace flash {

class UDPSocket : public Resource {
 public:
  explicit UDPSocket(Instance* instance);

  int32_t Bind(const PP_Flash_NetAddress* addr,
               const CompletionCallback& callback);
  int32_t RecvFrom(char* buffer,
                   int32_t num_bytes,
                   const CompletionCallback& callback);
  bool GetRecvFromAddress(PP_Flash_NetAddress* addr);
  int32_t SendTo(const char* buffer,
                 int32_t num_bytes,
                 const PP_Flash_NetAddress* addr,
                 const CompletionCallback& callback);
  void Close();
};

}  // namespace flash
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FLASH_UDP_SOCKET_H_

