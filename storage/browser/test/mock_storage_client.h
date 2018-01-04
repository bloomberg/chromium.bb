// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_STORAGE_CLIENT_H_
#define CONTENT_PUBLIC_TEST_MOCK_STORAGE_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "storage/browser/quota/quota_client.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"
#include "url/gurl.h"

namespace storage {
class QuotaManagerProxy;
}

using storage::QuotaClient;
using storage::QuotaManagerProxy;
using blink::mojom::StorageType;

namespace content {

struct MockOriginData {
  const char* origin;
  StorageType type;
  int64_t usage;
};

// Mock storage class for testing.
class MockStorageClient : public QuotaClient {
 public:
  MockStorageClient(QuotaManagerProxy* quota_manager_proxy,
                    const MockOriginData* mock_data,
                    QuotaClient::ID id,
                    size_t mock_data_size);
  ~MockStorageClient() override;

  // To add or modify mock data in this client.
  void AddOriginAndNotify(const GURL& origin_url,
                          StorageType type,
                          int64_t size);
  void ModifyOriginAndNotify(const GURL& origin_url,
                             StorageType type,
                             int64_t delta);
  void TouchAllOriginsAndNotify();

  void AddOriginToErrorSet(const GURL& origin_url, StorageType type);

  base::Time IncrementMockTime();

  // QuotaClient methods.
  QuotaClient::ID id() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const GURL& origin_url,
                      StorageType type,
                      const GetUsageCallback& callback) override;
  void GetOriginsForType(StorageType type,
                         const GetOriginsCallback& callback) override;
  void GetOriginsForHost(StorageType type,
                         const std::string& host,
                         const GetOriginsCallback& callback) override;
  void DeleteOriginData(const GURL& origin,
                        StorageType type,
                        const DeletionCallback& callback) override;
  bool DoesSupport(StorageType type) const override;

 private:
  void RunGetOriginUsage(const GURL& origin_url,
                         StorageType type,
                         const GetUsageCallback& callback);
  void RunGetOriginsForType(StorageType type,
                            const GetOriginsCallback& callback);
  void RunGetOriginsForHost(StorageType type,
                            const std::string& host,
                            const GetOriginsCallback& callback);
  void RunDeleteOriginData(const GURL& origin_url,
                           StorageType type,
                           const DeletionCallback& callback);

  void Populate(const MockOriginData* mock_data, size_t mock_data_size);

  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const ID id_;

  typedef std::map<std::pair<GURL, StorageType>, int64_t> OriginDataMap;
  OriginDataMap origin_data_;
  typedef std::set<std::pair<GURL, StorageType> > ErrorOriginSet;
  ErrorOriginSet error_origins_;

  int mock_time_counter_;

  base::WeakPtrFactory<MockStorageClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockStorageClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_STORAGE_CLIENT_H_
