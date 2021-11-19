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

#include "base/macros.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

// Mocks the pieces of QuotaManager's interface.
//
// For usage/quota tracking test:
// Usage and quota information can be updated by following private helper
// methods: SetQuota() and UpdateUsage().
//
// For time-based deletion test:
// Storage keys can be added to the mock by calling AddStorageKey, and that list
// of storage keys is then searched through in GetStorageKeysModifiedBetween.
// Neither GetStorageKeysModifiedBetween nor DeleteStorageKeyData touches the
// actual storage key data stored in the profile.
class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(bool is_incognito,
                   const base::FilePath& profile_path,
                   scoped_refptr<base::SingleThreadTaskRunner> io_thread,
                   scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  // Overrides QuotaManager's implementation that maintains an internal
  // container of created buckets and avoids going to the DB.
  void GetOrCreateBucket(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  // Overrides QuotaManager's implementation to fetch from an internal
  // container populated by calls to GetOrCreateBucket.
  void GetBucket(const blink::StorageKey& storage_key,
                 const std::string& bucket_name,
                 blink::mojom::StorageType type,
                 base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  // Overrides QuotaManager's implementation. The internal usage data is
  // updated when MockQuotaManagerProxy::NotifyStorageModified() is
  // called.  The internal quota value can be updated by calling
  // a helper method MockQuotaManagerProxy::SetQuota().
  void GetUsageAndQuota(const blink::StorageKey& storage_key,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override;

  // Overrides QuotaManager's implementation with a canned implementation that
  // allows clients to set up the storage key database that should be queried.
  // This method will only search through the storage keys added explicitly via
  // AddStorageKey.
  void GetBucketsModifiedBetween(blink::mojom::StorageType type,
                                 base::Time begin,
                                 base::Time end,
                                 GetBucketsCallback callback) override;

  // Removes a bucket from the canned list of buckets, but doesn't touch
  // anything on disk. The caller must provide `quota_client_types` which
  // specifies the types of QuotaClients which should be removed from this
  // bucket. Setting the mask to AllQuotaClientTypes() will remove all
  // clients from the bucket, regardless of type.
  void DeleteBucketData(const BucketLocator& bucket,
                        QuotaClientTypes quota_client_types,
                        StatusCallback callback) override;

  // Overrides QuotaManager's implementation so that tests can observe
  // calls to this function.
  void NotifyWriteFailed(const blink::StorageKey& storage_key) override;

  // Helper method for updating internal quota info.
  void SetQuota(const blink::StorageKey& storage_key,
                blink::mojom::StorageType type,
                int64_t quota);

  // Helper methods for timed-deletion testing:
  // Adds a bucket to the canned list that will be searched through via
  // GetBucketsModifiedBetween.
  // `quota_clients` specified the types of QuotaClients this canned bucket
  // contains.
  bool AddBucket(const BucketInfo& bucket,
                 QuotaClientTypes quota_client_types,
                 base::Time modified);

  // Creates a BucketInfo object with a generated BucketId. Makes sure newly
  // created buckets are created with a unique id and with the specified
  // attributes.
  BucketInfo CreateBucket(const blink::StorageKey& storage_key,
                          const std::string& name,
                          blink::mojom::StorageType type);

  // Helper methods for timed-deletion testing:
  // Checks a bucket against the buckets that have been added via AddBucket and
  // removed via DeleteBucketData. If the bucket exists in the canned list with
  // the proper client, returns true.
  bool BucketHasData(const BucketInfo& bucket,
                     QuotaClientType quota_client_type) const;

  // Returns the count for how many buckets still exist for the client to make
  // sure there are no buckets that aren't accounted for during testing.
  int BucketDataCount(QuotaClientType quota_client);

  std::map<const blink::StorageKey, int> write_error_tracker() const {
    return write_error_tracker_;
  }

 protected:
  ~MockQuotaManager() override;

 private:
  friend class MockQuotaManagerProxy;

  // Contains the essential bits of information about a bucket that the
  // MockQuotaManager needs to understand for time-based deletion:
  // the bucket itself, the StorageType, its modification time and its
  // QuotaClients.
  struct BucketData {
    BucketData(const BucketInfo& bucket,
               QuotaClientTypes quota_clients,
               base::Time modified);
    ~BucketData();

    BucketData(const BucketData&) = delete;
    BucketData& operator=(const BucketData&) = delete;

    BucketData(BucketData&&);
    BucketData& operator=(BucketData&&);

    BucketInfo bucket;
    QuotaClientTypes quota_client_types;
    base::Time modified;
  };

  // Contains the essential information for each storage key for usage/quota
  // testing. (Ideally this should probably merged into the above struct, but
  // for regular usage/quota testing we hardly need modified time but only want
  // to keep usage and quota information, so this struct exists.
  struct StorageInfo {
    StorageInfo();
    ~StorageInfo();
    int64_t usage;
    int64_t quota;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
  };

  QuotaErrorOr<BucketInfo> FindBucket(const blink::StorageKey& storage_key,
                                      const std::string& bucket_name,
                                      blink::mojom::StorageType type);

  // This must be called via MockQuotaManagerProxy.
  void UpdateUsage(const blink::StorageKey& storage_key,
                   blink::mojom::StorageType type,
                   int64_t delta);

  void DidGetBucket(base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
                    QuotaErrorOr<BucketInfo> result);
  void DidGetModifiedInTimeRange(
      GetBucketsCallback callback,
      std::unique_ptr<std::set<BucketLocator>> buckets,
      blink::mojom::StorageType storage_type);
  void DidDeleteStorageKeyData(StatusCallback callback,
                               blink::mojom::QuotaStatusCode status);

  BucketId::Generator bucket_id_generator_;

  // The list of stored buckets that have been added via AddBucket.
  std::vector<BucketData> buckets_;
  std::map<std::pair<blink::StorageKey, blink::mojom::StorageType>, StorageInfo>
      usage_and_quota_map_;

  // Tracks number of times NotifyFailedWrite has been called per storage key.
  std::map<const blink::StorageKey, int> write_error_tracker_;

  base::WeakPtrFactory<MockQuotaManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManager);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_
