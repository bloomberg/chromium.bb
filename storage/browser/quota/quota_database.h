// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "storage/browser/storage_browser_export.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"
#include "url/gurl.h"

namespace content {
class QuotaDatabaseTest;
}

namespace sql {
class Connection;
class MetaTable;
}

class GURL;

namespace storage {

class SpecialStoragePolicy;

// All the methods of this class must run on the DB thread.
class STORAGE_EXPORT QuotaDatabase {
 public:
  struct STORAGE_EXPORT OriginInfoTableEntry {
    OriginInfoTableEntry();
    OriginInfoTableEntry(const GURL& origin,
                         blink::mojom::StorageType type,
                         int used_count,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time);
    GURL origin;
    blink::mojom::StorageType type;
    int used_count;
    base::Time last_access_time;
    base::Time last_modified_time;
  };

  // Constants for {Get,Set}QuotaConfigValue keys.
  static const char kDesiredAvailableSpaceKey[];
  static const char kTemporaryQuotaOverrideKey[];

  // If 'path' is empty, an in memory database will be used.
  explicit QuotaDatabase(const base::FilePath& path);
  ~QuotaDatabase();

  void CloseConnection();

  // Returns whether the record could be found.
  bool GetHostQuota(const std::string& host,
                    blink::mojom::StorageType type,
                    int64_t* quota);

  // Returns whether the operation succeeded.
  bool SetHostQuota(const std::string& host,
                    blink::mojom::StorageType type,
                    int64_t quota);
  bool DeleteHostQuota(const std::string& host, blink::mojom::StorageType type);

  bool SetOriginLastAccessTime(const GURL& origin,
                               blink::mojom::StorageType type,
                               base::Time last_access_time);

  bool SetOriginLastModifiedTime(const GURL& origin,
                                 blink::mojom::StorageType type,
                                 base::Time last_modified_time);

  // Gets the time |origin| was last evicted. Returns whether the record could
  // be found.
  bool GetOriginLastEvictionTime(const GURL& origin,
                                 blink::mojom::StorageType type,
                                 base::Time* last_eviction_time);

  // Sets the time the origin was last evicted. Returns whether the operation
  // succeeded.
  bool SetOriginLastEvictionTime(const GURL& origin,
                                 blink::mojom::StorageType type,
                                 base::Time last_eviction_time);
  bool DeleteOriginLastEvictionTime(const GURL& origin,
                                    blink::mojom::StorageType type);

  // Register initial |origins| info |type| to the database.
  // This method is assumed to be called only after the installation or
  // the database schema reset.
  bool RegisterInitialOriginInfo(const std::set<GURL>& origins,
                                 blink::mojom::StorageType type);

  // Gets the OriginInfoTableEntry for |origin|. Returns whether the record
  // could be found.
  bool GetOriginInfo(const GURL& origin,
                     blink::mojom::StorageType type,
                     OriginInfoTableEntry* entry);

  bool DeleteOriginInfo(const GURL& origin, blink::mojom::StorageType type);

  bool GetQuotaConfigValue(const char* key, int64_t* value);
  bool SetQuotaConfigValue(const char* key, int64_t value);

  // Sets |origin| to the least recently used origin of origins not included
  // in |exceptions| and not granted the special unlimited storage right.
  // It returns false when it failed in accessing the database.
  // |origin| is set to empty when there is no matching origin.
  bool GetLRUOrigin(blink::mojom::StorageType type,
                    const std::set<GURL>& exceptions,
                    SpecialStoragePolicy* special_storage_policy,
                    GURL* origin);

  // Populates |origins| with the ones that have been modified since
  // the |modified_since|. Returns whether the operation succeeded.
  bool GetOriginsModifiedSince(blink::mojom::StorageType type,
                               std::set<GURL>* origins,
                               base::Time modified_since);

  // Returns false if SetOriginDatabaseBootstrapped has never
  // been called before, which means existing origins may not have been
  // registered.
  bool IsOriginDatabaseBootstrapped();
  bool SetOriginDatabaseBootstrapped(bool bootstrap_flag);

 private:
  struct STORAGE_EXPORT QuotaTableEntry {
    QuotaTableEntry();
    QuotaTableEntry(const std::string& host,
                    blink::mojom::StorageType type,
                    int64_t quota);
    std::string host;
    blink::mojom::StorageType type;
    int64_t quota;
  };
  friend STORAGE_EXPORT bool operator <(
      const QuotaTableEntry& lhs, const QuotaTableEntry& rhs);
  friend STORAGE_EXPORT bool operator <(
      const OriginInfoTableEntry& lhs, const OriginInfoTableEntry& rhs);

  // Structures used for CreateSchema.
  struct TableSchema {
    const char* table_name;
    const char* columns;
  };
  struct IndexSchema {
    const char* index_name;
    const char* table_name;
    const char* columns;
    bool unique;
  };

  typedef base::Callback<bool (const QuotaTableEntry&)> QuotaTableCallback;
  typedef base::Callback<bool (const OriginInfoTableEntry&)>
      OriginInfoTableCallback;

  struct QuotaTableImporter;

  // For long-running transactions support.  We always keep a transaction open
  // so that multiple transactions can be batched.  They are flushed
  // with a delay after a modification has been made.  We support neither
  // nested transactions nor rollback (as we don't need them for now).
  void Commit();
  void ScheduleCommit();

  bool LazyOpen(bool create_if_needed);
  bool EnsureDatabaseVersion();
  bool ResetSchema();
  bool UpgradeSchema(int current_version);
  bool InsertOrReplaceHostQuota(const std::string& host,
                                blink::mojom::StorageType type,
                                int64_t quota);

  static bool CreateSchema(
      sql::Connection* database,
      sql::MetaTable* meta_table,
      int schema_version, int compatible_version,
      const TableSchema* tables, size_t tables_size,
      const IndexSchema* indexes, size_t indexes_size);

  // |callback| may return false to stop reading data.
  bool DumpQuotaTable(const QuotaTableCallback& callback);
  bool DumpOriginInfoTable(const OriginInfoTableCallback& callback);

  base::FilePath db_file_path_;

  std::unique_ptr<sql::Connection> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  bool is_recreating_;
  bool is_disabled_;

  base::OneShotTimer timer_;

  friend class content::QuotaDatabaseTest;
  friend class QuotaManager;

  static const TableSchema kTables[];
  static const IndexSchema kIndexes[];

  DISALLOW_COPY_AND_ASSIGN(QuotaDatabase);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
