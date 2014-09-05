// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/quota/quota_types.h"

namespace storage {

class DatabaseTracker;

// A QuotaClient implementation to integrate WebSQLDatabases
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class STORAGE_EXPORT_PRIVATE DatabaseQuotaClient
    : public storage::QuotaClient {
 public:
  DatabaseQuotaClient(
      base::MessageLoopProxy* tracker_thread,
      DatabaseTracker* tracker);
  virtual ~DatabaseQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              storage::StorageType type,
                              const GetUsageCallback& callback) OVERRIDE;
  virtual void GetOriginsForType(storage::StorageType type,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void GetOriginsForHost(storage::StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                storage::StorageType type,
                                const DeletionCallback& callback) OVERRIDE;
  virtual bool DoesSupport(storage::StorageType type) const OVERRIDE;

 private:
  scoped_refptr<base::MessageLoopProxy> db_tracker_thread_;
  scoped_refptr<DatabaseTracker> db_tracker_;  // only used on its thread

  DISALLOW_COPY_AND_ASSIGN(DatabaseQuotaClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
