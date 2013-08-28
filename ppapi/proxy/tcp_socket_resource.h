// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_
#define PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/tcp_socket_resource_base.h"
#include "ppapi/thunk/ppb_tcp_socket_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketResource : public thunk::PPB_TCPSocket_API,
                                             public TCPSocketResourceBase {
 public:
  TCPSocketResource(Connection connection, PP_Instance instance);
  virtual ~TCPSocketResource();

  // PluginResource overrides.
  virtual thunk::PPB_TCPSocket_API* AsPPB_TCPSocket_API() OVERRIDE;

  // thunk::PPB_TCPSocket_API implementation.
  virtual int32_t Connect(PP_Resource addr,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetLocalAddress() OVERRIDE;
  virtual PP_Resource GetRemoteAddress() OVERRIDE;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t SetOption(PP_TCPSocket_Option name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TCPSocketResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_
