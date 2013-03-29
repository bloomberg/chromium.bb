// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UDP_SOCKET_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_UDP_SOCKET_PRIVATE_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_udp_socket_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT UDPSocketPrivateResource
    : public PluginResource,
      public thunk::PPB_UDPSocket_Private_API {
 public:
  UDPSocketPrivateResource(Connection connection,
                           PP_Instance instance);
  virtual ~UDPSocketPrivateResource();

  // The maximum number of bytes that each PpapiHostMsg_PPBUDPSocket_RecvFrom
  // message is allowed to request.
  static const int32_t kMaxReadSize;
  // The maximum number of bytes that each PpapiHostMsg_PPBUDPSocket_SendTo
  // message is allowed to carry.
  static const int32_t kMaxWriteSize;

  // PluginResource implementation.
  virtual thunk::PPB_UDPSocket_Private_API*
      AsPPB_UDPSocket_Private_API() OVERRIDE;

  // PPB_UDPSocket_Private_API implementation.
  virtual int32_t SetSocketFeature(PP_UDPSocketFeature_Private name,
                                   PP_Var value) OVERRIDE;
  virtual int32_t Bind(const PP_NetAddress_Private* addr,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetBoundAddress(PP_NetAddress_Private* addr) OVERRIDE;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetRecvFromAddress(PP_NetAddress_Private* addr) OVERRIDE;
  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_NetAddress_Private* addr,
                         scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  void PostAbortIfNecessary(scoped_refptr<TrackedCallback>* callback);

  void SendBoolSocketFeature(int32_t name, bool value);
  void SendBind(const PP_NetAddress_Private& addr);
  void SendRecvFrom(int32_t num_bytes);
  void SendSendTo(const std::string& buffer,
                  const PP_NetAddress_Private& addr);
  void SendClose();

  // IPC message handlers.
  void OnPluginMsgBindReply(const ResourceMessageReplyParams& params,
                            const PP_NetAddress_Private& bound_addr);
  void OnPluginMsgRecvFromReply(const ResourceMessageReplyParams& params,
                                const std::string& data,
                                const PP_NetAddress_Private& addr);
  void OnPluginMsgSendToReply(const ResourceMessageReplyParams& params,
                              int32_t bytes_written);

  bool bound_;
  bool closed_;

  scoped_refptr<TrackedCallback> bind_callback_;
  scoped_refptr<TrackedCallback> recvfrom_callback_;
  scoped_refptr<TrackedCallback> sendto_callback_;

  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private recvfrom_addr_;
  PP_NetAddress_Private bound_addr_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketPrivateResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UDP_SOCKET_PRIVATE_RESOURCE_H_
