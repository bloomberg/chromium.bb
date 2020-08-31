// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

class QuotaManagerProxy;

struct MockOriginData {
  const char* origin;
  blink::mojom::StorageType type;
  int64_t usage;
};

// Mock QuotaClient implementation for testing.
class MockQuotaClient : public QuotaClient {
 public:
  MockQuotaClient(scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
                  base::span<const MockOriginData> mock_data,
                  QuotaClientType client_type);

  // To add or modify mock data in this client.
  void AddOriginAndNotify(const url::Origin& origin,
                          blink::mojom::StorageType type,
                          int64_t size);
  void ModifyOriginAndNotify(const url::Origin& origin,
                             blink::mojom::StorageType type,
                             int64_t delta);
  void TouchAllOriginsAndNotify();

  void AddOriginToErrorSet(const url::Origin& origin,
                           blink::mojom::StorageType type);

  base::Time IncrementMockTime();

  // QuotaClient.
  QuotaClientType type() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeletionCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             base::OnceClosure callback) override;
  bool DoesSupport(blink::mojom::StorageType type) const override;

 private:
  ~MockQuotaClient() override;

  void RunGetOriginUsage(const url::Origin& origin,
                         blink::mojom::StorageType type,
                         GetUsageCallback callback);
  void RunGetOriginsForType(blink::mojom::StorageType type,
                            GetOriginsCallback callback);
  void RunGetOriginsForHost(blink::mojom::StorageType type,
                            const std::string& host,
                            GetOriginsCallback callback);
  void RunDeleteOriginData(const url::Origin& origin,
                           blink::mojom::StorageType type,
                           DeletionCallback callback);

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const QuotaClientType client_type_;

  std::map<std::pair<url::Origin, blink::mojom::StorageType>, int64_t>
      origin_data_;
  std::set<std::pair<url::Origin, blink::mojom::StorageType>> error_origins_;

  int mock_time_counter_;

  base::WeakPtrFactory<MockQuotaClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_CLIENT_H_
