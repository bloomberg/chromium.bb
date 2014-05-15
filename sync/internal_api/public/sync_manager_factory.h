// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_MANAGER_FACTORY_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_MANAGER_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"

namespace syncer {

class SyncManager;

// Helper class to allow dependency injection of the SyncManager.
class SYNC_EXPORT SyncManagerFactory {
 public:
  enum MANAGER_TYPE {
    NORMAL,     // Normal sync manager
    BACKUP,     // Backup sync manager
    ROLLBACK,   // Rollback sync manager
  };

  SyncManagerFactory(MANAGER_TYPE type);
  virtual ~SyncManagerFactory();

  virtual scoped_ptr<SyncManager> CreateSyncManager(std::string name);

 private:
  MANAGER_TYPE type_;
  DISALLOW_COPY_AND_ASSIGN(SyncManagerFactory);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_MANAGER_FACTORY_H_
