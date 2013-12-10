// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_SYNC_MANAGER_FOR_PROFILE_SYNC_TEST_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_SYNC_MANAGER_FOR_PROFILE_SYNC_TEST_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "sync/internal_api/sync_manager_impl.h"

namespace syncer {

// This class is used to help implement the TestProfileSyncService.
// Those tests try to test sync without instantiating a real backend.
class SyncManagerForProfileSyncTest
    : public syncer::SyncManagerImpl {
 public:
  SyncManagerForProfileSyncTest(std::string name,
                                base::Closure init_callback);
  virtual ~SyncManagerForProfileSyncTest();
  virtual void NotifyInitializationSuccess() OVERRIDE;

 private:
  base::Closure init_callback_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_SYNC_MANAGER_FOR_PROFILE_SYNC_TEST_H_
