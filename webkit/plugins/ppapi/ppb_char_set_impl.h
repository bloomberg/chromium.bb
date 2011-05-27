// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_CHAR_SET_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_CHAR_SET_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_char_set_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_CharSet_Impl : public ::ppapi::FunctionGroupBase,
                         public ::ppapi::thunk::PPB_CharSet_FunctionAPI {
 public:
  PPB_CharSet_Impl(PluginInstance* instance);
  ~PPB_CharSet_Impl();

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_CharSet_FunctionAPI* AsCharSet_FunctionAPI();

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

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PPB_CharSet_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_CHAR_SET_IMPL_H_
