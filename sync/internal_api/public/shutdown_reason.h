// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SHUTDOWN_REASON_H_
#define SYNC_INTERNAL_API_PUBLIC_SHUTDOWN_REASON_H_

#include "sync/base/sync_export.h"

namespace syncer {

// Reason for shutting down sync engine.
enum SYNC_EXPORT ShutdownReason {
  STOP_SYNC,         // Sync is asked to stop, e.g. due to scarce resource.
  DISABLE_SYNC,      // Sync is disabled, e.g. user sign out, dashboard clear.
  BROWSER_SHUTDOWN,  // Browser is closed.
  SWITCH_MODE_SYNC,  // Engine is shut down and a new engine will start in
                     // sync mode.
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SHUTDOWN_REASON_H_
