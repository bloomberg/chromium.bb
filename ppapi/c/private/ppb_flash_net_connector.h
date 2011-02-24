// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_NET_CONNECTOR_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_NET_CONNECTOR_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash_file.h"  // For |PP_FileHandle|.

#define PPB_FLASH_NETCONNECTOR_INTERFACE "PPB_Flash_NetConnector;1"

// This is an opaque type holding a network address.
struct PP_Flash_NetAddress {
  size_t size;
  char data[128];
};

struct PPB_Flash_NetConnector {
  PP_Resource (*Create)(PP_Instance instance_id);
  PP_Bool (*IsFlashNetConnector)(PP_Resource resource_id);

  // Connect to a TCP port given as a host-port pair. The local and remote
  // addresses of the connection (if successful) are returned in
  // |local_addr_out| and |remote_addr_out|, respectively, if non-null.
  int32_t (*ConnectTcp)(PP_Resource connector_id,
                        const char* host,
                        uint16_t port,
                        PP_FileHandle* socket_out,
                        struct PP_Flash_NetAddress* local_addr_out,
                        struct PP_Flash_NetAddress* remote_addr_out,
                        struct PP_CompletionCallback callback);

  // Same as |ConnectTcp()|, but connecting to the address given by |addr|. A
  // typical use-case would be for reconnections.
  int32_t (*ConnectTcpAddress)(PP_Resource connector_id,
                               const struct PP_Flash_NetAddress* addr,
                               PP_FileHandle* socket_out,
                               struct PP_Flash_NetAddress* local_addr_out,
                               struct PP_Flash_NetAddress* remote_addr_out,
                               struct PP_CompletionCallback callback);
};

#endif  // PPAPI_C_PRIVATE_PPB_FLASH_NET_CONNECTOR_H_
