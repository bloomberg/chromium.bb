// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/sync_manager_factory_for_profile_sync_test.h"

#include "sync/internal_api/test/sync_manager_for_profile_sync_test.h"

namespace syncer {

SyncManagerFactoryForProfileSyncTest::SyncManagerFactoryForProfileSyncTest(
    base::Closure init_callback)
  : SyncManagerFactory(SyncManagerFactory::NORMAL),
    init_callback_(init_callback) {
}

SyncManagerFactoryForProfileSyncTest::~SyncManagerFactoryForProfileSyncTest() {}

scoped_ptr<syncer::SyncManager>
SyncManagerFactoryForProfileSyncTest::CreateSyncManager(std::string name) {
  return scoped_ptr<syncer::SyncManager>(
      new SyncManagerForProfileSyncTest(
          name,
          init_callback_));
}

}  // namespace syncer
