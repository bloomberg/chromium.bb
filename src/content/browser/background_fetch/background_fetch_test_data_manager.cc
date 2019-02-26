// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_data_manager.h"

#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"

namespace content {

namespace {

class MockBGFQuotaManagerProxy : public MockQuotaManagerProxy {
 public:
  MockBGFQuotaManagerProxy(MockQuotaManager* quota_manager)
      : MockQuotaManagerProxy(quota_manager,
                              base::ThreadTaskRunnerHandle::Get().get()) {}

  // Ignore quota client, it is irrelevant for these tests.
  void RegisterClient(QuotaClient* client) override {
    delete client;  // Directly delete, to avoid memory leak.
  }

  void GetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                        const url::Origin& origin,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override {
    DCHECK(original_task_runner);
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0 /* usage */,
                            kBackgroundFetchMaxQuotaBytes);
  }

 protected:
  ~MockBGFQuotaManagerProxy() override = default;
};

}  // namespace

BackgroundFetchTestDataManager::BackgroundFetchTestDataManager(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    bool mock_fill_response)
    : BackgroundFetchDataManager(browser_context,
                                 service_worker_context,
                                 nullptr /* cache_storage_context */,
                                 nullptr /* quota_manager_proxy */),
      browser_context_(browser_context),
      storage_partition_(storage_partition),
      mock_fill_response_(mock_fill_response) {}

void BackgroundFetchTestDataManager::InitializeOnIOThread() {
  blob_storage_context_ = ChromeBlobStorageContext::GetFor(browser_context_);
  // Wait for ChromeBlobStorageContext to finish initializing.
  base::RunLoop().RunUntilIdle();

  mock_quota_manager_ = base::MakeRefCounted<MockQuotaManager>(
      storage_partition_->GetPath().empty(), storage_partition_->GetPath(),
      base::ThreadTaskRunnerHandle::Get().get(),
      base::MakeRefCounted<MockSpecialStoragePolicy>());

  quota_manager_proxy_ =
      base::MakeRefCounted<MockBGFQuotaManagerProxy>(mock_quota_manager_.get());

  cache_manager_ = CacheStorageManager::Create(
      storage_partition_->GetPath(), base::ThreadTaskRunnerHandle::Get(),
      quota_manager_proxy_);
  DCHECK(cache_manager_);

  cache_manager_->SetBlobParametersForCache(
      blob_storage_context_->context()->AsWeakPtr());
}

BackgroundFetchTestDataManager::~BackgroundFetchTestDataManager() = default;

}  // namespace content
