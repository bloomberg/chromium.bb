// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_NET_CONNECTOR_API_H_
#define PPAPI_THUNK_PPB_FLASH_NET_CONNECTOR_API_H_

#include "ppapi/c/private/ppb_flash_net_connector.h"

namespace ppapi {
namespace thunk {

class PPB_Flash_NetConnector_API {
 public:
  virtual ~PPB_Flash_NetConnector_API() {}

  virtual int32_t ConnectTcp(const char* host,
                             uint16_t port,
                             PP_FileHandle* socket_out,
                             PP_NetAddress_Private* local_addr_out,
                             PP_NetAddress_Private* remote_addr_out,
                             PP_CompletionCallback callback) = 0;
  virtual int32_t ConnectTcpAddress(const PP_NetAddress_Private* addr,
                                    PP_FileHandle* socket_out,
                                    PP_NetAddress_Private* local_addr_out,
                                    PP_NetAddress_Private* remote_addr_out,
                                    PP_CompletionCallback callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FLASH_NET_CONNECTOR_API_H_
