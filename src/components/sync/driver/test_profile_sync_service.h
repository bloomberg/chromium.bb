// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TEST_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_TEST_PROFILE_SYNC_SERVICE_H_

#include "base/macros.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/js/js_event_handler.h"

namespace syncer {

class SyncPrefs;

class TestProfileSyncService : public syncer::ProfileSyncService {
 public:
  explicit TestProfileSyncService(InitParams init_params);

  ~TestProfileSyncService() override;

  void OnConfigureDone(
      const syncer::DataTypeManager::ConfigureResult& result) override;

  // TODO(crbug.com/871221): This is overridden here to return true by default,
  // as a workaround for tests not setting up an authenticated account, and
  // IsSyncFeatureEnabled() therefore returning false.
  bool IsAuthenticatedAccountPrimary() const override;

  // We implement our own version to avoid some DCHECKs.
  syncer::UserShare* GetUserShare() const override;

  // Raise visibility to ease testing.
  using ProfileSyncService::NotifyObservers;

  syncer::SyncPrefs* sync_prefs() { return &sync_prefs_; }

 protected:
  // Return null handle to use in backend initialization to avoid receiving
  // js messages on UI loop when it's being destroyed, which are not deleted
  // and cause memory leak in test.
  syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProfileSyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TEST_PROFILE_SYNC_SERVICE_H_
