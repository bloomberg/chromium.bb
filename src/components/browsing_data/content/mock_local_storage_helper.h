// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_LOCAL_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_LOCAL_STORAGE_HELPER_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "components/browsing_data/content/local_storage_helper.h"

namespace browsing_data {

// Mock for browsing_data::LocalStorageHelper.
// Use AddLocalStorageSamples() or add directly to response_ list, then
// call Notify().
class MockLocalStorageHelper : public browsing_data::LocalStorageHelper {
 public:
  explicit MockLocalStorageHelper(content::BrowserContext* context);

  // browsing_data::LocalStorageHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteOrigin(const url::Origin& origin,
                    base::OnceClosure callback) override;

  // Adds some LocalStorageInfo samples.
  void AddLocalStorageSamples();

  // Add a LocalStorageInfo entry for a single origin.
  void AddLocalStorageForOrigin(const url::Origin& origin, int64_t size);

  // Notifies the callback.
  void Notify();

  // Marks all local storage files as existing.
  void Reset();

  // Returns true if all local storage files were deleted since the last Reset()
  // invocation.
  bool AllDeleted();

  url::Origin last_deleted_origin_;

 private:
  ~MockLocalStorageHelper() override;

  FetchCallback callback_;

  std::map<const url::Origin, bool> origins_;

  std::list<content::StorageUsageInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockLocalStorageHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_LOCAL_STORAGE_HELPER_H_
