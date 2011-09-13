// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_char_set_proxy.h"

#include "base/basictypes.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/char_set_impl.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

const PPB_Memory_Dev* GetMemoryDevInterface() {
  return static_cast<const PPB_Memory_Dev*>(
      PluginDispatcher::GetInterfaceFromDispatcher(PPB_MEMORY_DEV_INTERFACE));
}

InterfaceProxy* CreateCharSetProxy(Dispatcher* dispatcher,
                                   const void* target_interface) {
  return new PPB_CharSet_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_CharSet_Proxy::PPB_CharSet_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_CharSet_Proxy::~PPB_CharSet_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_CharSet_Proxy::GetInfo() {
  static const Info info = {
    ppapi::thunk::GetPPB_CharSet_Thunk(),
    PPB_CHAR_SET_DEV_INTERFACE,
    INTERFACE_ID_PPB_CHAR_SET,
    false,
    &CreateCharSetProxy,
  };
  return &info;
}

ppapi::thunk::PPB_CharSet_FunctionAPI*
PPB_CharSet_Proxy::AsPPB_CharSet_FunctionAPI() {
  return this;
}

char* PPB_CharSet_Proxy::UTF16ToCharSet(
    PP_Instance instance,
    const uint16_t* utf16, uint32_t utf16_len,
    const char* output_char_set,
    PP_CharSet_ConversionError on_error,
    uint32_t* output_length) {
  return ppapi::CharSetImpl::UTF16ToCharSet(
      GetMemoryDevInterface(), utf16, utf16_len, output_char_set, on_error,
      output_length);
}

uint16_t* PPB_CharSet_Proxy::CharSetToUTF16(
    PP_Instance instance,
    const char* input, uint32_t input_len,
    const char* input_char_set,
    PP_CharSet_ConversionError on_error,
    uint32_t* output_length) {
  return ppapi::CharSetImpl::CharSetToUTF16(
      GetMemoryDevInterface(), input, input_len, input_char_set, on_error,
      output_length);
}

PP_Var PPB_CharSet_Proxy::GetDefaultCharSet(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBCharSet_GetDefaultCharSet(
      INTERFACE_ID_PPB_CHAR_SET, instance, &result));
  return result.Return(dispatcher);
}

bool PPB_CharSet_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_CharSet_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCharSet_GetDefaultCharSet,
                        OnMsgGetDefaultCharSet)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_CharSet_Proxy::OnMsgGetDefaultCharSet(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  ppapi::thunk::EnterFunctionNoLock<ppapi::thunk::PPB_CharSet_FunctionAPI>
      enter(instance, true);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetDefaultCharSet(instance));
}

}  // namespace proxy
}  // namespace ppapi
