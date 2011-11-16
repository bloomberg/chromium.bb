// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_UDP_SOCKET_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_UDP_SOCKET_PRIVATE_API_H_

#include "ppapi/c/private/ppb_udp_socket_private.h"

namespace ppapi {
namespace thunk {

class PPB_UDPSocket_Private_API {
 public:
  virtual ~PPB_UDPSocket_Private_API() {}

  virtual int32_t Bind(const PP_NetAddress_Private* addr,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           PP_CompletionCallback callback) = 0;
  virtual PP_Bool GetRecvFromAddress(PP_NetAddress_Private* addr) = 0;
  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_NetAddress_Private* addr,
                         PP_CompletionCallback callback) = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_UDP_SOCKET_PRIVATE_API_H_
