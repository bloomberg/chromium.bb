// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_udp_socket_private_impl.h"

#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

PPB_UDPSocket_Private_Impl::PPB_UDPSocket_Private_Impl(
    PP_Instance instance, uint32 socket_id)
    : ::ppapi::UDPSocketPrivateImpl(instance, socket_id) {
}

PPB_UDPSocket_Private_Impl::~PPB_UDPSocket_Private_Impl() {
  Close();
}

PP_Resource PPB_UDPSocket_Private_Impl::CreateResource(PP_Instance instance) {
  PluginInstance* plugin_instance = HostGlobals::Get()->GetInstance(instance);
  if (!plugin_instance)
    return 0;

  PluginDelegate* pluign_delegate = plugin_instance->delegate();
  uint32 socket_id = pluign_delegate->UDPSocketCreate();
  if (!socket_id)
    return 0;

  return (new PPB_UDPSocket_Private_Impl(instance, socket_id))->GetReference();
}

void PPB_UDPSocket_Private_Impl::SendBind(const PP_NetAddress_Private& addr) {
  PluginDelegate* pluign_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!pluign_delegate)
    return;

  pluign_delegate->UDPSocketBind(this, socket_id_, addr);
}

void PPB_UDPSocket_Private_Impl::SendRecvFrom(int32_t num_bytes) {
  PluginDelegate* pluign_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!pluign_delegate)
    return;

  pluign_delegate->UDPSocketRecvFrom(socket_id_, num_bytes);
}

void PPB_UDPSocket_Private_Impl::SendSendTo(const std::string& buffer,
                                            const PP_NetAddress_Private& addr) {
  PluginDelegate* pluign_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!pluign_delegate)
    return;

  pluign_delegate->UDPSocketSendTo(socket_id_, buffer, addr);
}

void PPB_UDPSocket_Private_Impl::SendClose() {
  PluginDelegate* pluign_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!pluign_delegate)
    return;

  pluign_delegate->UDPSocketClose(socket_id_);
}

}  // namespace ppapi
}  // namespace webkit
