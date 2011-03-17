// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_DATABASE_H_
#define WEBKIT_QUOTA_QUOTA_DATABASE_H_

#include <map>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/time.h"

namespace sql {
class Connection;
class MetaTable;
class Statement;
}

class GURL;

namespace quota {

// TODO(kinuko): put this in a separated header file when we have more modules.
enum StorageType {
  kStorageTypeUnknown,
  kStorageTypeTemporary,
  kStorageTypePersistent,
};

// All the methods of this class must run on the DB thread.
class QuotaDatabase {
 public:
  explicit QuotaDatabase(const FilePath& path);
  ~QuotaDatabase();

  void CloseConnection();

  bool GetOriginQuota(const GURL& origin, StorageType type, int64* quota);
  bool SetOriginQuota(const GURL& origin, StorageType type, int64 quota);

  bool SetOriginLastAccessTime(const GURL& origin, StorageType type,
                               base::Time last_access_time);

  bool DeleteStorageInfo(const GURL& origin, StorageType type);

  bool GetGlobalQuota(StorageType type, int64* quota);
  bool SetGlobalQuota(StorageType type, int64 quota);

  // Return least recently used origins whose used_count is <=
  // |max_used_count| up to |num_origins_limit|.  If |max_used_count| is -1,
  // it just returns LRU storages regardless of the used_count value.
  // |num_origins_limit| must be > 0.
  bool GetLRUOrigins(StorageType type, std::vector<GURL>* origins,
                     int max_used_count, int num_origins_limit);

 private:
  struct StorageInfoRecord;

  bool FindOrigin(const GURL& origin_url, int64* origin_rowid);
  bool InsertOrigin(const GURL& origin_url, int64* origin_rowid);

  bool FindStorageInfo(int64 origin_rowid, StorageType type,
                       StorageInfoRecord* record);
  bool FindStorageInfo(const GURL& origin, StorageType type,
                       StorageInfoRecord* record);
  bool InsertStorageInfo(const StorageInfoRecord& record);

  bool LazyOpen(bool create_if_needed);
  bool EnsureDatabaseVersion();
  bool CreateSchema();
  bool ResetSchema();

  FilePath db_file_path_;

  scoped_ptr<sql::Connection> db_;
  scoped_ptr<sql::MetaTable> meta_table_;
  bool is_recreating_;
  bool is_disabled_;

  FRIEND_TEST_ALL_PREFIXES(QuotaDatabaseTest, LazyOpen);
  FRIEND_TEST_ALL_PREFIXES(QuotaDatabaseTest, OriginQuota);
  FRIEND_TEST_ALL_PREFIXES(QuotaDatabaseTest, GlobalQuota);
  FRIEND_TEST_ALL_PREFIXES(QuotaDatabaseTest, OriginLastAccessTimeLRU);

  DISALLOW_COPY_AND_ASSIGN(QuotaDatabase);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_DATABASE_H_
