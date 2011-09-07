// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/dispatcher.h"

#include <string.h>  // For memset.

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
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
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_audio_config_proxy.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_broker_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_char_set_proxy.h"
#include "ppapi/proxy/ppb_console_proxy.h"
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
#include "ppapi/proxy/ppb_input_event_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_memory_proxy.h"
#include "ppapi/proxy/ppb_opengles2_proxy.h"
#include "ppapi/proxy/ppb_pdf_proxy.h"
#include "ppapi/proxy/ppb_surface_3d_proxy.h"
#include "ppapi/proxy/ppb_testing_proxy.h"
#include "ppapi/proxy/ppb_url_loader_proxy.h"
#include "ppapi/proxy/ppb_url_request_info_proxy.h"
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
#include "ppapi/proxy/var_serialization_rules.h"

namespace ppapi {
namespace proxy {

namespace {

struct InterfaceList {
  InterfaceList();

  static InterfaceList* GetInstance();

  void AddPPP(const InterfaceProxy::Info* info);
  void AddPPB(const InterfaceProxy::Info* info);

  typedef std::map<std::string, const InterfaceProxy::Info*> NameToInfo;
  NameToInfo name_to_plugin_info_;
  NameToInfo name_to_browser_info_;

  // Note that there can be multiple interface names mapping to the same ID.
  // In this case, the ID will map to one of them. This is temporary while
  // we're converting to the thunk system, when that is complete, we need to
  // have a better way of handling multiple interface implemented by one
  // proxy object.
  const InterfaceProxy::Info* id_to_browser_info_[INTERFACE_ID_COUNT];
};

InterfaceList::InterfaceList() {
  memset(id_to_browser_info_, 0, sizeof(id_to_browser_info_));

  // PPB (browser) interfaces.
  AddPPB(PPB_AudioConfig_Proxy::GetInfo());
  AddPPB(PPB_Audio_Proxy::GetInfo());
  AddPPB(PPB_Broker_Proxy::GetInfo());
  AddPPB(PPB_Buffer_Proxy::GetInfo());
  AddPPB(PPB_CharSet_Proxy::GetInfo());
  AddPPB(PPB_Console_Proxy::GetInfo());
  AddPPB(PPB_Context3D_Proxy::GetInfo());
  AddPPB(PPB_Context3D_Proxy::GetTextureMappingInfo());
  AddPPB(PPB_Core_Proxy::GetInfo());
  AddPPB(PPB_Crypto_Proxy::GetInfo());
  AddPPB(PPB_CursorControl_Proxy::GetInfo());
  AddPPB(PPB_FileChooser_Proxy::GetInfo());
  AddPPB(PPB_FileChooser_Proxy::GetInfo0_4());
  AddPPB(PPB_FileRef_Proxy::GetInfo());
  AddPPB(PPB_FileSystem_Proxy::GetInfo());
  AddPPB(PPB_Flash_Clipboard_Proxy::GetInfo());
  AddPPB(PPB_Flash_File_FileRef_Proxy::GetInfo());
  AddPPB(PPB_Flash_File_ModuleLocal_Proxy::GetInfo());
  AddPPB(PPB_Flash_Menu_Proxy::GetInfo());
  AddPPB(PPB_Flash_Proxy::GetInfo());
  AddPPB(PPB_Flash_TCPSocket_Proxy::GetInfo());
  AddPPB(PPB_Font_Proxy::GetInfo());
  AddPPB(PPB_Graphics2D_Proxy::GetInfo());
  AddPPB(PPB_Graphics3D_Proxy::GetInfo());
  AddPPB(PPB_ImageData_Proxy::GetInfo());
  AddPPB(PPB_InputEvent_Proxy::GetInputEventInfo());
  AddPPB(PPB_InputEvent_Proxy::GetKeyboardInputEventInfo());
  AddPPB(PPB_InputEvent_Proxy::GetMouseInputEventInfo1_0());
  AddPPB(PPB_InputEvent_Proxy::GetMouseInputEventInfo1_1());
  AddPPB(PPB_InputEvent_Proxy::GetWheelInputEventInfo());
  AddPPB(PPB_Instance_Proxy::GetInfo0_5());
  AddPPB(PPB_Instance_Proxy::GetInfo1_0());
  AddPPB(PPB_Instance_Proxy::GetInfoFullscreen());
  AddPPB(PPB_Instance_Proxy::GetInfoMessaging());
  AddPPB(PPB_Instance_Proxy::GetInfoPrivate());
  AddPPB(PPB_Memory_Proxy::GetInfo());
  AddPPB(PPB_OpenGLES2_Proxy::GetInfo());
  AddPPB(PPB_PDF_Proxy::GetInfo());
  AddPPB(PPB_Surface3D_Proxy::GetInfo());
  AddPPB(PPB_Testing_Proxy::GetInfo());
  AddPPB(PPB_URLLoader_Proxy::GetInfo());
  AddPPB(PPB_URLLoader_Proxy::GetTrustedInfo());
  AddPPB(PPB_URLRequestInfo_Proxy::GetInfo());
  AddPPB(PPB_URLResponseInfo_Proxy::GetInfo());
  AddPPB(PPB_URLUtil_Proxy::GetInfo());
  AddPPB(PPB_Var_Deprecated_Proxy::GetInfo());
  AddPPB(PPB_Var_Proxy::GetInfo());
  AddPPB(PPB_VideoCapture_Proxy::GetInfo());
  AddPPB(PPB_VideoDecoder_Proxy::GetInfo());

#ifdef ENABLE_FLAPPER_HACKS
  AddPPB(PPB_Flash_NetConnector_Proxy::GetInfo());
#endif

  // PPP (plugin) interfaces.
  AddPPP(PPP_Graphics3D_Proxy::GetInfo());
  AddPPP(PPP_InputEvent_Proxy::GetInfo());
  AddPPP(PPP_Instance_Private_Proxy::GetInfo());
  AddPPP(PPP_Instance_Proxy::GetInfo1_0());
  AddPPP(PPP_Messaging_Proxy::GetInfo());
  AddPPP(PPP_VideoCapture_Proxy::GetInfo());
  AddPPP(PPP_VideoDecoder_Proxy::GetInfo());
}

void InterfaceList::AddPPP(const InterfaceProxy::Info* info) {
  DCHECK(name_to_plugin_info_.find(info->name) ==
         name_to_plugin_info_.end());
  DCHECK(info->id >= INTERFACE_ID_NONE && info->id < INTERFACE_ID_COUNT);

  name_to_plugin_info_[info->name] = info;
}

void InterfaceList::AddPPB(const InterfaceProxy::Info* info) {
  DCHECK(name_to_browser_info_.find(info->name) ==
         name_to_browser_info_.end());
  DCHECK(info->id >= INTERFACE_ID_NONE && info->id < INTERFACE_ID_COUNT);
  DCHECK(info->id == INTERFACE_ID_NONE ||
         id_to_browser_info_[info->id] == NULL);

  name_to_browser_info_[std::string(info->name)] = info;
  if (info->id != INTERFACE_ID_NONE)
    id_to_browser_info_[info->id] = info;
}

// static
InterfaceList* InterfaceList::GetInstance() {
  return Singleton<InterfaceList>::get();
}

}  // namespace

Dispatcher::Dispatcher(base::ProcessHandle remote_process_handle,
                       GetInterfaceFunc local_get_interface)
    : ProxyChannel(remote_process_handle),
      disallow_trusted_interfaces_(false),  // TODO(brettw) make this settable.
      local_get_interface_(local_get_interface),
      callback_tracker_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

Dispatcher::~Dispatcher() {
}

bool Dispatcher::OnMessageReceived(const IPC::Message& msg) {
  // Control messages.
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(Dispatcher, msg)
      IPC_MESSAGE_FORWARD(PpapiMsg_ExecuteCallback, &callback_tracker_,
                          CallbackTracker::ReceiveExecuteSerializedCallback)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }
  return false;
}

// static
const InterfaceProxy::Info* Dispatcher::GetPPBInterfaceInfo(
    const std::string& name) {
  const InterfaceList* list = InterfaceList::GetInstance();
  InterfaceList::NameToInfo::const_iterator found =
      list->name_to_browser_info_.find(name);
  if (found == list->name_to_browser_info_.end())
    return NULL;
  return found->second;
}

// static
const InterfaceProxy::Info* Dispatcher::GetPPBInterfaceInfo(InterfaceID id) {
  if (id <= 0 || id >= INTERFACE_ID_COUNT)
    return NULL;
  const InterfaceList* list = InterfaceList::GetInstance();
  return list->id_to_browser_info_[id];
}

// static
const InterfaceProxy::Info* Dispatcher::GetPPPInterfaceInfo(
    const std::string& name) {
  const InterfaceList* list = InterfaceList::GetInstance();
  InterfaceList::NameToInfo::const_iterator found =
      list->name_to_plugin_info_.find(name);
  if (found == list->name_to_plugin_info_.end())
    return NULL;
  return found->second;
}

void Dispatcher::SetSerializationRules(
    VarSerializationRules* var_serialization_rules) {
  serialization_rules_.reset(var_serialization_rules);
}

const void* Dispatcher::GetLocalInterface(const char* interface_name) {
  return local_get_interface_(interface_name);
}

base::MessageLoopProxy* Dispatcher::GetIPCMessageLoop() {
  return delegate()->GetIPCMessageLoop();
}

void Dispatcher::AddIOThreadMessageFilter(
    IPC::ChannelProxy::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

}  // namespace proxy
}  // namespace ppapi
