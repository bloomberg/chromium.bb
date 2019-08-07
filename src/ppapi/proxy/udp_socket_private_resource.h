// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UDP_SOCKET_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_UDP_SOCKET_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/udp_socket_resource_base.h"
#include "ppapi/thunk/ppb_udp_socket_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT UDPSocketPrivateResource
    : public UDPSocketResourceBase,
      public thunk::PPB_UDPSocket_Private_API {
 public:
  UDPSocketPrivateResource(Connection connection, PP_Instance instance);
  ~UDPSocketPrivateResource() override;

  // PluginResource implementation.
  thunk::PPB_UDPSocket_Private_API* AsPPB_UDPSocket_Private_API() override;

  // PPB_UDPSocket_Private_API implementation.
  int32_t SetSocketFeature(PP_UDPSocketFeature_Private name,
                           PP_Var value) override;
  int32_t Bind(const PP_NetAddress_Private* addr,
               scoped_refptr<TrackedCallback> callback) override;
  PP_Bool GetBoundAddress(PP_NetAddress_Private* addr) override;
  int32_t RecvFrom(char* buffer,
                   int32_t num_bytes,
                   scoped_refptr<TrackedCallback> callback) override;
  PP_Bool GetRecvFromAddress(PP_NetAddress_Private* addr) override;
  int32_t SendTo(const char* buffer,
                 int32_t num_bytes,
                 const PP_NetAddress_Private* addr,
                 scoped_refptr<TrackedCallback> callback) override;
  void Close() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UDPSocketPrivateResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UDP_SOCKET_PRIVATE_RESOURCE_H_
