// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASE_TRACKER_H_
#define WEBKIT_DATABASE_DATABASE_TRACKER_H_

#include <map>

#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/database/database_connections.h"

namespace sql {
class Connection;
class MetaTable;
}

namespace webkit_database {

class DatabasesTable;
class QuotaTable;

// This class is used to store information about all databases in an origin.
class OriginInfo {
 public:
  OriginInfo(const OriginInfo& origin_info)
      : origin_(origin_info.origin_),
        total_size_(origin_info.total_size_),
        quota_(origin_info.quota_),
        database_sizes_(origin_info.database_sizes_) {}
  string16 GetOrigin() const { return origin_; }
  int64 TotalSize() const { return total_size_; }
  int64 Quota() const { return quota_; }
  void GetAllDatabaseNames(std::vector<string16>* databases) const {
    for (std::map<string16, int64>::const_iterator it = database_sizes_.begin();
         it != database_sizes_.end(); it++) {
      databases->push_back(it->first);
    }
  }
  int64 GetDatabaseSize(const string16& database_name) const {
    std::map<string16, int64>::const_iterator it =
        database_sizes_.find(database_name);
    if (it != database_sizes_.end())
      return it->second;
    return 0;
  }

 protected:
  OriginInfo(const string16& origin, int64 total_size, int64 quota)
      : origin_(origin), total_size_(total_size), quota_(quota) { }

  string16 origin_;
  int64 total_size_;
  int64 quota_;
  std::map<string16, int64> database_sizes_;
};

// This class manages the main database, and keeps track of per origin quotas.
//
// The data in this class is not thread-safe, so all methods of this class
// should be called on the same thread. The only exception is
// database_directory() which returns a constant that is initialized when
// the DatabaseTracker instance is created.
//
// Furthermore, some methods of this class have to read/write data from/to
// the disk. Therefore, in a multi-threaded application, all methods of this
// class should be called on the thread dedicated to file operations (file
// thread in the browser process, for example), if such a thread exists.
class DatabaseTracker
    : public base::RefCountedThreadSafe<DatabaseTracker> {
 public:
  class Observer {
   public:
    virtual void OnDatabaseSizeChanged(const string16& origin_identifier,
                                       const string16& database_name,
                                       int64 database_size,
                                       int64 space_available) = 0;
    virtual ~Observer() {}
  };

  explicit DatabaseTracker(const FilePath& profile_path);

  void DatabaseOpened(const string16& origin_identifier,
                      const string16& database_name,
                      const string16& database_details,
                      int64 estimated_size,
                      int64* database_size,
                      int64* space_available);
  void DatabaseModified(const string16& origin_identifier,
                        const string16& database_name);
  void DatabaseClosed(const string16& origin_identifier,
                      const string16& database_name);
  void CloseDatabases(const DatabaseConnections& connections);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void CloseTrackerDatabaseAndClearCaches();

  const FilePath& DatabaseDirectory() const { return db_dir_; }
  FilePath GetFullDBFilePath(const string16& origin_identifier,
                             const string16& database_name) const;

  bool GetAllOriginsInfo(std::vector<OriginInfo>* origins_info);
  void SetOriginQuota(const string16& origin_identifier, int64 new_quota);
  bool DeleteDatabase(const string16& origin_identifier,
                      const string16& database_name);
  bool DeleteOrigin(const string16& origin_identifier);

 private:
  // Need this here to allow RefCountedThreadSafe to call ~DatabaseTracker().
  friend class base::RefCountedThreadSafe<DatabaseTracker>;

  class CachedOriginInfo : public OriginInfo {
   public:
    CachedOriginInfo() : OriginInfo(EmptyString16(), 0, 0) {}
    void SetOrigin(const string16& origin) { origin_ = origin; }
    void SetQuota(int64 new_quota) { quota_ = new_quota; }
    void SetDatabaseSize(const string16& database_name, int64 new_size) {
      int64 old_size = database_sizes_[database_name];
      database_sizes_[database_name] = new_size;
      if (new_size != old_size)
        total_size_ += new_size - old_size;
    }
  };

  ~DatabaseTracker();

  bool LazyInit();
  bool UpgradeToCurrentVersion();
  void InsertOrUpdateDatabaseDetails(const string16& origin_identifier,
                                     const string16& database_name,
                                     const string16& database_details,
                                     int64 estimated_size);

  void ClearAllCachedOriginInfo();
  CachedOriginInfo* GetCachedOriginInfo(const string16& origin_identifier);

  int64 GetDBFileSize(const string16& origin_identifier,
                      const string16& database_name) const;

  int64 GetOriginSpaceAvailable(const string16& origin_identifier);

  int64 UpdateCachedDatabaseFileSize(const string16& origin_identifier,
                                     const string16& database_name);

  bool initialized_;
  const FilePath db_dir_;
  scoped_ptr<sql::Connection> db_;
  scoped_ptr<DatabasesTable> databases_table_;
  scoped_ptr<QuotaTable> quota_table_;
  scoped_ptr<sql::MetaTable> meta_table_;
  ObserverList<Observer> observers_;
  std::map<string16, CachedOriginInfo> origins_info_map_;
  DatabaseConnections database_connections_;

  FRIEND_TEST(DatabaseTrackerTest, TestIt);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_TRACKER_H_
