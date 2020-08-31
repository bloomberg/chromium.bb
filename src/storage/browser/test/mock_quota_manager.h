// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;

namespace storage {

// Mocks the pieces of QuotaManager's interface.
//
// For usage/quota tracking test:
// Usage and quota information can be updated by following private helper
// methods: SetQuota() and UpdateUsage().
//
// For time-based deletion test:
// Origins can be added to the mock by calling AddOrigin, and that list of
// origins is then searched through in GetOriginsModifiedSince.
// Neither GetOriginsModifiedSince nor DeleteOriginData touches the actual
// origin data stored in the profile.
class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(bool is_incognito,
                   const base::FilePath& profile_path,
                   scoped_refptr<base::SingleThreadTaskRunner> io_thread,
                   scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  // Overrides QuotaManager's implementation. The internal usage data is
  // updated when MockQuotaManagerProxy::NotifyStorageModified() is
  // called.  The internal quota value can be updated by calling
  // a helper method MockQuotaManagerProxy::SetQuota().
  void GetUsageAndQuota(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override;

  // Overrides QuotaManager's implementation with a canned implementation that
  // allows clients to set up the origin database that should be queried. This
  // method will only search through the origins added explicitly via AddOrigin.
  void GetOriginsModifiedSince(blink::mojom::StorageType type,
                               base::Time modified_since,
                               GetOriginsCallback callback) override;

  // Removes an origin from the canned list of origins, but doesn't touch
  // anything on disk. The caller must provide |quota_client_types| which
  // specifies the types of QuotaClients which should be removed from this
  // origin. Setting the mask to AllQuotaClientTypes() will remove all clients
  // from the origin, regardless of type.
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        QuotaClientTypes quota_client_types,
                        StatusCallback callback) override;

  // Overrides QuotaManager's implementation so that tests can observe
  // calls to this function.
  void NotifyWriteFailed(const url::Origin& origin) override;

  // Helper method for updating internal quota info.
  void SetQuota(const url::Origin& origin, StorageType type, int64_t quota);

  // Helper methods for timed-deletion testing:
  // Adds an origin to the canned list that will be searched through via
  // GetOriginsModifiedSince.
  // |quota_clients| specified the types of QuotaClients this canned origin
  // contains.
  bool AddOrigin(const url::Origin& origin,
                 StorageType type,
                 QuotaClientTypes quota_client_types,
                 base::Time modified);

  // Helper methods for timed-deletion testing:
  // Checks an origin and type against the origins that have been added via
  // AddOrigin and removed via DeleteOriginData. If the origin exists in the
  // canned list with the proper StorageType and client, returns true.
  bool OriginHasData(const url::Origin& origin,
                     StorageType type,
                     QuotaClientType quota_client_type) const;

  std::map<const url::Origin, int> write_error_tracker() const {
    return write_error_tracker_;
  }

 protected:
  ~MockQuotaManager() override;

 private:
  friend class MockQuotaManagerProxy;

  // Contains the essential bits of information about an origin that the
  // MockQuotaManager needs to understand for time-based deletion:
  // the origin itself, the StorageType and its modification time.
  struct OriginInfo {
    OriginInfo(const url::Origin& origin,
               StorageType type,
               QuotaClientTypes quota_clients,
               base::Time modified);
    ~OriginInfo();

    OriginInfo(const OriginInfo&) = delete;
    OriginInfo& operator=(const OriginInfo&) = delete;

    OriginInfo(OriginInfo&&);
    OriginInfo& operator=(OriginInfo&&);

    url::Origin origin;
    StorageType type;
    QuotaClientTypes quota_client_types;
    base::Time modified;
  };

  // Contains the essential information for each origin for usage/quota testing.
  // (Ideally this should probably merged into the above struct, but for
  // regular usage/quota testing we hardly need modified time but only
  // want to keep usage and quota information, so this struct exists.
  struct StorageInfo {
    StorageInfo();
    ~StorageInfo();
    int64_t usage;
    int64_t quota;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
  };

  // This must be called via MockQuotaManagerProxy.
  void UpdateUsage(const url::Origin& origin, StorageType type, int64_t delta);
  void DidGetModifiedSince(GetOriginsCallback callback,
                           std::unique_ptr<std::set<url::Origin>> origins,
                           StorageType storage_type);
  void DidDeleteOriginData(StatusCallback callback,
                           blink::mojom::QuotaStatusCode status);

  // The list of stored origins that have been added via AddOrigin.
  std::vector<OriginInfo> origins_;
  std::map<std::pair<url::Origin, StorageType>, StorageInfo>
      usage_and_quota_map_;

  // Tracks number of times NotifyFailedWrite has been called per origin.
  std::map<const url::Origin, int> write_error_tracker_;

  base::WeakPtrFactory<MockQuotaManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManager);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_
