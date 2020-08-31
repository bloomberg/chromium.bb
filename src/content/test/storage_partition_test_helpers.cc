// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/storage_partition_test_helpers.h"

#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/browser/worker_host/test_shared_worker_service_impl.h"

namespace content {

void InjectTestSharedWorkerService(StoragePartition* storage_partition) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(storage_partition);

  storage_partition_impl->OverrideSharedWorkerServiceForTesting(
      std::make_unique<TestSharedWorkerServiceImpl>(
          storage_partition_impl,
          storage_partition_impl->GetServiceWorkerContext(),
          storage_partition_impl->GetAppCacheService()));
}

void TerminateAllSharedWorkers(StoragePartition* storage_partition,
                               base::OnceClosure callback) {
  static_cast<TestSharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService())
      ->TerminateAllWorkers(std::move(callback));
}

}  // namespace content
