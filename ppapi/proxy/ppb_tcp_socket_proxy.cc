// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_tcp_socket_proxy.h"

#include <map>

#include "base/logging.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/shared_impl/tcp_socket_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_net_address_api.h"
#include "ppapi/thunk/ppb_tcp_socket_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef thunk::EnterResourceNoLock<thunk::PPB_NetAddress_API>
    EnterNetAddressNoLock;

typedef std::map<uint32, TCPSocketShared*> IDToSocketMap;
IDToSocketMap* g_id_to_socket = NULL;

class TCPSocket : public thunk::PPB_TCPSocket_API,
                  public Resource,
                  public TCPSocketShared {
 public:
  TCPSocket(const HostResource& resource, uint32 socket_id);
  virtual ~TCPSocket();

  // Resource overrides.
  virtual thunk::PPB_TCPSocket_API* AsPPB_TCPSocket_API() OVERRIDE;

  // thunk::PPB_TCPSocket_API implementation.
  virtual int32_t Connect(PP_Resource addr,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetLocalAddress() OVERRIDE;
  virtual PP_Resource GetRemoteAddress() OVERRIDE;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t SetOption(PP_TCPSocket_Option name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // TCPSocketShared implementation.
  virtual void SendConnect(const std::string& host, uint16_t port) OVERRIDE;
  virtual void SendConnectWithNetAddress(
      const PP_NetAddress_Private& addr) OVERRIDE;
  virtual void SendSSLHandshake(
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs) OVERRIDE;
  virtual void SendRead(int32_t bytes_to_read) OVERRIDE;
  virtual void SendWrite(const std::string& buffer) OVERRIDE;
  virtual void SendDisconnect() OVERRIDE;
  virtual void SendSetOption(PP_TCPSocket_Option name,
                             const SocketOptionData& value) OVERRIDE;
  virtual Resource* GetOwnerResource() OVERRIDE;

 private:
  void SendToBrowser(IPC::Message* msg);

  DISALLOW_COPY_AND_ASSIGN(TCPSocket);
};

TCPSocket::TCPSocket(const HostResource& resource, uint32 socket_id)
    : Resource(OBJECT_IS_PROXY, resource),
      TCPSocketShared(OBJECT_IS_PROXY, socket_id) {
  if (!g_id_to_socket)
    g_id_to_socket = new IDToSocketMap();
  DCHECK(g_id_to_socket->find(socket_id) == g_id_to_socket->end());
  (*g_id_to_socket)[socket_id] = this;
}

TCPSocket::~TCPSocket() {
  DisconnectImpl();
}

thunk::PPB_TCPSocket_API* TCPSocket::AsPPB_TCPSocket_API() {
  return this;
}

int32_t TCPSocket::Connect(PP_Resource addr,
                           scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(addr, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  return ConnectWithNetAddressImpl(&enter.object()->GetNetAddressPrivate(),
                                   callback);
}

PP_Resource TCPSocket::GetLocalAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetLocalAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

PP_Resource TCPSocket::GetRemoteAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetRemoteAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

int32_t TCPSocket::Read(char* buffer,
                        int32_t bytes_to_read,
                        scoped_refptr<TrackedCallback> callback) {
  return ReadImpl(buffer, bytes_to_read, callback);
}

int32_t TCPSocket::Write(const char* buffer,
                         int32_t bytes_to_write,
                         scoped_refptr<TrackedCallback> callback) {
  return WriteImpl(buffer, bytes_to_write, callback);
}

void TCPSocket::Close() {
  DisconnectImpl();
}

int32_t TCPSocket::SetOption(PP_TCPSocket_Option name,
                             const PP_Var& value,
                             scoped_refptr<TrackedCallback> callback) {
  return SetOptionImpl(name, value, callback);
}

void TCPSocket::SendConnect(const std::string& host, uint16_t port) {
  NOTREACHED();
}

void TCPSocket::SendConnectWithNetAddress(const PP_NetAddress_Private& addr) {
  SendToBrowser(new PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress(
      API_ID_PPB_TCPSOCKET, socket_id_, addr));
}

void TCPSocket::SendSSLHandshake(
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  NOTREACHED();
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

void TCPSocket::SendSetOption(PP_TCPSocket_Option name,
                              const SocketOptionData& value) {
  SendToBrowser(
      new PpapiHostMsg_PPBTCPSocket_SetOption(socket_id_, name, value));
}

Resource* TCPSocket::GetOwnerResource() {
  return this;
}

void TCPSocket::SendToBrowser(IPC::Message* msg) {
  PluginGlobals::Get()->GetBrowserSender()->Send(msg);
}

}  // namespace

//------------------------------------------------------------------------------

PPB_TCPSocket_Proxy::PPB_TCPSocket_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_TCPSocket_Proxy::~PPB_TCPSocket_Proxy() {
}

// static
PP_Resource PPB_TCPSocket_Proxy::CreateProxyResource(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  uint32 socket_id = 0;
  PluginGlobals::Get()->GetBrowserSender()->Send(
      new PpapiHostMsg_PPBTCPSocket_Create(
          API_ID_PPB_TCPSOCKET, dispatcher->plugin_dispatcher_id(),
          &socket_id));
  if (socket_id == 0)
    return 0;
  return (new TCPSocket(HostResource::MakeInstanceOnly(instance),
                        socket_id))->GetReference();
}

bool PPB_TCPSocket_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_TCPSocket_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_ConnectACK,
                        OnMsgConnectACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_ReadACK, OnMsgReadACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_WriteACK, OnMsgWriteACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_SetOptionACK,
                        OnMsgSetOptionACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_TCPSocket_Proxy::OnMsgConnectACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    int32_t result,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnConnectCompleted(result, local_addr, remote_addr);
}

void PPB_TCPSocket_Proxy::OnMsgReadACK(uint32 /* plugin_dispatcher_id */,
                                       uint32 socket_id,
                                       int32_t result,
                                       const std::string& data) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnReadCompleted(result, data);
}

void PPB_TCPSocket_Proxy::OnMsgWriteACK(uint32 /* plugin_dispatcher_id */,
                                        uint32 socket_id,
                                        int32_t result) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnWriteCompleted(result);
}

void PPB_TCPSocket_Proxy::OnMsgSetOptionACK(uint32 /* plugin_dispatcher_id */,
                                            uint32 socket_id,
                                            int32_t result) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnSetOptionCompleted(result);
}

}  // namespace proxy
}  // namespace ppapi
