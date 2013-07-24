// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"

#include "ppapi/cpp/var.h"

LaunchNaClProcessFunc launch_nacl_process = NULL;

namespace plugin {

bool SelLdrLauncherChrome::Start(const char* url) {
  NACL_NOTREACHED();
  return false;
}

bool SelLdrLauncherChrome::Start(PP_Instance instance,
                                 const char* url,
                                 bool uses_irt,
                                 bool uses_ppapi,
                                 bool enable_ppapi_dev,
                                 bool enable_dyncode_syscalls,
                                 bool enable_exception_handling,
                                 nacl::string* error_message) {
  *error_message = "";
  if (!launch_nacl_process)
    return false;
  PP_Var var_error_message;
  // send a synchronous message to the browser process
  if (launch_nacl_process(instance,
                          url,
                          PP_FromBool(uses_irt),
                          PP_FromBool(uses_ppapi),
                          PP_FromBool(enable_ppapi_dev),
                          PP_FromBool(enable_dyncode_syscalls),
                          PP_FromBool(enable_exception_handling),
                          &channel_,
                          &var_error_message) != PP_EXTERNAL_PLUGIN_OK) {
    pp::Var var_error_message_cpp(pp::PASS_REF, var_error_message);
    if (var_error_message_cpp.is_string()) {
      *error_message = var_error_message_cpp.AsString();
    }
    return false;
  }
  return true;
}

}  // namespace plugin
