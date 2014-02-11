// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"

LaunchNaClProcessFunc launch_nacl_process = NULL;

namespace plugin {

bool SelLdrLauncherChrome::Start(const char* url) {
  NACL_NOTREACHED();
  return false;
}

void SelLdrLauncherChrome::Start(PP_Instance instance,
                                 const char* url,
                                 bool uses_irt,
                                 bool uses_ppapi,
                                 bool enable_ppapi_dev,
                                 bool enable_dyncode_syscalls,
                                 bool enable_exception_handling,
                                 bool enable_crash_throttling,
                                 PP_Var* error_message,
                                 pp::CompletionCallback callback) {
  if (!launch_nacl_process) {
    pp::Module::Get()->core()->CallOnMainThread(0, callback, PP_ERROR_FAILED);
    return;
  }
  launch_nacl_process(instance,
                      url,
                      PP_FromBool(uses_irt),
                      PP_FromBool(uses_ppapi),
                      PP_FromBool(enable_ppapi_dev),
                      PP_FromBool(enable_dyncode_syscalls),
                      PP_FromBool(enable_exception_handling),
                      PP_FromBool(enable_crash_throttling),
                      &channel_,
                      error_message,
                      callback.pp_completion_callback());
}

}  // namespace plugin
