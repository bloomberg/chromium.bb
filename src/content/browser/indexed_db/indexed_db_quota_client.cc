// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "storage/browser/database/database_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;
using ::storage::DatabaseUtil;
using ::storage::mojom::QuotaClient;

namespace content {

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl& indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBQuotaClient::GetBucketUsage(const storage::BucketLocator& bucket,
                                          GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, StorageType::kTemporary);

  // Skip non-default buckets until Storage Buckets are supported for IndexedDB.
  // TODO(crbug.com/1218100): Integrate IndexedDB with StorageBuckets.
  if (!bucket.is_default) {
    std::move(callback).Run(0);
    return;
  }

  std::move(callback).Run(
      indexed_db_context_.GetStorageKeyDiskUsage(bucket.storage_key));
}

void IndexedDBQuotaClient::GetStorageKeysForType(
    StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  std::vector<StorageKey> storage_keys =
      indexed_db_context_.GetAllStorageKeys();
  std::move(callback).Run(std::move(storage_keys));
}

void IndexedDBQuotaClient::DeleteBucketData(
    const storage::BucketLocator& bucket,
    DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  // Skip non-default buckets until Storage Buckets are supported for IndexedDB.
  // TODO(crbug.com/1218100): Integrate IndexedDB with StorageBuckets.
  if (!bucket.is_default) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  indexed_db_context_.DeleteForStorageKey(
      bucket.storage_key,
      base::BindOnce(
          [](DeleteBucketDataCallback callback, bool success) {
            blink::mojom::QuotaStatusCode status =
                success ? blink::mojom::QuotaStatusCode::kOk
                        : blink::mojom::QuotaStatusCode::kUnknown;
            std::move(callback).Run(status);
          },
          std::move(callback)));
}

void IndexedDBQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  std::move(callback).Run();
}

}  // namespace content
