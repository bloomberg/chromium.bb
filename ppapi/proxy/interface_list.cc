// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/interface_list.h"

#include "base/memory/singleton.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/dev/ppb_keyboard_input_event_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_message_loop_dev.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_flash_message_loop.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_network_list_private.h"
#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppb_audio_input_proxy.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_broker_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_core_proxy.h"
#include "ppapi/proxy/ppb_file_chooser_proxy.h"
#include "ppapi/proxy/ppb_file_io_proxy.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/ppb_file_system_proxy.h"
#include "ppapi/proxy/ppb_flash_menu_proxy.h"
#include "ppapi/proxy/ppb_flash_message_loop_proxy.h"
#include "ppapi/proxy/ppb_flash_proxy.h"
#include "ppapi/proxy/ppb_graphics_2d_proxy.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/ppb_host_resolver_private_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/proxy/ppb_network_monitor_private_proxy.h"
#include "ppapi/proxy/ppb_pdf_proxy.h"
#include "ppapi/proxy/ppb_talk_private_proxy.h"
#include "ppapi/proxy/ppb_tcp_server_socket_private_proxy.h"
#include "ppapi/proxy/ppb_tcp_socket_private_proxy.h"
#include "ppapi/proxy/ppb_testing_proxy.h"
#include "ppapi/proxy/ppb_udp_socket_private_proxy.h"
#include "ppapi/proxy/ppb_url_loader_proxy.h"
#include "ppapi/proxy/ppb_url_response_info_proxy.h"
#include "ppapi/proxy/ppb_var_deprecated_proxy.h"
#include "ppapi/proxy/ppb_video_capture_proxy.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/proxy/ppb_x509_certificate_private_proxy.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/ppp_graphics_3d_proxy.h"
#include "ppapi/proxy/ppp_input_event_proxy.h"
#include "ppapi/proxy/ppp_instance_private_proxy.h"
#include "ppapi/proxy/ppp_instance_proxy.h"
#include "ppapi/proxy/ppp_messaging_proxy.h"
#include "ppapi/proxy/ppp_mouse_lock_proxy.h"
#include "ppapi/proxy/ppp_printing_proxy.h"
#include "ppapi/proxy/ppp_text_input_proxy.h"
#include "ppapi/proxy/ppp_video_decoder_proxy.h"
#include "ppapi/proxy/resource_creation_proxy.h"
#include "ppapi/shared_impl/ppb_opengles2_shared.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "ppapi/thunk/thunk.h"

// Helper to get the proxy name PPB_Foo_Proxy given the API name PPB_Foo.
#define PROXY_CLASS_NAME(api_name) api_name##_Proxy

// Helper to get the interface ID PPB_Foo_Proxy::kApiID given the API
// name PPB_Foo.
#define PROXY_API_ID(api_name) PROXY_CLASS_NAME(api_name)::kApiID

// Helper to get the name of the templatized factory function.
#define PROXY_FACTORY_NAME(api_name) ProxyFactory<PROXY_CLASS_NAME(api_name)>

// Helper to get the name of the thunk GetPPB_Foo_1_0_Thunk given the interface
// struct name PPB_Foo_1_0.
#define INTERFACE_THUNK_NAME(iface_struct) thunk::Get##iface_struct##_Thunk

namespace ppapi {
namespace proxy {

namespace {

// The interface list has interfaces with no ID listed as "NoAPIName" which
// means there's no corresponding _Proxy object. Our macros expand this to
// NoAPIName_Proxy, and then they look for kApiID inside it.
//
// This dummy class provides the correct definition for that interface ID,
// which is "NONE".
class NoAPIName_Proxy {
 public:
  static const ApiID kApiID = API_ID_NONE;
};

template<typename ProxyClass>
InterfaceProxy* ProxyFactory(Dispatcher* dispatcher) {
  return new ProxyClass(dispatcher);
}

}  // namespace

InterfaceList::InterfaceList() {
  memset(id_to_factory_, 0, sizeof(id_to_factory_));

  // Register the API factories for each of the API types. This calls AddProxy
  // for each InterfaceProxy type we support.
  #define PROXIED_API(api_name) \
      AddProxy(PROXY_API_ID(api_name), &PROXY_FACTORY_NAME(api_name));

  // Register each proxied interface by calling AddPPB for each supported
  // interface.
  #define PROXIED_IFACE(api_name, iface_str, iface_struct) \
      AddPPB(iface_str, PROXY_API_ID(api_name), \
             INTERFACE_THUNK_NAME(iface_struct)());

  #include "ppapi/thunk/interfaces_ppb_public_stable.h"
#if !defined(OS_NACL)
  #include "ppapi/thunk/interfaces_ppb_public_dev.h"
  #include "ppapi/thunk/interfaces_ppb_private.h"
#endif

  #if !defined(OS_NACL)
  #include "ppapi/thunk/interfaces_ppb_private_flash.h"
  #endif

  #undef PROXIED_API
  #undef PROXIED_IFACE

  // Manually add some special proxies. Some of these don't have interfaces
  // that they support, so aren't covered by the macros above, but have proxies
  // for message routing. Others have different implementations between the
  // proxy and the impl and there's no obvious message routing.
  AddProxy(API_ID_RESOURCE_CREATION, &ResourceCreationProxy::Create);
  AddProxy(API_ID_PPP_CLASS, &PPP_Class_Proxy::Create);
  AddPPB(PPB_CORE_INTERFACE_1_0, API_ID_PPB_CORE,
         PPB_Core_Proxy::GetPPB_Core_Interface());
  AddPPB(PPB_MESSAGELOOP_DEV_INTERFACE_0_1, API_ID_NONE,
         PPB_MessageLoop_Proxy::GetInterface());
#if !defined(OS_NACL)
  AddPPB(PPB_OPENGLES2_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetInterface());
  AddPPB(PPB_OPENGLES2_INSTANCEDARRAYS_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetInstancedArraysInterface());
  AddPPB(PPB_OPENGLES2_FRAMEBUFFERBLIT_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetFramebufferBlitInterface());
  AddPPB(PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetFramebufferMultisampleInterface());
  AddPPB(PPB_OPENGLES2_CHROMIUMENABLEFEATURE_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetChromiumEnableFeatureInterface());
  AddPPB(PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetChromiumMapSubInterface());
  AddPPB(PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetChromiumMapSubInterface());
  AddPPB(PPB_OPENGLES2_QUERY_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetQueryInterface());
#endif
  AddPPB(PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0, API_ID_NONE,
         PPB_Var_Shared::GetVarArrayBufferInterface1_0());
  AddPPB(PPB_VAR_INTERFACE_1_1, API_ID_NONE,
         PPB_Var_Shared::GetVarInterface1_1());
  AddPPB(PPB_VAR_INTERFACE_1_0, API_ID_NONE,
         PPB_Var_Shared::GetVarInterface1_0());

#if !defined(OS_NACL)
  // PPB (browser) interfaces.
  // Do not add more stuff here, they should be added to interface_list*.h
  // TODO(brettw) remove these.
  AddPPB(PPB_Instance_Proxy::GetInfoPrivate());
  AddPPB(PPB_PDF_Proxy::GetInfo());
  AddPPB(PPB_Testing_Proxy::GetInfo());
  AddPPB(PPB_URLLoader_Proxy::GetTrustedInfo());
  AddPPB(PPB_Var_Deprecated_Proxy::GetInfo());
#endif

  // PPP (plugin) interfaces.
  // TODO(brettw) move these to interface_list*.h
  AddProxy(API_ID_PPP_INSTANCE, &ProxyFactory<PPP_Instance_Proxy>);
  AddPPP(PPP_INSTANCE_INTERFACE_1_1, API_ID_PPP_INSTANCE,
         PPP_Instance_Proxy::GetInstanceInterface());
  AddProxy(API_ID_PPP_PRINTING, &ProxyFactory<PPP_Printing_Proxy>);
  AddPPP(PPP_PRINTING_DEV_INTERFACE, API_ID_PPP_PRINTING,
         PPP_Printing_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_TEXT_INPUT, &ProxyFactory<PPP_TextInput_Proxy>);
  AddPPP(PPP_TEXTINPUT_DEV_INTERFACE, API_ID_PPP_TEXT_INPUT,
         PPP_TextInput_Proxy::GetProxyInterface());

  // Old-style GetInfo PPP interfaces.
  // Do not add more stuff here, they should be added to interface_list*.h
  // TODO(brettw) remove these.
  AddPPP(PPP_InputEvent_Proxy::GetInfo());
  AddPPP(PPP_Messaging_Proxy::GetInfo());
  AddPPP(PPP_MouseLock_Proxy::GetInfo());
#if !defined(OS_NACL)
  AddPPP(PPP_Graphics3D_Proxy::GetInfo());
  AddPPP(PPP_Instance_Private_Proxy::GetInfo());
  AddPPP(PPP_VideoCapture_Proxy::GetInfo());
  AddPPP(PPP_VideoDecoder_Proxy::GetInfo());
#endif
}

InterfaceList::~InterfaceList() {
}

// static
InterfaceList* InterfaceList::GetInstance() {
  return Singleton<InterfaceList>::get();
}

ApiID InterfaceList::GetIDForPPBInterface(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_browser_info_.find(name);
  if (found == name_to_browser_info_.end())
    return API_ID_NONE;
  return found->second.id;
}

ApiID InterfaceList::GetIDForPPPInterface(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_plugin_info_.find(name);
  if (found == name_to_plugin_info_.end())
    return API_ID_NONE;
  return found->second.id;
}

InterfaceProxy::Factory InterfaceList::GetFactoryForID(ApiID id) const {
  int index = static_cast<int>(id);
  COMPILE_ASSERT(API_ID_NONE == 0, none_must_be_zero);
  if (id <= 0 || id >= API_ID_COUNT)
    return NULL;
  return id_to_factory_[index];
}

const void* InterfaceList::GetInterfaceForPPB(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_browser_info_.find(name);
  if (found == name_to_browser_info_.end())
    return NULL;
  return found->second.iface;
}

const void* InterfaceList::GetInterfaceForPPP(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_plugin_info_.find(name);
  if (found == name_to_plugin_info_.end())
    return NULL;
  return found->second.iface;
}

void InterfaceList::AddProxy(ApiID id,
                             InterfaceProxy::Factory factory) {
  // For interfaces with no corresponding _Proxy objects, the macros will
  // generate calls to this function with API_ID_NONE. This means we
  // should just skip adding a factory for these functions.
  if (id == API_ID_NONE)
    return;

  // The factory should be an exact dupe of the one we already have if it
  // has already been registered before.
  int index = static_cast<int>(id);
  DCHECK(!id_to_factory_[index] || id_to_factory_[index] == factory);

  id_to_factory_[index] = factory;
}

void InterfaceList::AddPPB(const char* name,
                           ApiID id,
                           const void* iface) {
  DCHECK(name_to_browser_info_.find(name) == name_to_browser_info_.end());
  name_to_browser_info_[name] = InterfaceInfo(id, iface);
}

void InterfaceList::AddPPP(const char* name,
                           ApiID id,
                           const void* iface) {
  DCHECK(name_to_plugin_info_.find(name) == name_to_plugin_info_.end());
  name_to_plugin_info_[name] = InterfaceInfo(id, iface);
}

void InterfaceList::AddPPB(const InterfaceProxy::Info* info) {
  AddProxy(info->id, info->create_proxy);
  AddPPB(info->name, info->id, info->interface_ptr);
}

void InterfaceList::AddPPP(const InterfaceProxy::Info* info) {
  AddProxy(info->id, info->create_proxy);
  AddPPP(info->name, info->id, info->interface_ptr);
}

}  // namespace proxy
}  // namespace ppapi
