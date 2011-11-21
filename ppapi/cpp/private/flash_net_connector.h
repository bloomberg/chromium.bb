// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_FLASH_NET_CONNECTOR_H_
#define PPAPI_CPP_PRIVATE_FLASH_NET_CONNECTOR_H_

#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;

namespace flash {

class NetConnector : public Resource {
 public:
  explicit NetConnector(const Instance& instance);

  int32_t ConnectTcp(const char* host,
                     uint16_t port,
                     PP_FileHandle* socket_out,
                     PP_NetAddress_Private* local_addr_out,
                     PP_NetAddress_Private* remote_addr_out,
                     const CompletionCallback& cc);
  int32_t ConnectTcpAddress(const PP_NetAddress_Private* addr,
                            PP_FileHandle* socket_out,
                            PP_NetAddress_Private* local_addr_out,
                            PP_NetAddress_Private* remote_addr_out,
                            const CompletionCallback& cc);
};

}  // namespace flash
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FLASH_NET_CONNECTOR_H_
