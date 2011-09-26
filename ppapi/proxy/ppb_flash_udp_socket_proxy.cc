// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_udp_socket_proxy.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_flash_udp_socket_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_Flash_UDPSocket_API;

namespace ppapi {
namespace proxy {

const int32_t kFlashUDPSocketMaxReadSize = 1024 * 1024;
const int32_t kFlashUDPSocketMaxWriteSize = 1024 * 1024;

namespace {

class FlashUDPSocket;

typedef std::map<uint32, FlashUDPSocket*> IDToSocketMap;
IDToSocketMap* g_id_to_socket = NULL;

class AbortCallbackTask : public Task {
 public:
  explicit AbortCallbackTask(PP_CompletionCallback callback)
      : callback_(callback) {}
  virtual ~AbortCallbackTask() {}
  virtual void Run() {
    if (callback_.func)
      PP_RunCompletionCallback(&callback_, PP_ERROR_ABORTED);
  }

 private:
  PP_CompletionCallback callback_;
};

InterfaceProxy* CreateFlashUDPSocketProxy(Dispatcher* dispatcher) {
  return new PPB_Flash_UDPSocket_Proxy(dispatcher);
}

class FlashUDPSocket : public PPB_Flash_UDPSocket_API,
                       public Resource {
 public:
  FlashUDPSocket(const HostResource& resource, uint32 socket_id);
  virtual ~FlashUDPSocket();

  // ResourceObjectBase overrides.
  virtual PPB_Flash_UDPSocket_API* AsPPB_Flash_UDPSocket_API() OVERRIDE;

  // PPB_Flash_UDPSocket_API implementation.
  virtual int32_t Bind(const PP_Flash_NetAddress* addr,
                       PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           PP_CompletionCallback callback) OVERRIDE;
  virtual PP_Bool GetRecvFromAddress(PP_Flash_NetAddress* addr) OVERRIDE;

  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_Flash_NetAddress* addr,
                         PP_CompletionCallback callback) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Notifications from the proxy.
  void OnBindCompleted(bool succeeded);
  void OnRecvFromCompleted(bool succeeded,
                           const std::string& data,
                           const PP_Flash_NetAddress& addr);
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

  PP_Flash_NetAddress recvfrom_addr_;

  DISALLOW_COPY_AND_ASSIGN(FlashUDPSocket);
};

FlashUDPSocket::FlashUDPSocket(const HostResource& resource,
                               uint32 socket_id)
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

FlashUDPSocket::~FlashUDPSocket() {
  Close();
}

PPB_Flash_UDPSocket_API* FlashUDPSocket::AsPPB_Flash_UDPSocket_API() {
  return this;
}

int32_t FlashUDPSocket::Bind(const PP_Flash_NetAddress* addr,
                             PP_CompletionCallback callback) {
  if (!addr || !callback.func)
    return PP_ERROR_BADARGUMENT;
  if (binded_ || closed_)
    return PP_ERROR_FAILED;
  if (bind_callback_.func)
    return PP_ERROR_INPROGRESS;

  bind_callback_ = callback;

  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashUDPSocket_Bind(socket_id_, *addr));

  return PP_OK_COMPLETIONPENDING;
}

int32_t FlashUDPSocket::RecvFrom(char* buffer,
                                 int32_t num_bytes,
                                 PP_CompletionCallback callback) {
  if (!buffer || num_bytes <= 0 || !callback.func)
    return PP_ERROR_BADARGUMENT;
  if (!binded_)
    return PP_ERROR_FAILED;
  if (recvfrom_callback_.func)
    return PP_ERROR_INPROGRESS;

  read_buffer_ = buffer;
  bytes_to_read_ = std::min(num_bytes, kFlashUDPSocketMaxReadSize);
  recvfrom_callback_ = callback;

  // Send the request, the browser will call us back via RecvFromACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashUDPSocket_RecvFrom(
          socket_id_, num_bytes));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool FlashUDPSocket::GetRecvFromAddress(PP_Flash_NetAddress* addr) {
  if (!addr)
    return PP_FALSE;

  *addr = recvfrom_addr_;
  return PP_TRUE;
}

int32_t FlashUDPSocket::SendTo(const char* buffer,
                               int32_t num_bytes,
                               const PP_Flash_NetAddress* addr,
                               PP_CompletionCallback callback) {
  if (!buffer || num_bytes <= 0 || !addr || !callback.func)
    return PP_ERROR_BADARGUMENT;
  if (!binded_)
    return PP_ERROR_FAILED;
  if (sendto_callback_.func)
    return PP_ERROR_INPROGRESS;

  if (num_bytes > kFlashUDPSocketMaxWriteSize)
    num_bytes = kFlashUDPSocketMaxWriteSize;

  sendto_callback_ = callback;

  // Send the request, the browser will call us back via SendToACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashUDPSocket_SendTo(
          socket_id_, std::string(buffer, num_bytes), *addr));

  return PP_OK_COMPLETIONPENDING;
}

void FlashUDPSocket::Close() {
  if(closed_)
    return;

  binded_ = false;
  closed_ = true;

  // After removed from the mapping, this object won't receive any notfications
  // from the proxy.
  DCHECK(g_id_to_socket->find(socket_id_) != g_id_to_socket->end());
  g_id_to_socket->erase(socket_id_);

  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashUDPSocket_Close(socket_id_));
  socket_id_ = 0;

  PostAbortAndClearIfNecessary(&bind_callback_);
  PostAbortAndClearIfNecessary(&recvfrom_callback_);
  PostAbortAndClearIfNecessary(&sendto_callback_);
}

void FlashUDPSocket::OnBindCompleted(bool succeeded) {
  if (!bind_callback_.func) {
    NOTREACHED();
    return;
  }

  if (succeeded)
    binded_ = true;

  PP_RunAndClearCompletionCallback(&bind_callback_,
                                   succeeded ? PP_OK : PP_ERROR_FAILED);
}

void FlashUDPSocket::OnRecvFromCompleted(bool succeeded,
                                         const std::string& data,
                                         const PP_Flash_NetAddress& addr) {
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

void FlashUDPSocket::OnSendToCompleted(bool succeeded,
                                       int32_t bytes_written) {
  if (!sendto_callback_.func) {
    NOTREACHED();
    return;
  }

  PP_RunAndClearCompletionCallback(
      &sendto_callback_,
      succeeded ? bytes_written : static_cast<int32_t>(PP_ERROR_FAILED));
}

void FlashUDPSocket::PostAbortAndClearIfNecessary(
    PP_CompletionCallback* callback) {
  DCHECK(callback);

  if (callback->func) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     new AbortCallbackTask(*callback));
    *callback = PP_BlockUntilComplete();
  }
}
}  // namespace

PPB_Flash_UDPSocket_Proxy::PPB_Flash_UDPSocket_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_UDPSocket_Proxy::~PPB_Flash_UDPSocket_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_UDPSocket_Proxy::GetInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_Flash_UDPSocket_Thunk(),
    PPB_FLASH_UDPSOCKET_INTERFACE,
    INTERFACE_ID_PPB_FLASH_UDPSOCKET,
    false,
    &CreateFlashUDPSocketProxy,
  };
  return &info;
}

// static
PP_Resource PPB_Flash_UDPSocket_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  uint32 socket_id = 0;
  dispatcher->SendToBrowser(new PpapiHostMsg_PPBFlashUDPSocket_Create(
      INTERFACE_ID_PPB_FLASH_UDPSOCKET, dispatcher->plugin_dispatcher_id(),
      &socket_id));
  if (socket_id == 0)
    return 0;

  return (new FlashUDPSocket(HostResource::MakeInstanceOnly(instance),
                             socket_id))->GetReference();
}

bool PPB_Flash_UDPSocket_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_UDPSocket_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashUDPSocket_BindACK,
                        OnMsgBindACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashUDPSocket_RecvFromACK,
                        OnMsgRecvFromACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashUDPSocket_SendToACK,
                        OnMsgSendToACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Flash_UDPSocket_Proxy::OnMsgBindACK(
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

void PPB_Flash_UDPSocket_Proxy::OnMsgRecvFromACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded,
    const std::string& data,
    const PP_Flash_NetAddress& addr) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnRecvFromCompleted(succeeded, data, addr);
}

void PPB_Flash_UDPSocket_Proxy::OnMsgSendToACK(
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

