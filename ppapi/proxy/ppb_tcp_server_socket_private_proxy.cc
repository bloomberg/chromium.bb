// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_tcp_server_socket_private_proxy.h"

#include <cstddef>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_tcp_socket_private_proxy.h"
#include "ppapi/shared_impl/private/ppb_tcp_server_socket_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

class TCPServerSocket : public PPB_TCPServerSocket_Shared {
 public:
  TCPServerSocket(const HostResource& resource, uint32 plugin_dispatcher_id);
  virtual ~TCPServerSocket();

  virtual void OnAcceptCompleted(
      bool succeeded,
      uint32 tcp_socket_id,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr) OVERRIDE;

  virtual void SendListen(const PP_NetAddress_Private& addr,
                          int32_t backlog) OVERRIDE;
  virtual void SendAccept() OVERRIDE;
  virtual void SendStopListening() OVERRIDE;

 private:
  void SendToBrowser(IPC::Message* msg);

  uint32 plugin_dispatcher_id_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocket);
};

TCPServerSocket::TCPServerSocket(const HostResource& resource,
                                 uint32 plugin_dispatcher_id)
    : PPB_TCPServerSocket_Shared(resource),
      plugin_dispatcher_id_(plugin_dispatcher_id) {
}

TCPServerSocket::~TCPServerSocket() {
  StopListening();
}

void TCPServerSocket::OnAcceptCompleted(
    bool succeeded,
    uint32 accepted_socket_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  if (!TrackedCallback::IsPending(accept_callback_) || !tcp_socket_buffer_) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    *tcp_socket_buffer_ =
        PPB_TCPSocket_Private_Proxy::CreateProxyResourceForConnectedSocket(
            pp_instance(),
            accepted_socket_id,
            local_addr,
            remote_addr);
  }
  tcp_socket_buffer_ = NULL;

  accept_callback_->Run(succeeded ? PP_OK : PP_ERROR_FAILED);
}

void TCPServerSocket::SendListen(const PP_NetAddress_Private& addr,
                                 int32_t backlog) {
  SendToBrowser(new PpapiHostMsg_PPBTCPServerSocket_Listen(
      API_ID_PPB_TCPSERVERSOCKET_PRIVATE,
      plugin_dispatcher_id_,
      pp_resource(),
      addr,
      backlog));
}

void TCPServerSocket::SendAccept() {
  SendToBrowser(new PpapiHostMsg_PPBTCPServerSocket_Accept(
      API_ID_PPB_TCPSOCKET_PRIVATE, socket_id_));
}

void TCPServerSocket::SendStopListening() {
  if (socket_id_ != 0) {
    SendToBrowser(new PpapiHostMsg_PPBTCPServerSocket_Destroy(socket_id_));

    PluginDispatcher* dispatcher =
        PluginDispatcher::GetForInstance(host_resource().instance());
    if (dispatcher) {
      InterfaceProxy* proxy =
          dispatcher->GetInterfaceProxy(API_ID_PPB_TCPSERVERSOCKET_PRIVATE);
      PPB_TCPServerSocket_Private_Proxy* server_socket_proxy =
          static_cast<PPB_TCPServerSocket_Private_Proxy*>(proxy);
      server_socket_proxy->ObjectDestroyed(socket_id_);
    }
  }
}

void TCPServerSocket::SendToBrowser(IPC::Message* msg) {
  PluginGlobals::Get()->GetBrowserSender()->Send(msg);
}

}  // namespace

//------------------------------------------------------------------------------

PPB_TCPServerSocket_Private_Proxy::PPB_TCPServerSocket_Private_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_TCPServerSocket_Private_Proxy::~PPB_TCPServerSocket_Private_Proxy() {
}

PP_Resource PPB_TCPServerSocket_Private_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  TCPServerSocket* server_socket =
      new TCPServerSocket(HostResource::MakeInstanceOnly(instance),
                          dispatcher->plugin_dispatcher_id());
  return server_socket->GetReference();
}

void PPB_TCPServerSocket_Private_Proxy::ObjectDestroyed(uint32 socket_id) {
  id_to_server_socket_.erase(socket_id);
}

bool PPB_TCPServerSocket_Private_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_TCPServerSocket_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPServerSocket_ListenACK, OnMsgListenACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPServerSocket_AcceptACK, OnMsgAcceptACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_TCPServerSocket_Private_Proxy::OnMsgListenACK(
    uint32 plugin_dispatcher_id,
    PP_Resource socket_resource,
    uint32 socket_id,
    int32_t status) {
 thunk::EnterResourceNoLock<thunk::PPB_TCPServerSocket_Private_API>
     enter(socket_resource, true);
  if (enter.succeeded()) {
    PPB_TCPServerSocket_Shared* server_socket =
        static_cast<PPB_TCPServerSocket_Shared*>(enter.object());
    if (status == PP_OK)
      id_to_server_socket_[socket_id] = server_socket;
    server_socket->OnListenCompleted(socket_id, status);
  } else if (socket_id != 0 && status == PP_OK) {
    IPC::Message* msg =
        new PpapiHostMsg_PPBTCPServerSocket_Destroy(socket_id);
    PluginGlobals::Get()->GetBrowserSender()->Send(msg);
  }
}

void PPB_TCPServerSocket_Private_Proxy::OnMsgAcceptACK(
    uint32 plugin_dispatcher_id,
    uint32 server_socket_id,
    uint32 accepted_socket_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  IDToServerSocketMap::iterator it =
      id_to_server_socket_.find(server_socket_id);
  if (it != id_to_server_socket_.end()) {
    bool succeeded = (accepted_socket_id != 0);
    it->second->OnAcceptCompleted(succeeded,
                                  accepted_socket_id,
                                  local_addr,
                                  remote_addr);
  } else if (accepted_socket_id != 0) {
    PluginGlobals::Get()->GetBrowserSender()->Send(
        new PpapiHostMsg_PPBTCPSocket_Disconnect(accepted_socket_id));
  }
}

}  // namespace proxy
}  // namespace ppapi
