// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/dispatcher.h"

#include <string.h>  // For memset.

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_test_sink.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
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
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_audio_config_proxy.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_char_set_proxy.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"
#include "ppapi/proxy/ppb_core_proxy.h"
#include "ppapi/proxy/ppb_cursor_control_proxy.h"
#include "ppapi/proxy/ppb_file_chooser_proxy.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/ppb_flash_proxy.h"
#include "ppapi/proxy/ppb_flash_menu_proxy.h"
#include "ppapi/proxy/ppb_font_proxy.h"
#include "ppapi/proxy/ppb_fullscreen_proxy.h"
#include "ppapi/proxy/ppb_gles_chromium_texture_mapping_proxy.h"
#include "ppapi/proxy/ppb_graphics_2d_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_opengles2_proxy.h"
#include "ppapi/proxy/ppb_pdf_proxy.h"
#include "ppapi/proxy/ppb_surface_3d_proxy.h"
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

namespace {

struct InterfaceList {
  InterfaceList();

  static InterfaceList* GetInstance();

  void AddPPP(const InterfaceProxy::Info* info);
  void AddPPB(const InterfaceProxy::Info* info);

  typedef std::map<std::string, const InterfaceProxy::Info*> NameToInfo;
  NameToInfo name_to_plugin_info_;
  NameToInfo name_to_browser_info_;

  const InterfaceProxy::Info* id_to_plugin_info_[INTERFACE_ID_COUNT];
  const InterfaceProxy::Info* id_to_browser_info_[INTERFACE_ID_COUNT];
};

InterfaceList::InterfaceList() {
  memset(id_to_plugin_info_, 0,
         static_cast<int>(INTERFACE_ID_COUNT) * sizeof(InterfaceID));
  memset(id_to_browser_info_, 0,
         static_cast<int>(INTERFACE_ID_COUNT) * sizeof(InterfaceID));

  // PPB (browser) interfaces.
  AddPPB(PPB_AudioConfig_Proxy::GetInfo());
  AddPPB(PPB_Audio_Proxy::GetInfo());
  AddPPB(PPB_Buffer_Proxy::GetInfo());
  AddPPB(PPB_CharSet_Proxy::GetInfo());
  AddPPB(PPB_Context3D_Proxy::GetInfo());
  AddPPB(PPB_Core_Proxy::GetInfo());
  AddPPB(PPB_CursorControl_Proxy::GetInfo());
  AddPPB(PPB_FileChooser_Proxy::GetInfo());
  AddPPB(PPB_FileRef_Proxy::GetInfo());
  AddPPB(PPB_Flash_Proxy::GetInfo());
  AddPPB(PPB_Flash_Menu_Proxy::GetInfo());
  AddPPB(PPB_Font_Proxy::GetInfo());
  AddPPB(PPB_Fullscreen_Proxy::GetInfo());
  AddPPB(PPB_GLESChromiumTextureMapping_Proxy::GetInfo());
  AddPPB(PPB_Graphics2D_Proxy::GetInfo());
  AddPPB(PPB_ImageData_Proxy::GetInfo());
  AddPPB(PPB_Instance_Proxy::GetInfo());
  AddPPB(PPB_OpenGLES2_Proxy::GetInfo());
  AddPPB(PPB_PDF_Proxy::GetInfo());
  AddPPB(PPB_Surface3D_Proxy::GetInfo());
  AddPPB(PPB_Testing_Proxy::GetInfo());
  AddPPB(PPB_URLLoader_Proxy::GetInfo());
  AddPPB(PPB_URLLoaderTrusted_Proxy::GetInfo());
  AddPPB(PPB_URLRequestInfo_Proxy::GetInfo());
  AddPPB(PPB_URLResponseInfo_Proxy::GetInfo());
  AddPPB(PPB_Var_Deprecated_Proxy::GetInfo());

  // PPP (plugin) interfaces.
  AddPPP(PPP_Instance_Proxy::GetInfo());
}

void InterfaceList::AddPPP(const InterfaceProxy::Info* info) {
  DCHECK(name_to_plugin_info_.find(info->name) ==
         name_to_plugin_info_.end());
  DCHECK(info->id > 0 && info->id < INTERFACE_ID_COUNT);
  DCHECK(id_to_plugin_info_[info->id] == NULL);

  name_to_plugin_info_[info->name] = info;
  id_to_plugin_info_[info->id] = info;
}

void InterfaceList::AddPPB(const InterfaceProxy::Info* info) {
  DCHECK(name_to_browser_info_.find(info->name) ==
         name_to_browser_info_.end());
  DCHECK(info->id > 0 && info->id < INTERFACE_ID_COUNT);
  DCHECK(id_to_browser_info_[info->id] == NULL);

  name_to_browser_info_[std::string(info->name)] = info;
  id_to_browser_info_[info->id] = info;
}

// static
InterfaceList* InterfaceList::GetInstance() {
  return Singleton<InterfaceList>::get();
}

}  // namespace

Dispatcher::Dispatcher(base::ProcessHandle remote_process_handle,
                       GetInterfaceFunc local_get_interface)
    : remote_process_handle_(remote_process_handle),
      test_sink_(NULL),
      disallow_trusted_interfaces_(false),  // TODO(brettw) make this settable.
      local_get_interface_(local_get_interface),
      callback_tracker_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
      IPC_MESSAGE_FORWARD(PpapiMsg_ExecuteCallback, &callback_tracker_,
                          CallbackTracker::ReceiveExecuteSerializedCallback)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }
  return false;
}

void Dispatcher::OnChannelError() {
  channel_.reset();
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

// static
const InterfaceProxy::Info* Dispatcher::GetPPPInterfaceInfo(InterfaceID id) {
  if (id <= 0 || id >= INTERFACE_ID_COUNT)
    return NULL;
  const InterfaceList* list = InterfaceList::GetInstance();
  return list->id_to_plugin_info_[id];
}

void Dispatcher::SetSerializationRules(
    VarSerializationRules* var_serialization_rules) {
  serialization_rules_.reset(var_serialization_rules);
}

const void* Dispatcher::GetLocalInterface(const char* interface) {
  return local_get_interface_(interface);
}

bool Dispatcher::Send(IPC::Message* msg) {
  if (test_sink_)
    return test_sink_->Send(msg);
  if (channel_.get())
    return channel_->Send(msg);

  // Remote side crashed, drop this message.
  delete msg;
  return false;
}

}  // namespace proxy
}  // namespace pp

