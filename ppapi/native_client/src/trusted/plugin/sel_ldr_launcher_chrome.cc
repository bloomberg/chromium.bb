// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"

namespace plugin {

bool SelLdrLauncherChrome::Start(const char* url) {
  NACL_NOTREACHED();
  return false;
}

void SelLdrLauncherChrome::set_channel(NaClHandle channel) {
  CHECK(channel_ == NACL_INVALID_HANDLE);
  channel_ = channel;
}

}  // namespace plugin
