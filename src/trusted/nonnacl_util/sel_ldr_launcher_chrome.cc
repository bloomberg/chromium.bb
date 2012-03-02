// Copyright (c) 2012 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"

LaunchNaClProcessFunc launch_nacl_process = NULL;

#if !defined(NACL_STANDALONE)
namespace nacl {
bool SelLdrLauncher::Start(const char* url) {
  // send a synchronous message to the browser process
  Handle nacl_proc_handle;
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
  NaClHandlePassBrowserRememberHandle(nacl_proc_id, nacl_proc_handle);
#endif

  CloseHandlesAfterLaunch();
  // TODO(gregoryd): the handle is currently returned on Windows only.
  child_process_ = nacl_proc_handle;
  return true;
}

bool SelLdrLauncher::Start(int socket_count,
                           Handle* result_sockets,
                           const char* url) {
  UNREFERENCED_PARAMETER(socket_count);
  UNREFERENCED_PARAMETER(result_sockets);
  return Start(url);
}
}  // namespace
#endif  // !defined(NACL_STANDALONE)
