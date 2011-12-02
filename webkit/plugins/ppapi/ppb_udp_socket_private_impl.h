// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_UDP_SOCKET_PRIVATE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_UDP_SOCKET_PRIVATE_IMPL_H_

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/private/udp_socket_private_impl.h"

namespace webkit {
namespace ppapi {

class PPB_UDPSocket_Private_Impl : public ::ppapi::UDPSocketPrivateImpl {
 public:
  static PP_Resource CreateResource(PP_Instance instance);

  virtual void SendBind(const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendRecvFrom(int32_t num_bytes) OVERRIDE;
  virtual void SendSendTo(const std::string& buffer,
                          const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendClose() OVERRIDE;

 private:
  PPB_UDPSocket_Private_Impl(PP_Instance instance, uint32 socket_id);
  virtual ~PPB_UDPSocket_Private_Impl();

  DISALLOW_COPY_AND_ASSIGN(PPB_UDPSocket_Private_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_UDP_SOCKET_PRIVATE_IMPL_H_
