// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/interface_list.h"

#include "base/memory/singleton.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_opengles.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_broker_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_char_set_proxy.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"
#include "ppapi/proxy/ppb_core_proxy.h"
#include "ppapi/proxy/ppb_crypto_proxy.h"
#include "ppapi/proxy/ppb_cursor_control_proxy.h"
#include "ppapi/proxy/ppb_file_chooser_proxy.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/ppb_file_system_proxy.h"
#include "ppapi/proxy/ppb_flash_clipboard_proxy.h"
#include "ppapi/proxy/ppb_flash_file_proxy.h"
#include "ppapi/proxy/ppb_flash_proxy.h"
#include "ppapi/proxy/ppb_flash_menu_proxy.h"
#include "ppapi/proxy/ppb_flash_net_connector_proxy.h"
#include "ppapi/proxy/ppb_flash_tcp_socket_proxy.h"
#include "ppapi/proxy/ppb_font_proxy.h"
#include "ppapi/proxy/ppb_graphics_2d_proxy.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_memory_proxy.h"
#include "ppapi/proxy/ppp_mouse_lock_proxy.h"
#include "ppapi/proxy/ppb_pdf_proxy.h"
#include "ppapi/proxy/ppb_surface_3d_proxy.h"
#include "ppapi/proxy/ppb_testing_proxy.h"
#include "ppapi/proxy/ppb_url_loader_proxy.h"
#include "ppapi/proxy/ppb_url_response_info_proxy.h"
#include "ppapi/proxy/ppb_url_util_proxy.h"
#include "ppapi/proxy/ppb_var_deprecated_proxy.h"
#include "ppapi/proxy/ppb_var_proxy.h"
#include "ppapi/proxy/ppb_video_capture_proxy.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/ppp_graphics_3d_proxy.h"
#include "ppapi/proxy/ppp_input_event_proxy.h"
#include "ppapi/proxy/ppp_instance_private_proxy.h"
#include "ppapi/proxy/ppp_instance_proxy.h"
#include "ppapi/proxy/ppp_messaging_proxy.h"
#include "ppapi/proxy/ppp_video_decoder_proxy.h"
#include "ppapi/proxy/resource_creation_proxy.h"
#include "ppapi/shared_impl/opengles2_impl.h"
#include "ppapi/thunk/thunk.h"

// Helper to get the proxy name PPB_Foo_Proxy given the API name PPB_Foo.
#define PROXY_CLASS_NAME(api_name) api_name##_Proxy

// Helper to get the interface ID PPB_Foo_Proxy::kInterfaceID given the API
// name PPB_Foo.
#define PROXY_INTERFACE_ID(api_name) PROXY_CLASS_NAME(api_name)::kInterfaceID

// Helper to get the name of the factory function CreatePPB_Foo_Proxy given
// the API name PPB_Foo.
#define PROXY_FACTORY_NAME(api_name) Create##api_name##_Proxy

// Helper to get the name of the thunk GetPPB_Foo_1_0_Thunk given the interface
// struct name PPB_Foo_1_0.
#define INTERFACE_THUNK_NAME(iface_struct) thunk::Get##iface_struct##_Thunk

namespace ppapi {
namespace proxy {

namespace {

// The interface list has interfaces with no ID listed as "NoAPIName" which
// means there's no corresponding _Proxy object. Our macros expand this to
// NoAPIName_Proxy, and then they look for kInterfaceID inside it.
//
// This dummy class provides the correct definition for that interface ID,
// which is "NONE".
class NoAPIName_Proxy {
 public:
  static const InterfaceID kInterfaceID = INTERFACE_ID_NONE;
};

// Define factory functions for each interface type. These are of the form:
//   InterfaceProxy* CreatePPB_URLLoader_Proxy(...
#define PROXIED_API(api_name) \
    InterfaceProxy* PROXY_FACTORY_NAME(api_name)(Dispatcher* dispatcher) { \
      return new PROXY_CLASS_NAME(api_name)(dispatcher); \
    }
#include "ppapi/thunk/interfaces_ppb_public_stable.h"
#include "ppapi/thunk/interfaces_ppb_public_dev.h"
#include "ppapi/thunk/interfaces_ppb_private.h"
#undef PROXIED_API

}  // namespace

InterfaceList::InterfaceList() {
  memset(id_to_factory_, 0, sizeof(id_to_factory_));

  // Register the API factories for each of the API types. This calls AddProxy
  // for each InterfaceProxy type we support.
  #define PROXIED_API(api_name) \
      AddProxy(PROXY_INTERFACE_ID(api_name), &PROXY_FACTORY_NAME(api_name));

  // Register each proxied interface by calling AddPPB for each supported
  // interface.
  #define PROXIED_IFACE(api_name, iface_str, iface_struct) \
      AddPPB(iface_str, PROXY_INTERFACE_ID(api_name), \
             INTERFACE_THUNK_NAME(iface_struct)());

  #include "ppapi/thunk/interfaces_ppb_public_stable.h"
  #include "ppapi/thunk/interfaces_ppb_public_dev.h"
  #include "ppapi/thunk/interfaces_ppb_private.h"

  #undef PROXIED_API
  #undef PROXIED_IFACE

  // New-style AddPPB not converted to the macros above.
  AddPPB(PPB_CORE_INTERFACE, INTERFACE_ID_PPB_CORE,
         PPB_Core_Proxy::GetPPB_Core_Interface());
  AddPPB(PPB_MEMORY_DEV_INTERFACE, INTERFACE_ID_NONE,
         GetPPB_Memory_Interface());
  AddPPB(PPB_OPENGLES2_INTERFACE, INTERFACE_ID_NONE,
         OpenGLES2Impl::GetInterface());
  AddPPB(PPB_VAR_INTERFACE, INTERFACE_ID_NONE,
         GetPPB_Var_Interface());

  // Manually add some special proxies. These don't have interfaces that they
  // support, so aren't covered by the macros above, but have proxies for
  // message routing.
  AddProxy(INTERFACE_ID_RESOURCE_CREATION, &ResourceCreationProxy::Create);
  AddProxy(INTERFACE_ID_PPP_CLASS, &PPP_Class_Proxy::Create);

  // PPB (browser) interfaces.
  AddPPB(PPB_Crypto_Proxy::GetInfo());
  AddPPB(PPB_Flash_Clipboard_Proxy::GetInfo());
  AddPPB(PPB_Flash_File_FileRef_Proxy::GetInfo());
  AddPPB(PPB_Flash_File_ModuleLocal_Proxy::GetInfo());
  AddPPB(PPB_Flash_Menu_Proxy::GetInfo());
  AddPPB(PPB_Flash_Proxy::GetInfo());
  AddPPB(PPB_Flash_TCPSocket_Proxy::GetInfo());
  AddPPB(PPB_Instance_Proxy::GetInfoFullscreen());
  AddPPB(PPB_Instance_Proxy::GetInfoPrivate());
  AddPPB(PPB_PDF_Proxy::GetInfo());
  AddPPB(PPB_Testing_Proxy::GetInfo());
  AddPPB(PPB_URLLoader_Proxy::GetTrustedInfo());
  AddPPB(PPB_URLUtil_Proxy::GetInfo());
  AddPPB(PPB_Var_Deprecated_Proxy::GetInfo());

#ifdef ENABLE_FLAPPER_HACKS
  AddPPB(PPB_Flash_NetConnector_Proxy::GetInfo());
#endif

  // PPP (plugin) interfaces.
  AddPPP(PPP_Graphics3D_Proxy::GetInfo());
  AddPPP(PPP_InputEvent_Proxy::GetInfo());
  AddPPP(PPP_Instance_Private_Proxy::GetInfo());
  AddPPP(PPP_Instance_Proxy::GetInfo1_0());
  AddPPP(PPP_Messaging_Proxy::GetInfo());
  AddPPP(PPP_MouseLock_Proxy::GetInfo());
  AddPPP(PPP_VideoCapture_Proxy::GetInfo());
  AddPPP(PPP_VideoDecoder_Proxy::GetInfo());
}

InterfaceList::~InterfaceList() {
}

// static
InterfaceList* InterfaceList::GetInstance() {
  return Singleton<InterfaceList>::get();
}

InterfaceID InterfaceList::GetIDForPPBInterface(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_browser_info_.find(name);
  if (found == name_to_browser_info_.end())
    return INTERFACE_ID_NONE;
  return found->second.id;
}

InterfaceID InterfaceList::GetIDForPPPInterface(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_plugin_info_.find(name);
  if (found == name_to_plugin_info_.end())
    return INTERFACE_ID_NONE;
  return found->second.id;
}

InterfaceProxy::Factory InterfaceList::GetFactoryForID(InterfaceID id) const {
  int index = static_cast<int>(id);
  COMPILE_ASSERT(INTERFACE_ID_NONE == 0, none_must_be_zero);
  if (id <= 0 || id >= INTERFACE_ID_COUNT)
    return NULL;
  return id_to_factory_[index];
}

const void* InterfaceList::GetInterfaceForPPB(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_browser_info_.find(name);
  if (found == name_to_browser_info_.end())
    return NULL;
  return found->second.interface;
}

const void* InterfaceList::GetInterfaceForPPP(const std::string& name) const {
  NameToInterfaceInfoMap::const_iterator found =
      name_to_plugin_info_.find(name);
  if (found == name_to_plugin_info_.end())
    return NULL;
  return found->second.interface;
}

void InterfaceList::AddProxy(InterfaceID id,
                             InterfaceProxy::Factory factory) {
  // For interfaces with no corresponding _Proxy objects, the macros will
  // generate calls to this function with INTERFACE_ID_NONE. This means we
  // should just skip adding a factory for these functions.
  if (id == INTERFACE_ID_NONE)
    return;

  // The factory should be an exact dupe of the one we already have if it
  // has already been registered before.
  int index = static_cast<int>(id);
  DCHECK(!id_to_factory_[index] || id_to_factory_[index] == factory);

  id_to_factory_[index] = factory;
}

void InterfaceList::AddPPB(const char* name,
                           InterfaceID id,
                           const void* interface) {
  DCHECK(name_to_browser_info_.find(name) == name_to_browser_info_.end());
  name_to_browser_info_[name] = InterfaceInfo(id, interface);
}

void InterfaceList::AddPPP(const char* name,
                           InterfaceID id,
                           const void* interface) {
  DCHECK(name_to_plugin_info_.find(name) == name_to_plugin_info_.end());
  name_to_plugin_info_[name] = InterfaceInfo(id, interface);
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
