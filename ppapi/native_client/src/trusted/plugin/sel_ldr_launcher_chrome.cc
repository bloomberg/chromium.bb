// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"

#include "native_client/src/trusted/plugin/nacl_entry_points.h"

LaunchNaClProcessFunc launch_nacl_process = NULL;

namespace plugin {

bool SelLdrLauncherChrome::Start(const char* url) {
  return Start(0, url);
}

bool SelLdrLauncherChrome::Start(PP_Instance instance, const char* url) {
  // send a synchronous message to the browser process
  // TODO(sehr): This is asserted to be one.  Remove this parameter.
  static const int kNumberOfChannelsToBeCreated = 1;
  if (!launch_nacl_process ||
      !launch_nacl_process(instance,
                           url,
                           kNumberOfChannelsToBeCreated,
                           &channel_)) {
    return false;
  }
  return true;
}

}  // namespace plugin
