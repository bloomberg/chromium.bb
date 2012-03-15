// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_HOST_RESOLVER_PRIVATE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_HOST_RESOLVER_PRIVATE_IMPL_H_

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/private/ppb_host_resolver_shared.h"

namespace webkit {
namespace ppapi {

class PPB_HostResolver_Private_Impl : public ::ppapi::PPB_HostResolver_Shared {
 public:
  explicit PPB_HostResolver_Private_Impl(PP_Instance instance);
  virtual ~PPB_HostResolver_Private_Impl();

  virtual void SendResolve(const ::ppapi::HostPortPair& host_port,
                           const PP_HostResolver_Private_Hint* hint) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_HostResolver_Private_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_HOST_RESOLVER_PRIVATE_IMPL_H_
