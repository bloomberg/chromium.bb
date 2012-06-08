// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/error_counters.h"

namespace browser_sync {
namespace sessions {

ErrorCounters::ErrorCounters()
    : last_download_updates_result(UNSET),
      commit_result(UNSET) {
}

}  // namespace sessions
}  // namespace browser_sync
