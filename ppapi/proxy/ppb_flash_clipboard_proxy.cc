// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_clipboard_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/thunk/enter.h"

using ppapi::thunk::PPB_Flash_Clipboard_FunctionAPI;

namespace ppapi {
namespace proxy {

namespace {

typedef thunk::EnterFunctionNoLock<PPB_Flash_Clipboard_FunctionAPI>
    EnterFlashClipboardNoLock;

bool IsValidClipboardType(PP_Flash_Clipboard_Type clipboard_type) {
  return clipboard_type == PP_FLASH_CLIPBOARD_TYPE_STANDARD ||
         clipboard_type == PP_FLASH_CLIPBOARD_TYPE_SELECTION;
}

bool IsValidClipboardFormat(PP_Flash_Clipboard_Format format) {
  // Purposely excludes |PP_FLASH_CLIPBOARD_FORMAT_INVALID|.
  return format == PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT ||
         format == PP_FLASH_CLIPBOARD_FORMAT_HTML;
}

}  // namespace

PPB_Flash_Clipboard_Proxy::PPB_Flash_Clipboard_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_Clipboard_Proxy::~PPB_Flash_Clipboard_Proxy() {
}

PPB_Flash_Clipboard_FunctionAPI*
PPB_Flash_Clipboard_Proxy::AsPPB_Flash_Clipboard_FunctionAPI() {
  return this;
}

PP_Bool PPB_Flash_Clipboard_Proxy::IsFormatAvailable(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!IsValidClipboardType(clipboard_type) || !IsValidClipboardFormat(format))
    return PP_FALSE;

  bool result = false;
  dispatcher()->Send(new PpapiHostMsg_PPBFlashClipboard_IsFormatAvailable(
      API_ID_PPB_FLASH_CLIPBOARD,
      instance,
      static_cast<int>(clipboard_type),
      static_cast<int>(format),
      &result));
  return PP_FromBool(result);
}

PP_Var PPB_Flash_Clipboard_Proxy::ReadPlainText(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type) {
  if (!IsValidClipboardType(clipboard_type))
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlashClipboard_ReadPlainText(
      API_ID_PPB_FLASH_CLIPBOARD, instance,
      static_cast<int>(clipboard_type), &result));
  return result.Return(dispatcher());
}

int32_t PPB_Flash_Clipboard_Proxy::WritePlainText(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    const PP_Var& text) {
  if (!IsValidClipboardType(clipboard_type))
    return PP_ERROR_BADARGUMENT;

  dispatcher()->Send(new PpapiHostMsg_PPBFlashClipboard_WritePlainText(
      API_ID_PPB_FLASH_CLIPBOARD,
      instance,
      static_cast<int>(clipboard_type),
      SerializedVarSendInput(dispatcher(), text)));
  // Assume success, since it allows us to avoid a sync IPC.
  return PP_OK;
}

bool PPB_Flash_Clipboard_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Clipboard_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_IsFormatAvailable,
                        OnMsgIsFormatAvailable)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_ReadPlainText,
                        OnMsgReadPlainText)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_WritePlainText,
                        OnMsgWritePlainText)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Flash_Clipboard_Proxy::OnMsgIsFormatAvailable(
    PP_Instance instance,
    int clipboard_type,
    int format,
    bool* result) {
  EnterFlashClipboardNoLock enter(instance, true);
  if (enter.succeeded()) {
    *result = PP_ToBool(enter.functions()->IsFormatAvailable(
        instance,
        static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
        static_cast<PP_Flash_Clipboard_Format>(format)));
  } else {
    *result = false;
  }
}

void PPB_Flash_Clipboard_Proxy::OnMsgReadPlainText(
    PP_Instance instance,
    int clipboard_type,
    SerializedVarReturnValue result) {
  EnterFlashClipboardNoLock enter(instance, true);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->ReadPlainText(
                      instance,
                      static_cast<PP_Flash_Clipboard_Type>(clipboard_type)));
  }
}

void PPB_Flash_Clipboard_Proxy::OnMsgWritePlainText(
    PP_Instance instance,
    int clipboard_type,
    SerializedVarReceiveInput text) {
  EnterFlashClipboardNoLock enter(instance, true);
  if (enter.succeeded()) {
    int32_t result = enter.functions()->WritePlainText(
        instance,
        static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
        text.Get(dispatcher()));
    DLOG_IF(WARNING, result != PP_OK)
        << "Write to clipboard failed unexpectedly.";
    (void)result;  // Prevent warning in release mode.
  }
}

}  // namespace proxy
}  // namespace ppapi
