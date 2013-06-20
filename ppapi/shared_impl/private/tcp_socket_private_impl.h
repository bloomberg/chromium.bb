// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_TCP_SOCKET_PRIVATE_IMPL_H_
#define PPAPI_SHARED_IMPL_PRIVATE_TCP_SOCKET_PRIVATE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tcp_socket_shared.h"
#include "ppapi/thunk/ppb_tcp_socket_private_api.h"

namespace ppapi {

// This class provides the shared implementation of a
// PPB_TCPSocket_Private.  The functions that actually send messages
// to browser are implemented differently for the proxied and
// non-proxied derived classes.
class PPAPI_SHARED_EXPORT TCPSocketPrivateImpl
    : public thunk::PPB_TCPSocket_Private_API,
      public Resource,
      public TCPSocketShared {
 public:
  // C-tor used in Impl case.
  TCPSocketPrivateImpl(PP_Instance instance, uint32 socket_id);
  // C-tor used in Proxy case.
  TCPSocketPrivateImpl(const HostResource& resource, uint32 socket_id);

  virtual ~TCPSocketPrivateImpl();

  // Resource overrides.
  virtual PPB_TCPSocket_Private_API* AsPPB_TCPSocket_Private_API() OVERRIDE;

  // PPB_TCPSocket_Private_API implementation.
  virtual int32_t Connect(const char* host,
                          uint16_t port,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t ConnectWithNetAddress(
      const PP_NetAddress_Private* addr,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetLocalAddress(PP_NetAddress_Private* local_addr) OVERRIDE;
  virtual PP_Bool GetRemoteAddress(PP_NetAddress_Private* remote_addr) OVERRIDE;
  virtual int32_t SSLHandshake(
      const char* server_name,
      uint16_t server_port,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetServerCertificate() OVERRIDE;
  virtual PP_Bool AddChainBuildingCertificate(PP_Resource certificate,
                                              PP_Bool trusted) OVERRIDE;
  virtual int32_t Read(char* buffer,
                      int32_t bytes_to_read,
                      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual int32_t SetOption(PP_TCPSocketOption_Private name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // TCPSocketShared implementation.
  virtual Resource* GetOwnerResource() OVERRIDE;

  // TCPSocketShared overrides.
  virtual int32_t OverridePPError(int32_t pp_error) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TCPSocketPrivateImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_TCP_SOCKET_PRIVATE_IMPL_H_
