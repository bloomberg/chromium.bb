// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// Prevent include problems that result from render_messages.h including skia
#define SK_IGNORE_STDINT_DOT_H

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"

extern LaunchNaClProcessFunc launch_nacl_process;

namespace nacl {
  bool SelLdrLauncher::Start(int imc_fd) {
    // send a synchronous message to the browser process
    Handle handle;
    if (!launch_nacl_process || !launch_nacl_process(imc_fd, &handle)) {
      return false;
    }
    // The handle we get back is the plugins end of the initial communication
    // channel - it is now created by the browser process
    channel_ = handle;
    return true;
  }
}

