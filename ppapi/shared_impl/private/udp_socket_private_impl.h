// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_UDP_SOCKET_PRIVATE_IMPL_H_
#define PPAPI_SHARED_IMPL_PRIVATE_UDP_SOCKET_PRIVATE_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_udp_socket_private_api.h"

namespace ppapi {

// This class provides the shared implementation of a
// PPB_UDPSocket_Private.  The functions that actually send messages
// to browser are implemented differently for the proxied and
// non-proxied derived classes.
class PPAPI_SHARED_EXPORT UDPSocketPrivateImpl
    : public thunk::PPB_UDPSocket_Private_API,
      public Resource {
 public:
  // C-tor used in Impl case.
  UDPSocketPrivateImpl(PP_Instance instance, uint32 socket_id);
  // C-tor used in Proxy case.
  UDPSocketPrivateImpl(const HostResource& resource, uint32 socket_id);

  virtual ~UDPSocketPrivateImpl();

  // The maximum number of bytes that each PpapiHostMsg_PPBUDPSocket_RecvFrom
  // message is allowed to request.
  static const int32_t kMaxReadSize;
  // The maximum number of bytes that each PpapiHostMsg_PPBUDPSocket_SendTo
  // message is allowed to carry.
  static const int32_t kMaxWriteSize;

  // Resource overrides.
  virtual PPB_UDPSocket_Private_API* AsPPB_UDPSocket_Private_API() OVERRIDE;

  // PPB_UDPSocket_Private_API implementation.
  virtual int32_t Bind(const PP_NetAddress_Private* addr,
                       PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           PP_CompletionCallback callback) OVERRIDE;
  virtual PP_Bool GetRecvFromAddress(PP_NetAddress_Private* addr) OVERRIDE;
  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_NetAddress_Private* addr,
                         PP_CompletionCallback callback) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Notifications from the proxy.
  void OnBindCompleted(bool succeeded);
  void OnRecvFromCompleted(bool succeeded,
                           const std::string& data,
                           const PP_NetAddress_Private& addr);
  void OnSendToCompleted(bool succeeded, int32_t bytes_written);

  // Send functions that need to be implemented differently for
  // the proxied and non-proxied derived classes.
  virtual void SendBind(const PP_NetAddress_Private& addr) = 0;
  virtual void SendRecvFrom(int32_t num_bytes) = 0;
  virtual void SendSendTo(const std::string& buffer,
                          const PP_NetAddress_Private& addr) = 0;
  virtual void SendClose() = 0;

 protected:
  void Init(uint32 socket_id);
  void PostAbortAndClearIfNecessary(PP_CompletionCallback* callback);

  uint32 socket_id_;

  bool bound_;
  bool closed_;

  PP_CompletionCallback bind_callback_;
  PP_CompletionCallback recvfrom_callback_;
  PP_CompletionCallback sendto_callback_;

  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private recvfrom_addr_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketPrivateImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_UDP_SOCKET_PRIVATE_IMPL_H_
