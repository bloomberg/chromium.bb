// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/network_proxy_dev.h"

#include "ppapi/c/dev/ppb_network_proxy_dev.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkProxy_Dev_0_1>() {
  return PPB_NETWORKPROXY_DEV_INTERFACE_0_1;
}

}  // namespace

// static
bool NetworkProxy::IsAvailable() {
  return has_interface<PPB_NetworkProxy_Dev_0_1>();
}

// static
int32_t NetworkProxy::GetProxyForURL(
    const InstanceHandle& instance,
    const Var& url,
    const CompletionCallbackWithOutput<Var>& callback) {
  if (!has_interface<PPB_NetworkProxy_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_NetworkProxy_Dev_0_1>()->GetProxyForURL(
      instance.pp_instance(), url.pp_var(),
      callback.output(), callback.pp_completion_callback());
}

}  // namespace pp
