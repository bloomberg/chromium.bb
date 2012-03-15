// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_host_resolver_private_impl.h"

#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

PPB_HostResolver_Private_Impl::PPB_HostResolver_Private_Impl(
    PP_Instance instance)
    : ::ppapi::PPB_HostResolver_Shared(instance) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (plugin_delegate)
    plugin_delegate->RegisterHostResolver(this, host_resolver_id_);
}

PPB_HostResolver_Private_Impl::~PPB_HostResolver_Private_Impl() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (plugin_delegate)
    plugin_delegate->UnregisterHostResolver(host_resolver_id_);
}

void PPB_HostResolver_Private_Impl::SendResolve(
    const ::ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint* hint) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;
  plugin_delegate->HostResolverResolve(host_resolver_id_, host_port, hint);
}

}  // namespace ppapi
}  // namespace webkit
