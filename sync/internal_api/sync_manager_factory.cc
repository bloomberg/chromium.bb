// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sync_manager_factory.h"

#include "sync/internal_api/sync_backup_manager.h"
#include "sync/internal_api/sync_manager_impl.h"
#include "sync/internal_api/sync_rollback_manager.h"

namespace syncer {

SyncManagerFactory::SyncManagerFactory(SyncManagerFactory::MANAGER_TYPE type)
    : type_(type) {
}

SyncManagerFactory::~SyncManagerFactory() {
}

scoped_ptr<SyncManager> SyncManagerFactory::CreateSyncManager(
      const std::string name) {
  switch (type_) {
    case NORMAL:
      return scoped_ptr<SyncManager>(new SyncManagerImpl(name));
    case BACKUP:
      return scoped_ptr<SyncManager>(new SyncBackupManager());
    case ROLLBACK:
      return scoped_ptr<SyncManager>(new SyncRollbackManager());
    default:
      NOTREACHED();
      return scoped_ptr<SyncManager>(new SyncManagerImpl(name));
  }
}

}  // namespace syncer
