// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_

#include <vector>

#include "base/macros.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

// This class provides some common functionality for StatusChangeCheckers that
// observe many ProfileSyncServices.  This class is abstract.  Its descendants
// are expected to provide additional functionality.
class MultiClientStatusChangeChecker : public StatusChangeChecker,
                                       public syncer::SyncServiceObserver {
 public:
  explicit MultiClientStatusChangeChecker(
      std::vector<syncer::ProfileSyncService*> services);
  ~MultiClientStatusChangeChecker() override;

 protected:
  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // StatusChangeChecker implementations and stubs.
  bool IsExitConditionSatisfied(std::ostream* os) override = 0;

  const std::vector<syncer::ProfileSyncService*>& services() {
    return services_;
  }

 private:
  std::vector<syncer::ProfileSyncService*> services_;
  base::ScopedMultiSourceObservation<syncer::SyncService,
                                     syncer::SyncServiceObserver>
      scoped_observations_{this};

  DISALLOW_COPY_AND_ASSIGN(MultiClientStatusChangeChecker);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_
