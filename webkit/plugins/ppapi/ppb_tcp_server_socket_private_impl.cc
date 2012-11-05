// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_tcp_server_socket_private_impl.h"

#include "base/logging.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_tcp_socket_private_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

PPB_TCPServerSocket_Private_Impl::PPB_TCPServerSocket_Private_Impl(
    PP_Instance instance)
    : ::ppapi::PPB_TCPServerSocket_Shared(instance) {
}

PPB_TCPServerSocket_Private_Impl::~PPB_TCPServerSocket_Private_Impl() {
  StopListening();
}

PP_Resource PPB_TCPServerSocket_Private_Impl::CreateResource(
    PP_Instance instance) {
  PPB_TCPServerSocket_Private_Impl* socket =
      new PPB_TCPServerSocket_Private_Impl(instance);
  return socket->GetReference();
}

void PPB_TCPServerSocket_Private_Impl::OnAcceptCompleted(
    bool succeeded,
    uint32 accepted_socket_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  if (!::ppapi::TrackedCallback::IsPending(accept_callback_) ||
      !tcp_socket_buffer_) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    *tcp_socket_buffer_ =
        PPB_TCPSocket_Private_Impl::CreateConnectedSocket(pp_instance(),
                                                          accepted_socket_id,
                                                          local_addr,
                                                          remote_addr);
  }
  tcp_socket_buffer_ = NULL;

  accept_callback_->Run(succeeded ? PP_OK : PP_ERROR_FAILED);
}

void PPB_TCPServerSocket_Private_Impl::SendListen(
    const PP_NetAddress_Private& addr,
    int32_t backlog) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPServerSocketListen(pp_resource(), addr, backlog);
}

void PPB_TCPServerSocket_Private_Impl::SendAccept() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPServerSocketAccept(socket_id_);
}

void PPB_TCPServerSocket_Private_Impl::SendStopListening() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return;

  plugin_delegate->TCPServerSocketStopListening(pp_resource(), socket_id_);
}

}  // namespace ppapi
}  // namespace webkit
