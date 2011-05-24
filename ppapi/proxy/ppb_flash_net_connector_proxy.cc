// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_net_connector_proxy.h"

#include <algorithm>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

std::string NetAddressToString(const PP_Flash_NetAddress& addr) {
  return std::string(addr.data, std::min(static_cast<size_t>(addr.size),
                                         sizeof(addr.data)));
}

void StringToNetAddress(const std::string& str, PP_Flash_NetAddress* addr) {
  addr->size = std::min(str.size(), sizeof(addr->data));
  memcpy(addr->data, str.data(), addr->size);
}

class AbortCallbackTask : public Task {
 public:
  AbortCallbackTask(PP_CompletionCallback callback)
      : callback_(callback) {}

  virtual void Run() {
    PP_RunCompletionCallback(&callback_, PP_ERROR_ABORTED);
  }

 private:
  PP_CompletionCallback callback_;
};

class FlashNetConnector : public PluginResource {
 public:
  FlashNetConnector(const HostResource& resource)
      : PluginResource(resource),
        callback_(PP_BlockUntilComplete()),
        local_addr_out_(NULL),
        remote_addr_out_(NULL) {
  }
  ~FlashNetConnector() {
    if (callback_.func) {
      MessageLoop::current()->PostTask(FROM_HERE,
                                       new AbortCallbackTask(callback_));
    }
  }

  // Resource overrides.
  virtual FlashNetConnector* AsFlashNetConnector() {
    return this;
  }

  bool HasCallback() const {
    return callback_.func != NULL;
  }

  void SetCallback(const PP_CompletionCallback& callback,
                   PP_FileHandle* socket_out,
                   PP_Flash_NetAddress* local_addr_out,
                   PP_Flash_NetAddress* remote_addr_out) {
    callback_ = callback;
    socket_out_ = socket_out;
    local_addr_out_ = local_addr_out;
    remote_addr_out_ = remote_addr_out;
  }

  void ConnectComplete(int32_t result,
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

    PP_CompletionCallback temp_callback = callback_;
    callback_ = PP_BlockUntilComplete();
    PP_RunCompletionCallback(&temp_callback, result);
  }

 private:
  PP_CompletionCallback callback_;
  PP_FileHandle* socket_out_;
  PP_Flash_NetAddress* local_addr_out_;
  PP_Flash_NetAddress* remote_addr_out_;
};

// Contains the data that the host interface will write to so we can send it
// to the plugin. This is created when a request is initiated, and deleted in
// the callback handler.
struct PPB_Flash_NetConnector_Proxy::ConnectCallbackInfo {
  ConnectCallbackInfo(const HostResource& r) : resource(r), handle(0) {
    local_addr.size = 0;
    remote_addr.size = 0;
  }

  HostResource resource;

  PP_FileHandle handle;
  PP_Flash_NetAddress local_addr;
  PP_Flash_NetAddress remote_addr;
};

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFlashNetConnector_Create(
      INTERFACE_ID_PPB_FLASH_NETCONNECTOR, instance_id, &result));
  if (result.is_null())
    return 0;

  linked_ptr<FlashNetConnector> object(new FlashNetConnector(result));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

PP_Bool IsFlashNetConnector(PP_Resource resource_id) {
  FlashNetConnector* object =
      PluginResource::GetAs<FlashNetConnector>(resource_id);
  return BoolToPPBool(!!object);
}

// Backend for both ConnectTcp and ConnectTcpAddress. To keep things generic,
// the message is passed in (on error, it's deleted).
int32_t ConnectWithMessage(FlashNetConnector* object,
                           IPC::Message* msg,
                           PP_FileHandle* socket_out,
                           struct PP_Flash_NetAddress* local_addr_out,
                           struct PP_Flash_NetAddress* remote_addr_out,
                           struct PP_CompletionCallback callback) {
  scoped_ptr<IPC::Message> msg_deletor(msg);
  if (object->HasCallback())
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  // Send the request, it will call us back via ConnectACK.
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;
  dispatcher->Send(msg_deletor.release());

  object->SetCallback(callback, socket_out, local_addr_out, remote_addr_out);
  return PP_OK_COMPLETIONPENDING;
}

int32_t ConnectTcp(PP_Resource connector_id,
                   const char* host,
                   uint16_t port,
                   PP_FileHandle* socket_out,
                   struct PP_Flash_NetAddress* local_addr_out,
                   struct PP_Flash_NetAddress* remote_addr_out,
                   struct PP_CompletionCallback callback) {
  FlashNetConnector* object =
      PluginResource::GetAs<FlashNetConnector>(connector_id);
  if (!object)
    return PP_ERROR_BADARGUMENT;
  return ConnectWithMessage(
      object,
      new PpapiHostMsg_PPBFlashNetConnector_ConnectTcp(
          INTERFACE_ID_PPB_FLASH_NETCONNECTOR,
          object->host_resource(), host, port),
      socket_out, local_addr_out, remote_addr_out, callback);
}

int32_t ConnectTcpAddress(PP_Resource connector_id,
                          const struct PP_Flash_NetAddress* addr,
                          PP_FileHandle* socket_out,
                          struct PP_Flash_NetAddress* local_addr_out,
                          struct PP_Flash_NetAddress* remote_addr_out,
                          struct PP_CompletionCallback callback) {
  FlashNetConnector* object =
      PluginResource::GetAs<FlashNetConnector>(connector_id);
  if (!object)
    return PP_ERROR_BADARGUMENT;
  return ConnectWithMessage(
      object,
      new PpapiHostMsg_PPBFlashNetConnector_ConnectTcpAddress(
          INTERFACE_ID_PPB_FLASH_NETCONNECTOR,
          object->host_resource(), NetAddressToString(*addr)),
      socket_out, local_addr_out, remote_addr_out, callback);
}

const PPB_Flash_NetConnector flash_netconnector_interface = {
  &Create,
  &IsFlashNetConnector,
  &ConnectTcp,
  &ConnectTcpAddress
};

InterfaceProxy* CreateFlashNetConnectorProxy(Dispatcher* dispatcher,
                                             const void* target_interface) {
  return new PPB_Flash_NetConnector_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Flash_NetConnector_Proxy::PPB_Flash_NetConnector_Proxy(
    Dispatcher* dispatcher, const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Flash_NetConnector_Proxy::~PPB_Flash_NetConnector_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_NetConnector_Proxy::GetInfo() {
  static const Info info = {
    &flash_netconnector_interface,
    PPB_FLASH_NETCONNECTOR_INTERFACE,
    INTERFACE_ID_PPB_FLASH_NETCONNECTOR,
    false,
    &CreateFlashNetConnectorProxy
  };
  return &info;
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

void PPB_Flash_NetConnector_Proxy::OnMsgCreate(PP_Instance instance_id,
                                               HostResource* result) {
  result->SetHostResource(
      instance_id,
      ppb_flash_net_connector_target()->Create(instance_id));
}

void PPB_Flash_NetConnector_Proxy::OnMsgConnectTcp(
    const HostResource& resource,
    const std::string& host,
    uint16_t port) {
  ConnectCallbackInfo* info = new ConnectCallbackInfo(resource);
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Flash_NetConnector_Proxy::OnCompleteCallbackInHost, info);

  int32_t result = ppb_flash_net_connector_target()->ConnectTcp(
      resource.host_resource(), host.c_str(), port,
      &info->handle, &info->local_addr, &info->remote_addr,
      callback.pp_completion_callback());
  if (result != PP_OK_COMPLETIONPENDING)
    OnCompleteCallbackInHost(result, info);
}

void PPB_Flash_NetConnector_Proxy::OnMsgConnectTcpAddress(
    const HostResource& resource,
    const std::string& net_address_as_string) {
  ConnectCallbackInfo* info = new ConnectCallbackInfo(resource);
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Flash_NetConnector_Proxy::OnCompleteCallbackInHost, info);

  PP_Flash_NetAddress net_address;
  StringToNetAddress(net_address_as_string, &net_address);

  int32_t result = ppb_flash_net_connector_target()->ConnectTcpAddress(
      resource.host_resource(), &net_address,
      &info->handle, &info->local_addr, &info->remote_addr,
      callback.pp_completion_callback());
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

  PP_Resource plugin_resource =
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
          host_resource);
  if (!plugin_resource) {
    base::ClosePlatformFile(platform_file);
    return;
  }
  FlashNetConnector* object =
      PluginResource::GetAs<FlashNetConnector>(plugin_resource);
  if (!object) {
    base::ClosePlatformFile(platform_file);
    return;
  }

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
        INTERFACE_ID_PPB_FLASH_NETCONNECTOR,
        info->resource, result,
        dispatcher()->ShareHandleWithRemote(
            static_cast<base::PlatformFile>(info->handle), true),
        NetAddressToString(info->local_addr),
        NetAddressToString(info->remote_addr)));
  } else {
    dispatcher()->Send(new PpapiMsg_PPBFlashNetConnector_ConnectACK(
        INTERFACE_ID_PPB_FLASH_NETCONNECTOR,
        info->resource, result,
        IPC::InvalidPlatformFileForTransit(), std::string(), std::string()));
  }
}

}  // namespace proxy
}  // namespace pp
