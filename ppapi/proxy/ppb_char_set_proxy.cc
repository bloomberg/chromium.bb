// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_char_set_proxy.h"

#include "base/basictypes.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/char_set_impl.h"

namespace pp {
namespace proxy {

namespace {

const PPB_Core* GetCoreInterface() {
  return static_cast<const PPB_Core*>(
      PluginDispatcher::GetInterfaceFromDispatcher(PPB_CORE_INTERFACE));
}

char* UTF16ToCharSet(PP_Instance /* instance */,
                     const uint16_t* utf16, uint32_t utf16_len,
                     const char* output_char_set,
                     PP_CharSet_ConversionError on_error,
                     uint32_t* output_length) {
  return pp::shared_impl::CharSetImpl::UTF16ToCharSet(
      GetCoreInterface(), utf16, utf16_len, output_char_set, on_error,
      output_length);
}

uint16_t* CharSetToUTF16(PP_Instance /* instance */,
                         const char* input, uint32_t input_len,
                         const char* input_char_set,
                         PP_CharSet_ConversionError on_error,
                         uint32_t* output_length) {
  return pp::shared_impl::CharSetImpl::CharSetToUTF16(
      GetCoreInterface(), input, input_len, input_char_set, on_error,
      output_length);
}

PP_Var GetDefaultCharSet(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBCharSet_GetDefaultCharSet(
      INTERFACE_ID_PPB_CHAR_SET, instance, &result));
  return result.Return(dispatcher);
}

const PPB_CharSet_Dev charset_interface = {
  &UTF16ToCharSet,
  &CharSetToUTF16,
  &GetDefaultCharSet
};

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
    &charset_interface,
    PPB_CHAR_SET_DEV_INTERFACE,
    INTERFACE_ID_PPB_CHAR_SET,
    false,
    &CreateCharSetProxy,
  };
  return &info;
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
  result.Return(dispatcher(),
                ppb_char_set_target()->GetDefaultCharSet(instance));
}

}  // namespace proxy
}  // namespace pp
