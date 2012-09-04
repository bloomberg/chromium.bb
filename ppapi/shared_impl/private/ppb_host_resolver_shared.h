// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_HOST_RESOLVER_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_HOST_RESOLVER_SHARED_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_host_resolver_private_api.h"

namespace net {
class AddressList;
}

namespace ppapi {

struct HostPortPair {
  std::string host;
  uint16_t port;
};

typedef std::vector<PP_NetAddress_Private> NetAddressList;

#if !defined(OS_NACL) && !defined(NACL_WIN64)
PPAPI_SHARED_EXPORT NetAddressList*
    CreateNetAddressListFromAddressList(const net::AddressList& list);
#endif

class PPAPI_SHARED_EXPORT PPB_HostResolver_Shared
    : public thunk::PPB_HostResolver_Private_API,
      public Resource {
 public:
  // C-tor used in Impl case.
  explicit PPB_HostResolver_Shared(PP_Instance instance);

  // C-tor used in Proxy case.
  explicit PPB_HostResolver_Shared(const HostResource& resource);

  virtual ~PPB_HostResolver_Shared();

  // Resource overrides.
  virtual PPB_HostResolver_Private_API*
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

  void OnResolveCompleted(bool succeeded,
                          const std::string& canonical_name,
                          const NetAddressList& net_address_list);

  // Send functions that need to be implemented differently for the
  // proxied and non-proxied derived classes.
  virtual void SendResolve(const HostPortPair& host_port,
                           const PP_HostResolver_Private_Hint* hint) = 0;

 protected:
  static uint32 GenerateHostResolverID();
  bool ResolveInProgress() const;

  const uint32 host_resolver_id_;

  scoped_refptr<TrackedCallback> resolve_callback_;

  std::string canonical_name_;
  NetAddressList net_address_list_;

  DISALLOW_COPY_AND_ASSIGN(PPB_HostResolver_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_HOST_RESOLVER_SHARED_H_
