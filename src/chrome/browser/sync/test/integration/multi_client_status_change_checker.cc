// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

#include "base/logging.h"
#include "components/sync/driver/profile_sync_service.h"

MultiClientStatusChangeChecker::MultiClientStatusChangeChecker(
    std::vector<syncer::ProfileSyncService*> services)
    : services_(services), scoped_observer_(this) {
  for (syncer::ProfileSyncService* service : services) {
    scoped_observer_.Add(service);
  }
}

MultiClientStatusChangeChecker::~MultiClientStatusChangeChecker() {}

void MultiClientStatusChangeChecker::OnStateChanged(syncer::SyncService* sync) {
  CheckExitCondition();
}
