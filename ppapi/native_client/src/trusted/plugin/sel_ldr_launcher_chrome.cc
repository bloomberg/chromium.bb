// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"

#include "native_client/src/trusted/plugin/nacl_entry_points.h"

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
                                 bool enable_dyncode_syscalls) {
  if (!launch_nacl_process)
    return false;
  // send a synchronous message to the browser process
  if (launch_nacl_process(instance,
                          url,
                          PP_FromBool(uses_irt),
                          PP_FromBool(uses_ppapi),
                          PP_FromBool(enable_ppapi_dev),
                          PP_FromBool(enable_dyncode_syscalls),
                          &channel_) != PP_NACL_OK) {
    return false;
  }
  return true;
}

}  // namespace plugin
