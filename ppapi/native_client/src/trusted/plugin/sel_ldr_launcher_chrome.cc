// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"

#include "native_client/src/trusted/plugin/nacl_entry_points.h"

#if NACL_WINDOWS
# include <windows.h>
#endif

LaunchNaClProcessFunc launch_nacl_process = NULL;

namespace plugin {

bool SelLdrLauncherChrome::Start(const char* url) {
  // send a synchronous message to the browser process
  // TODO(mseaborn): Remove the nacl_proc_handle and nacl_proc_id
  // arguments.  Chromium is being changed not to give the renderer
  // the Windows handle of the NaCl process.
  nacl::Handle nacl_proc_handle;
  int nacl_proc_id;
  // TODO(sehr): This is asserted to be one.  Remove this parameter.
  static const int kNumberOfChannelsToBeCreated = 1;
  if (!launch_nacl_process ||
      !launch_nacl_process(url,
                           kNumberOfChannelsToBeCreated,
                           &channel_,
                           &nacl_proc_handle,
                           &nacl_proc_id)) {
    return false;
  }

#if NACL_WINDOWS
  if (nacl_proc_handle != nacl::kInvalidHandle &&
      nacl_proc_handle != NULL) {
    CloseHandle(nacl_proc_handle);
  }
#endif
  return true;
}

}  // namespace plugin
