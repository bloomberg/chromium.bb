// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_MOCK_QUOTA_MANAGER_H_
#define WEBKIT_QUOTA_MOCK_QUOTA_MANAGER_H_
#pragma once

#include <vector>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

namespace quota {

// Mocks the pieces of QuotaManager's interface that are used for time-based
// deletion of a profile's browsing data. Origins can be added to the mock by
// calling AddOrigin, and that list of origins is then searched through in
// GetOriginsModifiedSince. Neither GetOriginsModifiedSince nor DeleteOriginData
// touches the actual origin data stored in the profile.
class MockQuotaManager : public QuotaManager {
 public:
  // Contains the essential bits of information about an origin that the
  // MockQuotaManager needs to understand: the origin itself, the StorageType,
  // and its modification time.
  struct OriginInfo {
    OriginInfo(const GURL& origin,
               StorageType type,
               base::Time modified);
    ~OriginInfo();

    GURL origin;
    StorageType type;
    base::Time modified;
  };

  MockQuotaManager(bool is_incognito,
                   const FilePath& profile_path,
                   base::MessageLoopProxy* io_thread,
                   base::MessageLoopProxy* db_thread,
                   SpecialStoragePolicy* special_storage_policy);

  virtual ~MockQuotaManager();

  // Adds an origin to the canned list that will be searched through via
  // GetOriginsModifiedSince.
  bool AddOrigin(const GURL& origin, StorageType type, base::Time modified);

  // Checks an origin and type against the origins that have been added via
  // AddOrigin and removed via DeleteOriginData. If the origin exists in the
  // canned list with the proper StorageType, returns true.
  bool OriginHasData(const GURL& origin, StorageType type) const;

  // Overrides QuotaManager's implementation with a canned implementation that
  // allows clients to set up the origin database that should be queried. This
  // method will only search through the origins added explicitly via AddOrigin.
  virtual void GetOriginsModifiedSince(StorageType type,
                                       base::Time modified_since,
                                       GetOriginsCallback* callback) OVERRIDE;

  // Removes an origin from the canned list of origins, but doesn't touch
  // anything on disk.
  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                StatusCallback* callback) OVERRIDE;
 private:
  class GetModifiedSinceTask;
  class DeleteOriginDataTask;

  // The list of stored origins that have been added via AddOrigin.
  std::vector<OriginInfo> origins_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManager);
};

}  // namespace quota

#endif // WEBKIT_QUOTA_MOCK_QUOTA_MANAGER_H_
