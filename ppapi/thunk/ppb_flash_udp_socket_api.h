// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_UDP_SOCKET_API_H_
#define PPAPI_THUNK_PPB_FLASH_UDP_SOCKET_API_H_

#include "ppapi/c/private/ppb_flash_udp_socket.h"

namespace ppapi {
namespace thunk {

class PPB_Flash_UDPSocket_API {
 public:
  virtual ~PPB_Flash_UDPSocket_API() {}

  virtual int32_t Bind(const PP_Flash_NetAddress* addr,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           PP_CompletionCallback callback) = 0;
  virtual PP_Bool GetRecvFromAddress(PP_Flash_NetAddress* addr) = 0;
  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_Flash_NetAddress* addr,
                         PP_CompletionCallback callback) = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FLASH_UDP_SOCKET_API_H_

