// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/system_services.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/mac_logging.h"

extern "C" {
OSStatus SetApplicationIsDaemon(Boolean isDaemon);
void _LSSetApplicationLaunchServicesServerConnectionStatus(
    uint64_t flags,
    bool (^connection_allowed)(CFDictionaryRef options));
}  // extern "C"

namespace sandbox {

void DisableLaunchServices() {
  // Allow the process to continue without a LaunchServices ASN. The
  // INIT_Process function in HIServices will abort if it cannot connect to
  // launchservicesd to get an ASN. By setting this flag, HIServices skips
  // that.
  OSStatus status = SetApplicationIsDaemon(true);
  OSSTATUS_LOG_IF(ERROR, status != noErr, status) << "SetApplicationIsDaemon";

  // Close any connections to launchservicesd and use an always-false predicate
  // to discourage future attempts to connect.
  _LSSetApplicationLaunchServicesServerConnectionStatus(
      0, ^bool(CFDictionaryRef options) {
        return false;
      });
}

}  // namespace sandbox
