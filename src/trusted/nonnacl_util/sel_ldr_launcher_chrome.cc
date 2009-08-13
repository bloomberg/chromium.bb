// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// Prevent include problems that result from render_messages.h including skia
#define SK_IGNORE_STDINT_DOT_H
#define NOMINMAX

#include "native_client/src/include/portability.h"
#include "chrome/common/render_messages.h"
#include "chrome/nacl/nacl_descriptors.h"
#include "chrome/renderer/render_thread.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {
  bool SelLdrLauncher::Start(int imc_fd) {
    // send a synchronous message to the browser process
    FileDescriptor handle;
    if (!RenderThread::current()->Send(new ViewHostMsg_LaunchNaCl(
        imc_fd, &handle))) {
      return false;
    }
    // The handle we get back is the plugins end of the initial communication
    // channel - it is now created by the browser process
    channel_ = NATIVE_HANDLE(handle);
    return true;
  }
}

