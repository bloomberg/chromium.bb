// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_
#define PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_char_set_api.h"

struct PPB_CharSet_Dev;
struct PPB_Core;

namespace ppapi {
namespace proxy {

class SerializedVarReturnValue;

class PPB_CharSet_Proxy : public InterfaceProxy,
                          public ppapi::thunk::PPB_CharSet_FunctionAPI {
 public:
  PPB_CharSet_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_CharSet_Proxy();

  // FunctionGroupBase overrides.
  virtual ppapi::thunk::PPB_CharSet_FunctionAPI* AsPPB_CharSet_FunctionAPI()
      OVERRIDE;

  // PPB_CharSet_FunctionAPI implementation.
  virtual char* UTF16ToCharSet(PP_Instance instance,
                               const uint16_t* utf16, uint32_t utf16_len,
                               const char* output_char_set,
                               PP_CharSet_ConversionError on_error,
                               uint32_t* output_length) OVERRIDE;
  virtual uint16_t* CharSetToUTF16(PP_Instance instance,
                                   const char* input, uint32_t input_len,
                                   const char* input_char_set,
                                   PP_CharSet_ConversionError on_error,
                                   uint32_t* output_length) OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  static const InterfaceID kInterfaceID = INTERFACE_ID_PPB_CHAR_SET;

 private:
  void OnMsgGetDefaultCharSet(PP_Instance instance,
                              SerializedVarReturnValue result);

  DISALLOW_COPY_AND_ASSIGN(PPB_CharSet_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_
