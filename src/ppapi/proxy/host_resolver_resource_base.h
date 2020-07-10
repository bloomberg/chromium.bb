// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_RESOLVER_RESOURCE_BASE_H_
#define PPAPI_PROXY_HOST_RESOLVER_RESOURCE_BASE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace ppapi {

class TrackedCallback;

struct HostPortPair {
  std::string host;
  uint16_t port;
};

namespace proxy {

class NetAddressResource;

class PPAPI_PROXY_EXPORT HostResolverResourceBase: public PluginResource {
 public:
  HostResolverResourceBase(Connection connection,
                           PP_Instance instance,
                           bool private_api);
  virtual ~HostResolverResourceBase();

  int32_t ResolveImpl(const char* host,
                      uint16_t port,
                      const PP_HostResolver_Private_Hint* hint,
                      scoped_refptr<TrackedCallback> callback);
  PP_Var GetCanonicalNameImpl();
  uint32_t GetSizeImpl();
  scoped_refptr<NetAddressResource> GetNetAddressImpl(uint32_t index);

 private:
  // IPC message handlers.
  void OnPluginMsgResolveReply(
      const ResourceMessageReplyParams& params,
      const std::string& canonical_name,
      const std::vector<PP_NetAddress_Private>& net_address_list);

  void SendResolve(const HostPortPair& host_port,
                   const PP_HostResolver_Private_Hint* hint);

  bool ResolveInProgress() const;

  bool private_api_;

  scoped_refptr<TrackedCallback> resolve_callback_;

  // Set to false if there is a pending resolve request or the previous request
  // failed.
  bool allow_get_results_;
  std::string canonical_name_;
  std::vector<scoped_refptr<NetAddressResource> > net_address_list_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverResourceBase);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_RESOLVER_RESOURCE_BASE_H_
