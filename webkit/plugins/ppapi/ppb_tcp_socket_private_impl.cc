// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_tcp_socket_private_impl.h"

#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

PPB_TCPSocket_Private_Impl::PPB_TCPSocket_Private_Impl(
    PP_Instance instance, uint32 socket_id)
    : ::ppapi::TCPSocketPrivateImpl(instance, socket_id) {
}

PPB_TCPSocket_Private_Impl::~PPB_TCPSocket_Private_Impl() {
  Disconnect();
}

PP_Resource PPB_TCPSocket_Private_Impl::CreateResource(PP_Instance instance) {
  PluginDelegate* plugin_delegate = GetPluginDelegate(instance);
  if (!plugin_delegate)
    return 0;

  uint32 socket_id = plugin_delegate->TCPSocketCreate();
  if (!socket_id)
    return 0;

  return (new PPB_TCPSocket_Private_Impl(instance, socket_id))->GetReference();
}

PP_Resource PPB_TCPSocket_Private_Impl::CreateConnectedSocket(
    PP_Instance instance,
    uint32 socket_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  PluginDelegate* plugin_delegate = GetPluginDelegate(instance);
  if (!plugin_delegate)
    return 0;

  PPB_TCPSocket_Private_Impl* socket =
      new PPB_TCPSocket_Private_Impl(instance, socket_id);

  socket->connection_state_ = PPB_TCPSocket_Private_Impl::CONNECTED;
  socket->local_addr_ = local_addr;
  socket->remote_addr_ = remote_addr;

  plugin_delegate->RegisterTCPSocket(socket, socket_id);

  return socket->GetReference();
}

void PPB_TCPSocket_Private_Impl::SendConnect(const std::string& host,
                                             uint16_t port) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketConnect(this, socket_id_, host, port);
}

void PPB_TCPSocket_Private_Impl::SendConnectWithNetAddress(
    const PP_NetAddress_Private& addr) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketConnectWithNetAddress(this, socket_id_, addr);
}

void PPB_TCPSocket_Private_Impl::SendSSLHandshake(
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketSSLHandshake(socket_id_, server_name, server_port,
                                         trusted_certs, untrusted_certs);
}

void PPB_TCPSocket_Private_Impl::SendRead(int32_t bytes_to_read) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketRead(socket_id_, bytes_to_read);
}


void PPB_TCPSocket_Private_Impl::SendWrite(const std::string& buffer) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketWrite(socket_id_, buffer);
}

void PPB_TCPSocket_Private_Impl::SendDisconnect() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketDisconnect(socket_id_);
}

void PPB_TCPSocket_Private_Impl::SendSetBoolOption(
    PP_TCPSocketOption_Private name,
    bool value) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPSocketSetBoolOption(socket_id_, name, value);
}

PluginDelegate* PPB_TCPSocket_Private_Impl::GetPluginDelegate(
    PP_Instance instance) {
  PluginInstance* plugin_instance = HostGlobals::Get()->GetInstance(instance);
  if (!plugin_instance)
    return NULL;
  return plugin_instance->delegate();
}

}  // namespace ppapi
}  // namespace webkit
