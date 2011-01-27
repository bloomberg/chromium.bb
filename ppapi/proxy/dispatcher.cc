// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/dispatcher.h"

#include <string.h>  // For memset.

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_test_sink.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_audio_config_proxy.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_char_set_proxy.h"
#include "ppapi/proxy/ppb_core_proxy.h"
#include "ppapi/proxy/ppb_cursor_control_proxy.h"
#include "ppapi/proxy/ppb_flash_proxy.h"
#include "ppapi/proxy/ppb_font_proxy.h"
#include "ppapi/proxy/ppb_fullscreen_proxy.h"
#include "ppapi/proxy/ppb_graphics_2d_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_pdf_proxy.h"
#include "ppapi/proxy/ppb_testing_proxy.h"
#include "ppapi/proxy/ppb_url_loader_proxy.h"
#include "ppapi/proxy/ppb_url_request_info_proxy.h"
#include "ppapi/proxy/ppb_url_response_info_proxy.h"
#include "ppapi/proxy/ppb_var_deprecated_proxy.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/ppp_instance_proxy.h"
#include "ppapi/proxy/var_serialization_rules.h"

namespace pp {
namespace proxy {

Dispatcher::Dispatcher(base::ProcessHandle remote_process_handle,
                       GetInterfaceFunc local_get_interface)
    : pp_module_(0),
      remote_process_handle_(remote_process_handle),
      test_sink_(NULL),
      disallow_trusted_interfaces_(false),  // TODO(brettw) make this settable.
      local_get_interface_(local_get_interface),
      declared_supported_remote_interfaces_(false),
      callback_tracker_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  memset(id_to_proxy_, 0,
         static_cast<int>(INTERFACE_ID_COUNT) * sizeof(InterfaceProxy*));
}

Dispatcher::~Dispatcher() {
}

bool Dispatcher::InitWithChannel(MessageLoop* ipc_message_loop,
                                 const IPC::ChannelHandle& channel_handle,
                                 bool is_client,
                                 base::WaitableEvent* shutdown_event) {
  IPC::Channel::Mode mode = is_client ? IPC::Channel::MODE_CLIENT
                                      : IPC::Channel::MODE_SERVER;
  channel_.reset(new IPC::SyncChannel(channel_handle, mode, this,
                                      ipc_message_loop, false, shutdown_event));
  return true;
}

void Dispatcher::InitWithTestSink(IPC::TestSink* test_sink) {
  DCHECK(!test_sink_);
  test_sink_ = test_sink;
}

bool Dispatcher::OnMessageReceived(const IPC::Message& msg) {
  // Control messages.
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(Dispatcher, msg)
      IPC_MESSAGE_HANDLER(PpapiMsg_DeclareInterfaces,
                          OnMsgDeclareInterfaces)
      IPC_MESSAGE_HANDLER(PpapiMsg_SupportsInterface, OnMsgSupportsInterface)
      IPC_MESSAGE_FORWARD(PpapiMsg_ExecuteCallback, &callback_tracker_,
                          CallbackTracker::ReceiveExecuteSerializedCallback)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  // Interface-specific messages.
  if (msg.routing_id() > 0 && msg.routing_id() < INTERFACE_ID_COUNT) {
    InterfaceProxy* proxy = id_to_proxy_[msg.routing_id()];
    if (proxy)
      return proxy->OnMessageReceived(msg);

    NOTREACHED();
    // TODO(brettw): kill the plugin if it starts sending invalid messages?
  }

  return false;
}

void Dispatcher::SetSerializationRules(
    VarSerializationRules* var_serialization_rules) {
  serialization_rules_.reset(var_serialization_rules);
}

void Dispatcher::InjectProxy(InterfaceID id,
                             const std::string& name,
                             InterfaceProxy* proxy) {
  proxies_[name] = linked_ptr<InterfaceProxy>(proxy);
  id_to_proxy_[id] = proxy;
}

const void* Dispatcher::GetLocalInterface(const char* interface) {
  return local_get_interface_(interface);
}

const void* Dispatcher::GetProxiedInterface(const std::string& interface) {
  // See if we already know about this interface and have created a host.
  ProxyMap::const_iterator found = proxies_.find(interface);
  if (found != proxies_.end())
    return found->second->GetSourceInterface();

  // When the remote side has sent us a declared list of all interfaces it
  // supports and we don't have it in our list, we know the requested interface
  // doesn't exist and we can return failure.
  if (declared_supported_remote_interfaces_)
    return NULL;

  if (!RemoteSupportsTargetInterface(interface))
    return NULL;

  linked_ptr<InterfaceProxy> proxy(CreateProxyForInterface(interface, NULL));
  if (!proxy.get())
    return NULL;  // Don't know how to proxy this interface.

  // Save our proxy.
  proxies_[interface] = proxy;
  id_to_proxy_[proxy->GetInterfaceId()] = proxy.get();
  return proxy->GetSourceInterface();
}

bool Dispatcher::Send(IPC::Message* msg) {
  if (test_sink_)
    return test_sink_->Send(msg);
  return channel_->Send(msg);
}

bool Dispatcher::RemoteSupportsTargetInterface(const std::string& interface) {
  bool result = false;
  Send(new PpapiMsg_SupportsInterface(interface, &result));
  return result;
}

bool Dispatcher::IsInterfaceTrusted(const std::string& interface) {
  // FIXME(brettw)
  (void)interface;
  return false;
}

bool Dispatcher::SetupProxyForTargetInterface(const std::string& interface) {
  // If we already have a proxy that knows about the locally-implemented
  // interface, we know it's supported and don't need to re-query.
  ProxyMap::const_iterator found = proxies_.find(interface);
  if (found != proxies_.end())
    return true;

  if (disallow_trusted_interfaces_ && IsInterfaceTrusted(interface))
    return false;

  // Create the proxy if it doesn't exist and set the local interface on it.
  // This also handles the case where possibly an interface could be supported
  // by both the local and remote side.
  const void* interface_functions = local_get_interface_(interface.c_str());
  if (!interface_functions)
    return false;
  InterfaceProxy* proxy = CreateProxyForInterface(interface,
                                                  interface_functions);
  if (!proxy)
    return false;

  proxies_[interface] = linked_ptr<InterfaceProxy>(proxy);
  id_to_proxy_[proxy->GetInterfaceId()] = proxy;
  return true;
}

void Dispatcher::OnMsgSupportsInterface(const std::string& interface_name,
                                        bool* result) {
  *result = SetupProxyForTargetInterface(interface_name);
}

void Dispatcher::OnMsgDeclareInterfaces(
    const std::vector<std::string>& interfaces) {
  // Make proxies for all the interfaces it supports that we also support.
  for (size_t i = 0; i < interfaces.size(); i++) {
    // Possibly the plugin could request an interface before the "declare"
    // message is received, so we could already have an entry for this
    // interface. In this case, we can just skip to the next one.
    if (proxies_.find(interfaces[i]) != proxies_.end())
      continue;

    linked_ptr<InterfaceProxy> proxy(CreateProxyForInterface(interfaces[i],
                                                             NULL));
    if (!proxy.get()) {
      // Since only the browser declares supported interfaces, we should never
      // get one we don't support.
      //NOTREACHED() << "Remote side declaring an unsupported proxy.";
      continue;
    }
    proxies_[interfaces[i]] = proxy;
    id_to_proxy_[proxy->GetInterfaceId()] = proxy.get();
  }
}

InterfaceProxy* Dispatcher::CreateProxyForInterface(
    const std::string& interface_name,
    const void* interface_functions) {
  if (interface_name == PPB_AUDIO_CONFIG_INTERFACE)
    return new PPB_AudioConfig_Proxy(this, interface_functions);
  if (interface_name == PPB_AUDIO_INTERFACE)
    return new PPB_Audio_Proxy(this, interface_functions);
  if (interface_name == PPB_BUFFER_DEV_INTERFACE)
    return new PPB_Buffer_Proxy(this, interface_functions);
  if (interface_name == PPB_CHAR_SET_DEV_INTERFACE)
    return new PPB_CharSet_Proxy(this, interface_functions);
  if (interface_name == PPB_CORE_INTERFACE)
    return new PPB_Core_Proxy(this, interface_functions);
  if (interface_name == PPB_CURSOR_CONTROL_DEV_INTERFACE)
    return new PPB_CursorControl_Proxy(this, interface_functions);
  if (interface_name == PPB_FONT_DEV_INTERFACE)
    return new PPB_Font_Proxy(this, interface_functions);
  if (interface_name == PPB_FULLSCREEN_DEV_INTERFACE)
    return new PPB_Fullscreen_Proxy(this, interface_functions);
  if (interface_name == PPB_GRAPHICS_2D_INTERFACE)
    return new PPB_Graphics2D_Proxy(this, interface_functions);
  if (interface_name == PPB_IMAGEDATA_INTERFACE)
    return new PPB_ImageData_Proxy(this, interface_functions);
  if (interface_name == PPB_INSTANCE_INTERFACE)
    return new PPB_Instance_Proxy(this, interface_functions);
  if (interface_name == PPB_TESTING_DEV_INTERFACE)
    return new PPB_Testing_Proxy(this, interface_functions);
  if (interface_name == PPB_URLLOADER_INTERFACE)
    return new PPB_URLLoader_Proxy(this, interface_functions);
  if (interface_name == PPB_URLREQUESTINFO_INTERFACE)
    return new PPB_URLRequestInfo_Proxy(this, interface_functions);
  if (interface_name == PPB_URLRESPONSEINFO_INTERFACE)
    return new PPB_URLResponseInfo_Proxy(this, interface_functions);
  if (interface_name == PPB_VAR_DEPRECATED_INTERFACE)
    return new PPB_Var_Deprecated_Proxy(this, interface_functions);
  if (interface_name == PPP_INSTANCE_INTERFACE)
    return new PPP_Instance_Proxy(this, interface_functions);

  // Trusted interfaces.
  if (!disallow_trusted_interfaces_) {
    if (interface_name == PPB_FLASH_INTERFACE)
      return new PPB_Flash_Proxy(this, interface_functions);
    if (interface_name == PPB_PDF_INTERFACE)
      return new PPB_PDF_Proxy(this, interface_functions);
    if (interface_name == PPB_URLLOADERTRUSTED_INTERFACE)
      return new PPB_URLLoaderTrusted_Proxy(this, interface_functions);
  }

  return NULL;
}

}  // namespace proxy
}  // namespace pp

