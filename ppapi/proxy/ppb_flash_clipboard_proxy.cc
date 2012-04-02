// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
         format == PP_FLASH_CLIPBOARD_FORMAT_HTML ||
         format == PP_FLASH_CLIPBOARD_FORMAT_RTF;
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
PP_Var PPB_Flash_Clipboard_Proxy::ReadData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!IsValidClipboardType(clipboard_type) || !IsValidClipboardFormat(format))
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlashClipboard_ReadData(
      API_ID_PPB_FLASH_CLIPBOARD, instance,
      static_cast<int>(clipboard_type), static_cast<int>(format), &result));
  return result.Return(dispatcher());
}


int32_t PPB_Flash_Clipboard_Proxy::WriteData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    uint32_t data_item_count,
    const PP_Flash_Clipboard_Format formats[],
    const PP_Var data_items[]) {
  if (!IsValidClipboardType(clipboard_type))
    return PP_ERROR_BADARGUMENT;

  for (size_t i = 0; i < data_item_count; ++i) {
    if (!IsValidClipboardFormat(formats[i]))
      return PP_ERROR_BADARGUMENT;
  }

  std::vector<int> formats_vector(formats, formats + data_item_count);

  std::vector<SerializedVar> data_items_vector;
  SerializedVarSendInput::ConvertVector(
      dispatcher(),
      data_items,
      data_item_count,
      &data_items_vector);

  dispatcher()->Send(new PpapiHostMsg_PPBFlashClipboard_WriteData(
      API_ID_PPB_FLASH_CLIPBOARD,
      instance,
      static_cast<int>(clipboard_type),
      formats_vector,
      data_items_vector));
  // Assume success, since it allows us to avoid a sync IPC.
  return PP_OK;
}

bool PPB_Flash_Clipboard_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Clipboard_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_IsFormatAvailable,
                        OnMsgIsFormatAvailable)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_ReadData,
                        OnMsgReadData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashClipboard_WriteData,
                        OnMsgWriteData)
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

void PPB_Flash_Clipboard_Proxy::OnMsgReadData(
    PP_Instance instance,
    int clipboard_type,
    int format,
    SerializedVarReturnValue result) {
  EnterFlashClipboardNoLock enter(instance, true);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->ReadData(
                      instance,
                      static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
                      static_cast<PP_Flash_Clipboard_Format>(format)));
  }
}

void PPB_Flash_Clipboard_Proxy::OnMsgWriteData(
    PP_Instance instance,
    int clipboard_type,
    const std::vector<int>& formats,
    SerializedVarVectorReceiveInput data_items) {
  EnterFlashClipboardNoLock enter(instance, true);
  if (enter.succeeded()) {
    uint32_t data_item_count;
    PP_Var* data_items_array = data_items.Get(dispatcher(), &data_item_count);
    CHECK(data_item_count == formats.size());

    scoped_array<PP_Flash_Clipboard_Format> formats_array(
        new PP_Flash_Clipboard_Format[formats.size()]);
    for (uint32_t i = 0; i < formats.size(); ++i) {
      formats_array[i] = static_cast<PP_Flash_Clipboard_Format>(formats[i]);
    }

    int32_t result = enter.functions()->WriteData(
        instance,
        static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
        data_item_count,
        formats_array.get(),
        data_items_array);
    DLOG_IF(WARNING, result != PP_OK)
        << "Write to clipboard failed unexpectedly.";
    (void)result;  // Prevent warning in release mode.
  }
}

}  // namespace proxy
}  // namespace ppapi
