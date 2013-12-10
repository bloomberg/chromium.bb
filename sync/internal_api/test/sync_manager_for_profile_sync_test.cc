// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/test/sync_manager_for_profile_sync_test.h"

#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/syncable/directory.h"

namespace syncer {

SyncManagerForProfileSyncTest::SyncManagerForProfileSyncTest(
    std::string name,
    base::Closure init_callback)
  : SyncManagerImpl(name),
    init_callback_(init_callback) {}

SyncManagerForProfileSyncTest::~SyncManagerForProfileSyncTest() {}

void SyncManagerForProfileSyncTest::NotifyInitializationSuccess() {
  UserShare* user_share = GetUserShare();
  syncable::Directory* directory = user_share->directory.get();

  if (!init_callback_.is_null())
    init_callback_.Run();

  ModelTypeSet early_download_types;
  early_download_types.PutAll(ControlTypes());
  early_download_types.PutAll(PriorityUserTypes());
  for (ModelTypeSet::Iterator it = early_download_types.First();
       it.Good(); it.Inc()) {
    if (!directory->InitialSyncEndedForType(it.Get())) {
      syncer::TestUserShare::CreateRoot(it.Get(), user_share);
    }
  }

  SyncManagerImpl::NotifyInitializationSuccess();
}

};
