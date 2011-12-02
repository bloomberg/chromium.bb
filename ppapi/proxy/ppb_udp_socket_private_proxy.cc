// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_udp_socket_private_proxy.h"

#include <map>

#include "base/logging.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/udp_socket_private_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef std::map<uint32, UDPSocketPrivateImpl*> IDToSocketMap;
IDToSocketMap* g_id_to_socket = NULL;

class UDPSocket : public UDPSocketPrivateImpl {
 public:
  UDPSocket(const HostResource& resource, uint32 socket_id);
  virtual ~UDPSocket();

  virtual void SendBind(const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendRecvFrom(int32_t num_bytes) OVERRIDE;
  virtual void SendSendTo(const std::string& data,
                          const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendClose() OVERRIDE;

 private:
  void SendToBrowser(IPC::Message* msg);

  DISALLOW_COPY_AND_ASSIGN(UDPSocket);
};

UDPSocket::UDPSocket(const HostResource& resource, uint32 socket_id)
    : UDPSocketPrivateImpl(resource, socket_id) {
  if (!g_id_to_socket)
    g_id_to_socket = new IDToSocketMap();
  DCHECK(g_id_to_socket->find(socket_id) == g_id_to_socket->end());
  (*g_id_to_socket)[socket_id] = this;
}

UDPSocket::~UDPSocket() {
  Close();
}

void UDPSocket::SendBind(const PP_NetAddress_Private& addr) {
  SendToBrowser(new PpapiHostMsg_PPBUDPSocket_Bind(socket_id_, addr));
}

void UDPSocket::SendRecvFrom(int32_t num_bytes) {
  SendToBrowser(new PpapiHostMsg_PPBUDPSocket_RecvFrom(socket_id_, num_bytes));
}

void UDPSocket::SendSendTo(const std::string& data,
                           const PP_NetAddress_Private& addr) {
  SendToBrowser(new PpapiHostMsg_PPBUDPSocket_SendTo(socket_id_, data, addr));
}

void UDPSocket::SendClose() {
  // After removed from the mapping, this object won't receive any notifications
  // from the proxy.
  DCHECK(g_id_to_socket->find(socket_id_) != g_id_to_socket->end());
  g_id_to_socket->erase(socket_id_);
  SendToBrowser(new PpapiHostMsg_PPBUDPSocket_Close(socket_id_));
}

void UDPSocket::SendToBrowser(IPC::Message* msg) {
  PluginDispatcher::GetForResource(this)->SendToBrowser(msg);
}

}  // namespace

//------------------------------------------------------------------------------

PPB_UDPSocket_Private_Proxy::PPB_UDPSocket_Private_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_UDPSocket_Private_Proxy::~PPB_UDPSocket_Private_Proxy() {
}

// static
PP_Resource PPB_UDPSocket_Private_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  uint32 socket_id = 0;
  dispatcher->SendToBrowser(new PpapiHostMsg_PPBUDPSocket_Create(
      API_ID_PPB_UDPSOCKET_PRIVATE, dispatcher->plugin_dispatcher_id(),
      &socket_id));
  if (socket_id == 0)
    return 0;

  return (new UDPSocket(HostResource::MakeInstanceOnly(instance),
                        socket_id))->GetReference();
}

bool PPB_UDPSocket_Private_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_UDPSocket_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBUDPSocket_BindACK,
                        OnMsgBindACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBUDPSocket_RecvFromACK,
                        OnMsgRecvFromACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBUDPSocket_SendToACK,
                        OnMsgSendToACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_UDPSocket_Private_Proxy::OnMsgBindACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnBindCompleted(succeeded);
}

void PPB_UDPSocket_Private_Proxy::OnMsgRecvFromACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnRecvFromCompleted(succeeded, data, addr);
}

void PPB_UDPSocket_Private_Proxy::OnMsgSendToACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded,
    int32_t bytes_written) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnSendToCompleted(succeeded, bytes_written);
}

}  // namespace proxy
}  // namespace ppapi
