// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SYSTEM_SERVICES_H_
#define SANDBOX_MAC_SYSTEM_SERVICES_H_

#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// Tells LaunchServices that the process is not a normal GUI application, which
// permits the process to call into various Carbon services without requiring
// an ASN. This marks the process as a "daemon". If the process is currently
// connected to LaunchServices, the connection is closed and the system is
// instructed to not attempt to connect again. This does not alter the sandbox
// policy (i.e., direct lookup requests to launchd will not be affected), but
// it instructs system frameworks to not attempt to connect.
SEATBELT_EXPORT void DisableLaunchServices();

}  // namespace sandbox

#endif  // SANDBOX_MAC_SYSTEM_SERVICES_H_
