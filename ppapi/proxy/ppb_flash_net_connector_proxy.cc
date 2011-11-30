// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_net_connector_proxy.h"

#include <algorithm>

#include "base/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_flash_net_connector_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::PPB_Flash_NetConnector_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

std::string NetAddressToString(const PP_NetAddress_Private& addr) {
  return std::string(addr.data, std::min(static_cast<size_t>(addr.size),
                                         sizeof(addr.data)));
}

void StringToNetAddress(const std::string& str, PP_NetAddress_Private* addr) {
  addr->size = std::min(str.size(), sizeof(addr->data));
  memcpy(addr->data, str.data(), addr->size);
}

void AbortCallback(PP_CompletionCallback callback) {
  PP_RunCompletionCallback(&callback, PP_ERROR_ABORTED);
}

class FlashNetConnector : public PPB_Flash_NetConnector_API,
                          public Resource {
 public:
  explicit FlashNetConnector(const HostResource& resource);
  virtual ~FlashNetConnector();

  // Resource overrides.
  virtual PPB_Flash_NetConnector_API* AsPPB_Flash_NetConnector_API() OVERRIDE;

  // PPB_Flash_NetConnector_API implementation.
  virtual int32_t ConnectTcp(const char* host,
                             uint16_t port,
                             PP_FileHandle* socket_out,
                             PP_NetAddress_Private* local_addr_out,
                             PP_NetAddress_Private* remote_addr_out,
                             PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t ConnectTcpAddress(const PP_NetAddress_Private* addr,
                                    PP_FileHandle* socket_out,
                                    PP_NetAddress_Private* local_addr_out,
                                    PP_NetAddress_Private* remote_addr_out,
                                    PP_CompletionCallback callback) OVERRIDE;

  void ConnectComplete(int32_t result,
                       base::PlatformFile file,
                       const std::string& local_addr_as_string,
                       const std::string& remote_addr_as_string);

 private:
  // Backend for both ConnectTcp and ConnectTcpAddress. To keep things generic,
  // the message is passed in (on error, it's deleted).
  int32_t ConnectWithMessage(IPC::Message* msg,
                             PP_FileHandle* socket_out,
                             PP_NetAddress_Private* local_addr_out,
                             PP_NetAddress_Private* remote_addr_out,
                             PP_CompletionCallback callback);

  PP_CompletionCallback callback_;
  PP_FileHandle* socket_out_;
  PP_NetAddress_Private* local_addr_out_;
  PP_NetAddress_Private* remote_addr_out_;
};

FlashNetConnector::FlashNetConnector(const HostResource& resource)
    : Resource(resource),
      callback_(PP_BlockUntilComplete()),
      socket_out_(NULL),
      local_addr_out_(NULL),
      remote_addr_out_(NULL) {
}

FlashNetConnector::~FlashNetConnector() {
  if (callback_.func) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&AbortCallback, callback_));
  }
}

PPB_Flash_NetConnector_API* FlashNetConnector::AsPPB_Flash_NetConnector_API() {
  return this;
}

int32_t FlashNetConnector::ConnectTcp(
    const char* host,
    uint16_t port,
    PP_FileHandle* socket_out,
    PP_NetAddress_Private* local_addr_out,
    PP_NetAddress_Private* remote_addr_out,
    PP_CompletionCallback callback) {
  return ConnectWithMessage(
      new PpapiHostMsg_PPBFlashNetConnector_ConnectTcp(
          API_ID_PPB_FLASH_NETCONNECTOR, host_resource(), host, port),
      socket_out, local_addr_out, remote_addr_out, callback);
}

int32_t FlashNetConnector::ConnectTcpAddress(
    const PP_NetAddress_Private* addr,
    PP_FileHandle* socket_out,
    PP_NetAddress_Private* local_addr_out,
    PP_NetAddress_Private* remote_addr_out,
    PP_CompletionCallback callback) {
  return ConnectWithMessage(
      new PpapiHostMsg_PPBFlashNetConnector_ConnectTcpAddress(
          API_ID_PPB_FLASH_NETCONNECTOR,
          host_resource(), NetAddressToString(*addr)),
      socket_out, local_addr_out, remote_addr_out, callback);
}

void FlashNetConnector::ConnectComplete(
    int32_t result,
    base::PlatformFile file,
    const std::string& local_addr_as_string,
    const std::string& remote_addr_as_string) {
  if (!callback_.func) {
    base::ClosePlatformFile(file);
    return;
  }

  *socket_out_ = static_cast<PP_FileHandle>(file);
  StringToNetAddress(local_addr_as_string, local_addr_out_);
  StringToNetAddress(remote_addr_as_string, remote_addr_out_);

  PP_RunAndClearCompletionCallback(&callback_, result);
}

int32_t FlashNetConnector::ConnectWithMessage(
    IPC::Message* msg,
    PP_FileHandle* socket_out,
    PP_NetAddress_Private* local_addr_out,
    PP_NetAddress_Private* remote_addr_out,
    PP_CompletionCallback callback) {
  scoped_ptr<IPC::Message> msg_deletor(msg);
  if (callback_.func != NULL)
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  // Send the request, it will call us back via ConnectACK.
  PluginDispatcher::GetForResource(this)->Send(msg_deletor.release());

  callback_ = callback;
  socket_out_ = socket_out;
  local_addr_out_ = local_addr_out;
  remote_addr_out_ = remote_addr_out;
  return PP_OK_COMPLETIONPENDING;
}

// Contains the data that the host interface will write to so we can send it
// to the plugin. This is created when a request is initiated, and deleted in
// the callback handler.
struct PPB_Flash_NetConnector_Proxy::ConnectCallbackInfo {
  ConnectCallbackInfo(const HostResource& r) : resource(r), handle(0) {
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));
  }

  HostResource resource;

  PP_FileHandle handle;
  PP_NetAddress_Private local_addr;
  PP_NetAddress_Private remote_addr;
};

PPB_Flash_NetConnector_Proxy::PPB_Flash_NetConnector_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Flash_NetConnector_Proxy::~PPB_Flash_NetConnector_Proxy() {
}

// static
PP_Resource PPB_Flash_NetConnector_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFlashNetConnector_Create(
      API_ID_PPB_FLASH_NETCONNECTOR, instance, &result));
  if (result.is_null())
    return 0;
  return (new FlashNetConnector(result))->GetReference();
}

bool PPB_Flash_NetConnector_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_NetConnector_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashNetConnector_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashNetConnector_ConnectTcp,
                        OnMsgConnectTcp)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashNetConnector_ConnectTcpAddress,
                        OnMsgConnectTcpAddress)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashNetConnector_ConnectACK,
                        OnMsgConnectACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Flash_NetConnector_Proxy::OnMsgCreate(PP_Instance instance,
                                               HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(
        instance,
        enter.functions()->CreateFlashNetConnector(instance));
  }
}

void PPB_Flash_NetConnector_Proxy::OnMsgConnectTcp(
    const HostResource& resource,
    const std::string& host,
    uint16_t port) {
  ConnectCallbackInfo* info = new ConnectCallbackInfo(resource);
  pp::CompletionCallback callback = callback_factory_.NewOptionalCallback(
      &PPB_Flash_NetConnector_Proxy::OnCompleteCallbackInHost, info);

  EnterHostFromHostResource<PPB_Flash_NetConnector_API> enter(resource);
  int32_t result = PP_ERROR_BADRESOURCE;
  if (enter.succeeded()) {
    result = enter.object()->ConnectTcp(
        host.c_str(), port, &info->handle, &info->local_addr,
        &info->remote_addr, callback.pp_completion_callback());
  }
  if (result != PP_OK_COMPLETIONPENDING)
    OnCompleteCallbackInHost(result, info);
}

void PPB_Flash_NetConnector_Proxy::OnMsgConnectTcpAddress(
    const HostResource& resource,
    const std::string& net_address_as_string) {
  ConnectCallbackInfo* info = new ConnectCallbackInfo(resource);
  pp::CompletionCallback callback = callback_factory_.NewOptionalCallback(
      &PPB_Flash_NetConnector_Proxy::OnCompleteCallbackInHost, info);

  PP_NetAddress_Private net_address;
  StringToNetAddress(net_address_as_string, &net_address);

  EnterHostFromHostResource<PPB_Flash_NetConnector_API> enter(resource);
  int32_t result = PP_ERROR_BADRESOURCE;
  if (enter.succeeded()) {
    result = enter.object()->ConnectTcpAddress(
        &net_address, &info->handle, &info->local_addr, &info->remote_addr,
        callback.pp_completion_callback());
  }
  if (result != PP_OK_COMPLETIONPENDING)
    OnCompleteCallbackInHost(result, info);
}

void PPB_Flash_NetConnector_Proxy::OnMsgConnectACK(
    const HostResource& host_resource,
    int32_t result,
    IPC::PlatformFileForTransit handle,
    const std::string& load_addr_as_string,
    const std::string& remote_addr_as_string) {
  base::PlatformFile platform_file =
      IPC::PlatformFileForTransitToPlatformFile(handle);

  EnterPluginFromHostResource<PPB_Flash_NetConnector_API> enter(host_resource);
  if (enter.failed()) {
    base::ClosePlatformFile(platform_file);
    return;
  }
  FlashNetConnector* object = static_cast<FlashNetConnector*>(enter.object());
  object->ConnectComplete(result, platform_file,
                          load_addr_as_string, remote_addr_as_string);
}

void PPB_Flash_NetConnector_Proxy::OnCompleteCallbackInHost(
    int32_t result,
    ConnectCallbackInfo* info) {
  // Callback must always delete the info.
  scoped_ptr<ConnectCallbackInfo> info_deletor(info);

  if (result == PP_OK) {
    dispatcher()->Send(new PpapiMsg_PPBFlashNetConnector_ConnectACK(
        API_ID_PPB_FLASH_NETCONNECTOR,
        info->resource, result,
        dispatcher()->ShareHandleWithRemote(
            static_cast<base::PlatformFile>(info->handle), true),
        NetAddressToString(info->local_addr),
        NetAddressToString(info->remote_addr)));
  } else {
    dispatcher()->Send(new PpapiMsg_PPBFlashNetConnector_ConnectACK(
        API_ID_PPB_FLASH_NETCONNECTOR,
        info->resource, result,
        IPC::InvalidPlatformFileForTransit(), std::string(), std::string()));
  }
}

}  // namespace proxy
}  // namespace ppapi
