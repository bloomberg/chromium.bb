// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_udp_socket_private_proxy.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_udp_socket_private_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_UDPSocket_Private_API;

namespace ppapi {
namespace proxy {

const int32_t kUDPSocketMaxReadSize = 1024 * 1024;
const int32_t kUDPSocketMaxWriteSize = 1024 * 1024;

namespace {

class UDPSocket;

typedef std::map<uint32, UDPSocket*> IDToSocketMap;
IDToSocketMap* g_id_to_socket = NULL;

void AbortCallback(PP_CompletionCallback callback) {
  if (callback.func)
    PP_RunCompletionCallback(&callback, PP_ERROR_ABORTED);
}

class UDPSocket : public PPB_UDPSocket_Private_API,
                  public Resource {
 public:
  UDPSocket(const HostResource& resource, uint32 socket_id);
  virtual ~UDPSocket();

  // ResourceObjectBase overrides.
  virtual PPB_UDPSocket_Private_API* AsPPB_UDPSocket_Private_API() OVERRIDE;

  // PPB_UDPSocket_Private_API implementation.
  virtual int32_t Bind(const PP_NetAddress_Private* addr,
                       PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           PP_CompletionCallback callback) OVERRIDE;
  virtual PP_Bool GetRecvFromAddress(PP_NetAddress_Private* addr) OVERRIDE;

  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_NetAddress_Private* addr,
                         PP_CompletionCallback callback) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Notifications from the proxy.
  void OnBindCompleted(bool succeeded);
  void OnRecvFromCompleted(bool succeeded,
                           const std::string& data,
                           const PP_NetAddress_Private& addr);
  void OnSendToCompleted(bool succeeded,
                         int32_t bytes_written);

 private:
  void PostAbortAndClearIfNecessary(PP_CompletionCallback* callback);

  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  uint32 socket_id_;

  bool binded_;
  bool closed_;

  PP_CompletionCallback bind_callback_;
  PP_CompletionCallback recvfrom_callback_;
  PP_CompletionCallback sendto_callback_;

  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private recvfrom_addr_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocket);
};

UDPSocket::UDPSocket(const HostResource& resource, uint32 socket_id)
    : Resource(resource),
      socket_id_(socket_id),
      binded_(false),
      closed_(false),
      bind_callback_(PP_BlockUntilComplete()),
      recvfrom_callback_(PP_BlockUntilComplete()),
      sendto_callback_(PP_BlockUntilComplete()),
      read_buffer_(NULL),
      bytes_to_read_(-1) {
  DCHECK(socket_id != 0);

  recvfrom_addr_.size = 0;
  memset(recvfrom_addr_.data, 0, sizeof(recvfrom_addr_.data));

  if (!g_id_to_socket)
    g_id_to_socket = new IDToSocketMap();
  DCHECK(g_id_to_socket->find(socket_id) == g_id_to_socket->end());
  (*g_id_to_socket)[socket_id] = this;
}

UDPSocket::~UDPSocket() {
  Close();
}

PPB_UDPSocket_Private_API* UDPSocket::AsPPB_UDPSocket_Private_API() {
  return this;
}

int32_t UDPSocket::Bind(const PP_NetAddress_Private* addr,
                        PP_CompletionCallback callback) {
  if (!addr || !callback.func)
    return PP_ERROR_BADARGUMENT;
  if (binded_ || closed_)
    return PP_ERROR_FAILED;
  if (bind_callback_.func)
    return PP_ERROR_INPROGRESS;

  bind_callback_ = callback;

  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBUDPSocket_Bind(socket_id_, *addr));

  return PP_OK_COMPLETIONPENDING;
}

int32_t UDPSocket::RecvFrom(char* buffer,
                            int32_t num_bytes,
                            PP_CompletionCallback callback) {
  if (!buffer || num_bytes <= 0 || !callback.func)
    return PP_ERROR_BADARGUMENT;
  if (!binded_)
    return PP_ERROR_FAILED;
  if (recvfrom_callback_.func)
    return PP_ERROR_INPROGRESS;

  read_buffer_ = buffer;
  bytes_to_read_ = std::min(num_bytes, kUDPSocketMaxReadSize);
  recvfrom_callback_ = callback;

  // Send the request, the browser will call us back via RecvFromACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBUDPSocket_RecvFrom(
          socket_id_, num_bytes));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocket::GetRecvFromAddress(PP_NetAddress_Private* addr) {
  if (!addr)
    return PP_FALSE;

  *addr = recvfrom_addr_;
  return PP_TRUE;
}

int32_t UDPSocket::SendTo(const char* buffer,
                          int32_t num_bytes,
                          const PP_NetAddress_Private* addr,
                          PP_CompletionCallback callback) {
  if (!buffer || num_bytes <= 0 || !addr || !callback.func)
    return PP_ERROR_BADARGUMENT;
  if (!binded_)
    return PP_ERROR_FAILED;
  if (sendto_callback_.func)
    return PP_ERROR_INPROGRESS;

  if (num_bytes > kUDPSocketMaxWriteSize)
    num_bytes = kUDPSocketMaxWriteSize;

  sendto_callback_ = callback;

  // Send the request, the browser will call us back via SendToACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBUDPSocket_SendTo(
          socket_id_, std::string(buffer, num_bytes), *addr));

  return PP_OK_COMPLETIONPENDING;
}

void UDPSocket::Close() {
  if(closed_)
    return;

  binded_ = false;
  closed_ = true;

  // After removed from the mapping, this object won't receive any notfications
  // from the proxy.
  DCHECK(g_id_to_socket->find(socket_id_) != g_id_to_socket->end());
  g_id_to_socket->erase(socket_id_);

  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBUDPSocket_Close(socket_id_));
  socket_id_ = 0;

  PostAbortAndClearIfNecessary(&bind_callback_);
  PostAbortAndClearIfNecessary(&recvfrom_callback_);
  PostAbortAndClearIfNecessary(&sendto_callback_);
}

void UDPSocket::OnBindCompleted(bool succeeded) {
  if (!bind_callback_.func) {
    NOTREACHED();
    return;
  }

  if (succeeded)
    binded_ = true;

  PP_RunAndClearCompletionCallback(&bind_callback_,
                                   succeeded ? PP_OK : PP_ERROR_FAILED);
}

void UDPSocket::OnRecvFromCompleted(bool succeeded,
                                    const std::string& data,
                                    const PP_NetAddress_Private& addr) {
  if (!recvfrom_callback_.func || !read_buffer_) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memcpy(read_buffer_, data.c_str(), data.size());
  }
  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  recvfrom_addr_ = addr;

  PP_RunAndClearCompletionCallback(
      &recvfrom_callback_,
      succeeded ? static_cast<int32_t>(data.size()) :
                  static_cast<int32_t>(PP_ERROR_FAILED));
}

void UDPSocket::OnSendToCompleted(bool succeeded, int32_t bytes_written) {
  if (!sendto_callback_.func) {
    NOTREACHED();
    return;
  }

  PP_RunAndClearCompletionCallback(
      &sendto_callback_,
      succeeded ? bytes_written : static_cast<int32_t>(PP_ERROR_FAILED));
}

void UDPSocket::PostAbortAndClearIfNecessary(
    PP_CompletionCallback* callback) {
  DCHECK(callback);

  if (callback->func) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&AbortCallback, *callback));
    *callback = PP_BlockUntilComplete();
  }
}
}  // namespace

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
