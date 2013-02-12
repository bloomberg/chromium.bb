// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_RESOLVER_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_HOST_RESOLVER_PRIVATE_RESOURCE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_host_resolver_private_api.h"

namespace ppapi {

struct HostPortPair {
  std::string host;
  uint16_t port;
};

namespace proxy {

class PPAPI_PROXY_EXPORT HostResolverPrivateResource
    : public PluginResource,
      public thunk::PPB_HostResolver_Private_API {
 public:
  HostResolverPrivateResource(Connection connection,
                              PP_Instance instance);
  virtual ~HostResolverPrivateResource();

  // PluginResource overrides.
  virtual thunk::PPB_HostResolver_Private_API*
      AsPPB_HostResolver_Private_API() OVERRIDE;

  // PPB_HostResolver_Private_API implementation.
  virtual int32_t Resolve(const char* host,
                          uint16_t port,
                          const PP_HostResolver_Private_Hint* hint,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Var GetCanonicalName() OVERRIDE;
  virtual uint32_t GetSize() OVERRIDE;
  virtual bool GetNetAddress(uint32_t index,
                             PP_NetAddress_Private* address) OVERRIDE;

 private:
  // IPC message handlers.
  void OnPluginMsgResolveReply(
      const ResourceMessageReplyParams& params,
      const std::string& canonical_name,
      const std::vector<PP_NetAddress_Private>& net_address_list);

  void SendResolve(const HostPortPair& host_port,
                   const PP_HostResolver_Private_Hint* hint);

  bool ResolveInProgress() const;

  scoped_refptr<TrackedCallback> resolve_callback_;

  std::string canonical_name_;
  std::vector<PP_NetAddress_Private> net_address_list_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverPrivateResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_RESOLVER_PRIVATE_RESOURCE_H_
