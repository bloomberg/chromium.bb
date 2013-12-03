// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_SYNC_MANAGER_FACTORY_FOR_PROFILE_SYNC_TEST_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_SYNC_MANAGER_FACTORY_FOR_PROFILE_SYNC_TEST_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/sync_manager_factory.h"

namespace syncer {

class SyncManagerFactoryForProfileSyncTest : public syncer::SyncManagerFactory {
 public:
  SyncManagerFactoryForProfileSyncTest(base::Closure init_callback);
  virtual ~SyncManagerFactoryForProfileSyncTest();
  virtual scoped_ptr<syncer::SyncManager> CreateSyncManager(
      std::string name) OVERRIDE;
 private:
  base::Closure init_callback_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_SYNC_MANAGER_FACTORY_FOR_PROFILE_SYNC_TEST_H_
