// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_tcp_socket_private_proxy.h"

#include <map>

#include "base/logging.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef std::map<uint32, TCPSocketPrivateImpl*> IDToSocketMap;
IDToSocketMap* g_id_to_socket = NULL;

class TCPSocket : public TCPSocketPrivateImpl {
 public:
  TCPSocket(const HostResource& resource, uint32 socket_id);
  virtual ~TCPSocket();

  virtual void SendConnect(const std::string& host, uint16_t port) OVERRIDE;
  virtual void SendConnectWithNetAddress(
      const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendSSLHandshake(const std::string& server_name,
                                uint16_t server_port) OVERRIDE;
  virtual void SendRead(int32_t bytes_to_read) OVERRIDE;
  virtual void SendWrite(const std::string& buffer) OVERRIDE;
  virtual void SendDisconnect() OVERRIDE;

 private:
  void SendToBrowser(IPC::Message* msg);

  DISALLOW_COPY_AND_ASSIGN(TCPSocket);
};

TCPSocket::TCPSocket(const HostResource& resource, uint32 socket_id)
    : TCPSocketPrivateImpl(resource, socket_id) {
  if (!g_id_to_socket)
    g_id_to_socket = new IDToSocketMap();
  DCHECK(g_id_to_socket->find(socket_id) == g_id_to_socket->end());
  (*g_id_to_socket)[socket_id] = this;
}

TCPSocket::~TCPSocket() {
  Disconnect();
}

void TCPSocket::SendConnect(const std::string& host, uint16_t port) {
  SendToBrowser(new PpapiHostMsg_PPBTCPSocket_Connect(socket_id_, host, port));
}

void TCPSocket::SendConnectWithNetAddress(const PP_NetAddress_Private& addr) {
  SendToBrowser(
      new PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress(socket_id_, addr));
}

void TCPSocket::SendSSLHandshake(const std::string& server_name,
                                 uint16_t server_port) {
  SendToBrowser(new PpapiHostMsg_PPBTCPSocket_SSLHandshake(
      socket_id_, server_name, server_port));
}

void TCPSocket::SendRead(int32_t bytes_to_read) {
  SendToBrowser(new PpapiHostMsg_PPBTCPSocket_Read(socket_id_, bytes_to_read));
}

void TCPSocket::SendWrite(const std::string& buffer) {
  SendToBrowser(new PpapiHostMsg_PPBTCPSocket_Write(socket_id_, buffer));
}

void TCPSocket::SendDisconnect() {
  // After removed from the mapping, this object won't receive any notifications
  // from the proxy.
  DCHECK(g_id_to_socket->find(socket_id_) != g_id_to_socket->end());
  g_id_to_socket->erase(socket_id_);
  SendToBrowser(new PpapiHostMsg_PPBTCPSocket_Disconnect(socket_id_));
}

void TCPSocket::SendToBrowser(IPC::Message* msg) {
  PluginDispatcher::GetForResource(this)->SendToBrowser(msg);
}

}  // namespace

//------------------------------------------------------------------------------

PPB_TCPSocket_Private_Proxy::PPB_TCPSocket_Private_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_TCPSocket_Private_Proxy::~PPB_TCPSocket_Private_Proxy() {
}

// static
PP_Resource PPB_TCPSocket_Private_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  uint32 socket_id = 0;
  dispatcher->SendToBrowser(new PpapiHostMsg_PPBTCPSocket_Create(
      API_ID_PPB_TCPSOCKET_PRIVATE, dispatcher->plugin_dispatcher_id(),
      &socket_id));
  if (socket_id == 0)
    return 0;
  return (new TCPSocket(HostResource::MakeInstanceOnly(instance),
                        socket_id))->GetReference();
}

bool PPB_TCPSocket_Private_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_TCPSocket_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_ConnectACK,
                        OnMsgConnectACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_SSLHandshakeACK,
                        OnMsgSSLHandshakeACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_ReadACK, OnMsgReadACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_WriteACK, OnMsgWriteACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_TCPSocket_Private_Proxy::OnMsgConnectACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnConnectCompleted(succeeded, local_addr, remote_addr);
}

void PPB_TCPSocket_Private_Proxy::OnMsgSSLHandshakeACK(
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
  iter->second->OnSSLHandshakeCompleted(succeeded);
}

void PPB_TCPSocket_Private_Proxy::OnMsgReadACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded,
    const std::string& data) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnReadCompleted(succeeded, data);
}

void PPB_TCPSocket_Private_Proxy::OnMsgWriteACK(
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
  iter->second->OnWriteCompleted(succeeded, bytes_written);
}

}  // namespace proxy
}  // namespace ppapi
