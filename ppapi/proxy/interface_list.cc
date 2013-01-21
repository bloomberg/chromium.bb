// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/interface_list.h"

#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_graphics_2d_dev.h"
#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/dev/ppb_keyboard_input_event_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_message_loop.h"
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
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_device_id.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_flash_message_loop.h"
#include "ppapi/c/private/ppb_flash_print.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_network_list_private.h"
#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/private/ppp_content_decryptor_private.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_broker_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_core_proxy.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/ppb_file_system_proxy.h"
#include "ppapi/proxy/ppb_flash_message_loop_proxy.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/ppb_host_resolver_private_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/proxy/ppb_network_monitor_private_proxy.h"
#include "ppapi/proxy/ppb_pdf_proxy.h"
#include "ppapi/proxy/ppb_tcp_server_socket_private_proxy.h"
#include "ppapi/proxy/ppb_tcp_socket_private_proxy.h"
#include "ppapi/proxy/ppb_testing_proxy.h"
#include "ppapi/proxy/ppb_url_loader_proxy.h"
#include "ppapi/proxy/ppb_var_deprecated_proxy.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/proxy/ppb_x509_certificate_private_proxy.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/ppp_content_decryptor_private_proxy.h"
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

base::LazyInstance<PpapiPermissions> g_process_global_permissions;

}  // namespace

InterfaceList::InterfaceList() {
  memset(id_to_factory_, 0, sizeof(id_to_factory_));

  // Register the API factories for each of the API types. This calls AddProxy
  // for each InterfaceProxy type we support.
  #define PROXIED_API(api_name) \
      AddProxy(PROXY_API_ID(api_name), &PROXY_FACTORY_NAME(api_name));

  // Register each proxied interface by calling AddPPB for each supported
  // interface. Set current_required_permission to the appropriate value for
  // the value you want expanded by this macro.
  #define PROXIED_IFACE(api_name, iface_str, iface_struct) \
      AddPPB(iface_str, PROXY_API_ID(api_name), \
             INTERFACE_THUNK_NAME(iface_struct)(), \
             current_required_permission);

  {
    Permission current_required_permission = PERMISSION_NONE;
    #include "ppapi/thunk/interfaces_ppb_private_no_permissions.h"
    #include "ppapi/thunk/interfaces_ppb_public_stable.h"
  }

  {
    Permission current_required_permission = PERMISSION_DEV;
    #include "ppapi/thunk/interfaces_ppb_public_dev.h"
  }
  {
    Permission current_required_permission = PERMISSION_PRIVATE;
    #include "ppapi/thunk/interfaces_ppb_private.h"
  }
  {
#if !defined(OS_NACL)
    Permission current_required_permission = PERMISSION_FLASH;
    #include "ppapi/thunk/interfaces_ppb_private_flash.h"
#endif  // !defined(OS_NACL)
  }

  #undef PROXIED_API
  #undef PROXIED_IFACE

  // Manually add some special proxies. Some of these don't have interfaces
  // that they support, so aren't covered by the macros above, but have proxies
  // for message routing. Others have different implementations between the
  // proxy and the impl and there's no obvious message routing.
  AddProxy(API_ID_RESOURCE_CREATION, &ResourceCreationProxy::Create);
  AddProxy(API_ID_PPP_CLASS, &PPP_Class_Proxy::Create);
  AddPPB(PPB_CORE_INTERFACE_1_0, API_ID_PPB_CORE,
         PPB_Core_Proxy::GetPPB_Core_Interface(), PERMISSION_NONE);
  AddPPB(PPB_MESSAGELOOP_INTERFACE_1_0, API_ID_NONE,
         PPB_MessageLoop_Proxy::GetInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_INSTANCEDARRAYS_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetInstancedArraysInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_FRAMEBUFFERBLIT_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetFramebufferBlitInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetFramebufferMultisampleInterface(),
         PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_CHROMIUMENABLEFEATURE_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetChromiumEnableFeatureInterface(),
         PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetChromiumMapSubInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetChromiumMapSubInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_QUERY_INTERFACE_1_0, API_ID_NONE,
         PPB_OpenGLES2_Shared::GetQueryInterface(), PERMISSION_NONE);
  AddPPB(PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0, API_ID_NONE,
         PPB_Var_Shared::GetVarArrayBufferInterface1_0(),
         PERMISSION_NONE);
  AddPPB(PPB_VAR_INTERFACE_1_1, API_ID_NONE,
         PPB_Var_Shared::GetVarInterface1_1(), PERMISSION_NONE);
  AddPPB(PPB_VAR_INTERFACE_1_0, API_ID_NONE,
         PPB_Var_Shared::GetVarInterface1_0(), PERMISSION_NONE);

#if !defined(OS_NACL)
  // PPB (browser) interfaces.
  // Do not add more stuff here, they should be added to interface_list*.h
  // TODO(brettw) remove these.
  AddPPB(PPB_Instance_Proxy::GetInfoPrivate(), PERMISSION_PRIVATE);
  AddPPB(PPB_PDF_Proxy::GetInfo(), PERMISSION_PRIVATE);
  AddPPB(PPB_URLLoader_Proxy::GetTrustedInfo(), PERMISSION_PRIVATE);
  AddPPB(PPB_Var_Deprecated_Proxy::GetInfo(), PERMISSION_DEV);

  // TODO(tomfinegan): Figure out where to put these once we refactor things
  // to load the PPP interface struct from the PPB interface.
  AddProxy(API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
           &ProxyFactory<PPP_ContentDecryptor_Private_Proxy>);
  AddPPP(PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE,
         API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
         PPP_ContentDecryptor_Private_Proxy::GetProxyInterface());
#endif
  AddPPB(PPB_Testing_Proxy::GetInfo(), PERMISSION_TESTING);

  // PPP (plugin) interfaces.
  // TODO(brettw) move these to interface_list*.h
  AddProxy(API_ID_PPP_INSTANCE, &ProxyFactory<PPP_Instance_Proxy>);
  #if !defined(OS_NACL)
  AddPPP(PPP_INSTANCE_INTERFACE_1_1, API_ID_PPP_INSTANCE,
         PPP_Instance_Proxy::GetInstanceInterface());
  #endif
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
  AddPPP(PPP_Graphics3D_Proxy::GetInfo());
#if !defined(OS_NACL)
  AddPPP(PPP_Instance_Private_Proxy::GetInfo());
  AddPPP(PPP_VideoDecoder_Proxy::GetInfo());
#endif
}

InterfaceList::~InterfaceList() {
}

// static
InterfaceList* InterfaceList::GetInstance() {
  return Singleton<InterfaceList>::get();
}

// static
void InterfaceList::SetProcessGlobalPermissions(
    const PpapiPermissions& permissions) {
  g_process_global_permissions.Get() = permissions;
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

  if (g_process_global_permissions.Get().HasPermission(
          found->second.required_permission))
    return found->second.iface;
  return NULL;
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
                           const void* iface,
                           Permission perm) {
  DCHECK(name_to_browser_info_.find(name) == name_to_browser_info_.end());
  name_to_browser_info_[name] = InterfaceInfo(id, iface, perm);
}

void InterfaceList::AddPPP(const char* name,
                           ApiID id,
                           const void* iface) {
  DCHECK(name_to_plugin_info_.find(name) == name_to_plugin_info_.end());
  name_to_plugin_info_[name] = InterfaceInfo(id, iface, PERMISSION_NONE);
}

void InterfaceList::AddPPB(const InterfaceProxy::Info* info, Permission perm) {
  AddProxy(info->id, info->create_proxy);
  AddPPB(info->name, info->id, info->interface_ptr, perm);
}

void InterfaceList::AddPPP(const InterfaceProxy::Info* info) {
  AddProxy(info->id, info->create_proxy);
  AddPPP(info->name, info->id, info->interface_ptr);
}

}  // namespace proxy
}  // namespace ppapi
