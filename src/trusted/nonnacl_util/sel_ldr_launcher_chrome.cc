// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Prevent include problems that result from render_messages.h including skia
#define SK_IGNORE_STDINT_DOT_H

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"

namespace nacl {
  bool SelLdrLauncher::Start(const char* url, int imc_fd) {
    // send a synchronous message to the browser process
    Handle imc_handle;
    Handle nacl_proc_handle;
    int nacl_proc_id;
    if (!launch_nacl_process ||
        !launch_nacl_process(url,
                             imc_fd,
                             &imc_handle,
                             &nacl_proc_handle,
                             &nacl_proc_id)) {
      return false;
    }
    // The handle we get back is the plugins end of the initial communication
    // channel - it is now created by the browser process
    channel_ = imc_handle;
    return true;
  }
}

