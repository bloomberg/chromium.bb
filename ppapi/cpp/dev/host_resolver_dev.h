// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_HOST_RESOLVER_DEV_H_
#define PPAPI_CPP_DEV_HOST_RESOLVER_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/dev/ppb_host_resolver_dev.h"
#include "ppapi/cpp/dev/net_address_dev.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;

class HostResolver_Dev : public Resource {
 public:
  HostResolver_Dev();

  explicit HostResolver_Dev(const InstanceHandle& instance);

  HostResolver_Dev(PassRef, PP_Resource resource);

  HostResolver_Dev(const HostResolver_Dev& other);

  virtual ~HostResolver_Dev();

  HostResolver_Dev& operator=(const HostResolver_Dev& other);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t Resolve(const char* host,
                  uint16_t port,
                  const PP_HostResolver_Hint_Dev& hint,
                  const CompletionCallback& callback);
  Var GetCanonicalName() const;
  uint32_t GetNetAddressCount() const;
  NetAddress_Dev GetNetAddress(uint32_t index) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_HOST_RESOLVER_DEV_H_
