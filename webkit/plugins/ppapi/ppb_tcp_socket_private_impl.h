// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_TCP_SOCKET_PRIVATE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_TCP_SOCKET_PRIVATE_IMPL_H_

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"

namespace webkit {
namespace ppapi {

class PPB_TCPSocket_Private_Impl : public ::ppapi::TCPSocketPrivateImpl {
 public:
  static PP_Resource CreateResource(PP_Instance instance);

  virtual void SendConnect(const std::string& host, uint16_t port) OVERRIDE;
  virtual void SendConnectWithNetAddress(
      const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendSSLHandshake(const std::string& server_name,
                                uint16_t server_port) OVERRIDE;
  virtual void SendRead(int32_t bytes_to_read) OVERRIDE;
  virtual void SendWrite(const std::string& buffer) OVERRIDE;
  virtual void SendDisconnect() OVERRIDE;

 private:
  PPB_TCPSocket_Private_Impl(PP_Instance instance, uint32 socket_id);
  virtual ~PPB_TCPSocket_Private_Impl();

  DISALLOW_COPY_AND_ASSIGN(PPB_TCPSocket_Private_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_TCP_SOCKET_PRIVATE_IMPL_H_
