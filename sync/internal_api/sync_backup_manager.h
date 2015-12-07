// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_BACKUP_MANAGER_H_
#define SYNC_INTERNAL_API_SYNC_BACKUP_MANAGER_H_

#include <set>

#include "sync/internal_api/sync_rollback_manager_base.h"
#include "url/gurl.h"

namespace syncer {

// SyncBackupManager runs before user signs in to sync to back up user's data
// before sync starts. The data that's backed up can be used to restore user's
// settings to pre-sync state.
class SYNC_EXPORT_PRIVATE SyncBackupManager : public SyncRollbackManagerBase {
 public:
  SyncBackupManager();
  ~SyncBackupManager() override;

  // SyncManager implementation.
  void Init(InitArgs* args) override;
  void SaveChanges() override;
  SyncStatus GetDetailedStatus() const override;
  void ShutdownOnSyncThread(ShutdownReason reason) override;

  // DirectoryChangeDelegate implementation.
  ModelTypeSet HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) override;

  void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RequestEmitDebugInfo() override;
  void ClearServerData(const ClearServerDataCallback& callback) override;

 private:
  // Replaces local IDs with server IDs and clear unsynced bit of modified
  // entries.
  void NormalizeEntries();

  // Manipulate preference nodes so that they'll be overwritten by local
  // preference values during model association, i.e. local wins instead of
  // server wins. This is for preventing backup from changing preferences in
  // case backup DB has hijacked preferences.
  void HideSyncPreference(ModelType pref_type);

  // Handles of unsynced entries caused by local model changes.
  std::set<int64> unsynced_;

  // True if NormalizeEntries() is being called.
  bool in_normalization_;

  SyncStatus status_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackupManager);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_BACKUP_MANAGER_H_
