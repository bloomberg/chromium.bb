// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_NET_CONNECTOR_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_NET_CONNECTOR_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_flash_net_connector_api.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace webkit {
namespace ppapi {

class PPB_Flash_NetConnector_Impl
    : public ::ppapi::Resource,
      public ::ppapi::thunk::PPB_Flash_NetConnector_API {
 public:
  explicit PPB_Flash_NetConnector_Impl(PP_Instance instance);
  virtual ~PPB_Flash_NetConnector_Impl();

  // Resource override.
  virtual ::ppapi::thunk::PPB_Flash_NetConnector_API*
      AsPPB_Flash_NetConnector_API() OVERRIDE;

  // PPB_Flash_NetConnector implementation.
  virtual int32_t ConnectTcp(const char* host,
                             uint16_t port,
                             PP_FileHandle* socket_out,
                             PP_NetAddress_Private* local_addr_out,
                             PP_NetAddress_Private* remote_addr_out,
                             PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t ConnectTcpAddress(const PP_NetAddress_Private* addr,
                                    PP_FileHandle* socket_out,
                                    PP_NetAddress_Private* local_addr_out,
                                    PP_NetAddress_Private* remote_addr_out,
                                    PP_CompletionCallback callback) OVERRIDE;

  // Called to complete |ConnectTcp()| and |ConnectTcpAddress()|.
  WEBKIT_PLUGINS_EXPORT void CompleteConnectTcp(
      PP_FileHandle socket,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr);

 private:
  // Any pending callback (for |ConnectTcp()| or |ConnectTcpAddress()|).
  scoped_refptr<TrackedCompletionCallback> callback_;

  // Output buffers to be filled in when the callback is completed successfully
  // (|{local,remote}_addr_out| are optional and may be null).
  PP_FileHandle* socket_out_;
  PP_NetAddress_Private* local_addr_out_;
  PP_NetAddress_Private* remote_addr_out_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_NetConnector_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_NET_CONNECTOR_IMPL_H_
