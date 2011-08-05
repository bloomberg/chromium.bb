// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_MOCK_STORAGE_CLIENT_H_
#define WEBKIT_QUOTA_MOCK_STORAGE_CLIENT_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/task.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_client.h"

namespace quota {

class QuotaManagerProxy;

struct MockOriginData {
  const char* origin;
  StorageType type;
  int64 usage;
};

// Mock storage class for testing.
class MockStorageClient : public QuotaClient {
 public:
  MockStorageClient(QuotaManagerProxy* quota_manager_proxy,
                    const MockOriginData* mock_data, size_t mock_data_size);
  virtual ~MockStorageClient();

  // To add or modify mock data in this client.
  void AddOriginAndNotify(
      const GURL& origin_url, StorageType type, int64 size);
  void ModifyOriginAndNotify(
      const GURL& origin_url, StorageType type, int64 delta);
  void TouchAllOriginsAndNotify();

  void AddOriginToErrorSet(const GURL& origin_url, StorageType type);

  base::Time IncrementMockTime();

  // QuotaClient methods.
  virtual QuotaClient::ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              StorageType type,
                              GetUsageCallback* callback) OVERRIDE;
  virtual void GetOriginsForType(StorageType type,
                                 GetOriginsCallback* callback) OVERRIDE;
  virtual void GetOriginsForHost(StorageType type, const std::string& host,
                                 GetOriginsCallback* callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                DeletionCallback* callback) OVERRIDE;

 private:
  void RunGetOriginUsage(const GURL& origin_url,
                         StorageType type,
                         GetUsageCallback* callback);
  void RunGetOriginsForType(StorageType type,
                            GetOriginsCallback* callback);
  void RunGetOriginsForHost(StorageType type,
                            const std::string& host,
                            GetOriginsCallback* callback);
  void RunDeleteOriginData(const GURL& origin_url,
                           StorageType type,
                           DeletionCallback* callback);

  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const ID id_;

  typedef std::map<std::pair<GURL, StorageType>, int64> OriginDataMap;
  OriginDataMap origin_data_;
  typedef std::set<std::pair<GURL, StorageType> > ErrorOriginSet;
  ErrorOriginSet error_origins_;

  std::set<GetUsageCallback*> usage_callbacks_;
  std::set<GetOriginsCallback*> origins_callbacks_;
  std::set<DeletionCallback*> deletion_callbacks_;

  int mock_time_counter_;

  ScopedRunnableMethodFactory<MockStorageClient> runnable_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockStorageClient);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_MOCK_STORAGE_CLIENT_H_
