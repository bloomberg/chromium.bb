// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_char_set_impl.h"

#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/shared_impl/char_set_impl.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

using ::ppapi::thunk::PPB_CharSet_FunctionAPI;

namespace webkit {
namespace ppapi {

PPB_CharSet_Impl::PPB_CharSet_Impl(PluginInstance* instance)
    : instance_(instance) {
}

PPB_CharSet_Impl::~PPB_CharSet_Impl() {
}

PPB_CharSet_FunctionAPI* PPB_CharSet_Impl::AsCharSet_FunctionAPI() {
  return this;
}

char* PPB_CharSet_Impl::UTF16ToCharSet(PP_Instance instance,
                                       const uint16_t* utf16,
                                       uint32_t utf16_len,
                                       const char* output_char_set,
                                       PP_CharSet_ConversionError on_error,
                                       uint32_t* output_length) {
  return ::ppapi::CharSetImpl::UTF16ToCharSet(
      PluginModule::GetCore(), utf16, utf16_len, output_char_set, on_error,
      output_length);
}

uint16_t* PPB_CharSet_Impl::CharSetToUTF16(PP_Instance instance,
                                           const char* input,
                                           uint32_t input_len,
                                           const char* input_char_set,
                                           PP_CharSet_ConversionError on_error,
                                           uint32_t* output_length) {
  return ::ppapi::CharSetImpl::CharSetToUTF16(
      PluginModule::GetCore(), input, input_len, input_char_set, on_error,
      output_length);
}

PP_Var PPB_CharSet_Impl::GetDefaultCharSet(PP_Instance instance) {
  std::string encoding = instance_->delegate()->GetDefaultEncoding();
  return StringVar::StringToPPVar(instance_->module(), encoding);
}

}  // namespace ppapi
}  // namespace webkit
