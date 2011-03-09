// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_clipboard_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

namespace {

bool IsValidClipboardType(PP_Flash_Clipboard_Type clipboard_type) {
  return clipboard_type == PP_FLASH_CLIPBOARD_TYPE_STANDARD ||
         clipboard_type == PP_FLASH_CLIPBOARD_TYPE_SELECTION ||
         clipboard_type == PP_FLASH_CLIPBOARD_TYPE_DRAG;
}

PP_Var ReadPlainText(PP_Instance instance_id,
                     PP_Flash_Clipboard_Type clipboard_type) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_MakeUndefined();

  if (!IsValidClipboardType(clipboard_type))
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBFlashClipboard_ReadPlainText(
      INTERFACE_ID_PPB_FLASH_CLIPBOARD, instance_id,
      static_cast<int>(clipboard_type), &result));
  return result.Return(dispatcher);
}

int32_t WritePlainText(PP_Instance instance_id,
                       PP_Flash_Clipboard_Type clipboard_type,
                       PP_Var text) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  if (!IsValidClipboardType(clipboard_type))
    return PP_ERROR_BADARGUMENT;

  dispatcher->Send(new PpapiHostMsg_PPBFlashClipboard_WritePlainText(
      INTERFACE_ID_PPB_FLASH_CLIPBOARD,
      instance_id,
      static_cast<int>(clipboard_type),
      SerializedVarSendInput(dispatcher, text)));
  // Assume success, since it allows us to avoid a sync IPC.
  return PP_OK;
}

const PPB_Flash_Clipboard flash_clipboard_interface = {
  &ReadPlainText,
  &WritePlainText
};

InterfaceProxy* CreateFlashClipboardProxy(Dispatcher* dispatcher,
                                          const void* target_interface) {
  return new PPB_Flash_Clipboard_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Flash_Clipboard_Proxy::PPB_Flash_Clipboard_Proxy(
    Dispatcher* dispatcher, const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Flash_Clipboard_Proxy::~PPB_Flash_Clipboard_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_Clipboard_Proxy::GetInfo() {
  static const Info info = {
    &flash_clipboard_interface,
    PPB_FLASH_CLIPBOARD_INTERFACE,
    INTERFACE_ID_PPB_FLASH_CLIPBOARD,
    false,
    &CreateFlashClipboardProxy
  };
  return &info;
}

bool PPB_Flash_Clipboard_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Clipboard_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_ReadPlainText,
                        OnMsgReadPlainText)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_WritePlainText,
                        OnMsgWritePlainText)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Flash_Clipboard_Proxy::OnMsgReadPlainText(
    PP_Instance instance_id,
    int clipboard_type,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_flash_clipboard_target()->ReadPlainText(
                    instance_id,
                    static_cast<PP_Flash_Clipboard_Type>(clipboard_type)));
}

void PPB_Flash_Clipboard_Proxy::OnMsgWritePlainText(
    PP_Instance instance_id,
    int clipboard_type,
    SerializedVarReceiveInput text) {
  int32_t result = ppb_flash_clipboard_target()->WritePlainText(
      instance_id,
      static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
      text.Get(dispatcher()));
  LOG_IF(WARNING, result != PP_OK) << "Write to clipboard failed unexpectedly.";
}

}  // namespace proxy
}  // namespace pp
