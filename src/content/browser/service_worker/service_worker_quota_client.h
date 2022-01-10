// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/cpp/storage_key_quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class ServiceWorkerContextCore;

class ServiceWorkerQuotaClient : public storage::StorageKeyQuotaClient {
 public:
  // `context` must outlive this instance. This is true because `context` owns
  // this instance.
  explicit ServiceWorkerQuotaClient(ServiceWorkerContextCore& context);

  ServiceWorkerQuotaClient(const ServiceWorkerQuotaClient&) = delete;
  ServiceWorkerQuotaClient& operator=(const ServiceWorkerQuotaClient&) = delete;

  ~ServiceWorkerQuotaClient() override;

  // Called when an error causes the ServiceWorkerContextCore to be rebuilt.
  void ResetContext(ServiceWorkerContextCore& new_context) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    context_ = &new_context;
  }

  // storage::StorageKeyQuotaClient override methods.
  void GetStorageKeyUsage(const blink::StorageKey& storage_key,
                          blink::mojom::StorageType type,
                          GetStorageKeyUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void GetStorageKeysForHost(blink::mojom::StorageType type,
                             const std::string& host,
                             GetStorageKeysForHostCallback callback) override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            DeleteStorageKeyDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  friend class ServiceWorkerContextWrapper;
  friend class ServiceWorkerQuotaClientTest;

  SEQUENCE_CHECKER(sequence_checker_);

  // The raw pointer is safe because `context_` owns this instance.
  //
  // The pointer is guaranteed to be non-null. It is not a reference because
  // ResetContext() changes the object it points to.
  raw_ptr<ServiceWorkerContextCore> context_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
