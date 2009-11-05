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
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace sql {
class Connection;
class MetaTable;
}

namespace webkit_database {

class DatabasesTable;

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

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void CloseTrackerDatabaseAndClearCaches();

  const FilePath& DatabaseDirectory() const { return db_dir_; }
  FilePath GetFullDBFilePath(const string16& origin_identifier,
                             const string16& database_name) const;

 private:
  friend class base::RefCountedThreadSafe<DatabaseTracker>;

  ~DatabaseTracker();

  class CachedOriginInfo {
   public:
    CachedOriginInfo() : total_size_(0) { }
    int64 TotalSize() const { return total_size_; }
    int64 GetCachedDatabaseSize(const string16& database_name) {
      return cached_database_sizes_[database_name];
    }
    void SetCachedDatabaseSize(const string16& database_name, int64 new_size) {
      int64 old_size = cached_database_sizes_[database_name];
      cached_database_sizes_[database_name] = new_size;
      if (new_size != old_size)
        total_size_ += new_size - old_size;
    }

   private:
    int64 total_size_;
    std::map<string16, int64> cached_database_sizes_;
  };

  bool LazyInit();
  void InsertOrUpdateDatabaseDetails(const string16& origin_identifier,
                                     const string16& database_name,
                                     const string16& database_details,
                                     int64 estimated_size);

  void ClearAllCachedOriginInfo();
  CachedOriginInfo* GetCachedOriginInfo(const string16& origin_identifier);
  int64 GetCachedDatabaseFileSize(const string16& origin_identifier,
                                  const string16& database_name);
  int64 UpdateCachedDatabaseFileSize(const string16& origin_identifier,
                                     const string16& database_name);
  int64 GetDBFileSize(const string16& origin_identifier,
                      const string16& database_name) const;
  int64 GetOriginUsage(const string16& origin_identifer);
  int64 GetOriginQuota(const string16& origin_identifier) const;
  int64 GetOriginSpaceAvailable(const string16& origin_identifier);

  bool initialized_;
  const FilePath db_dir_;
  scoped_ptr<sql::Connection> db_;
  scoped_ptr<DatabasesTable> databases_table_;
  scoped_ptr<sql::MetaTable> meta_table_;
  ObserverList<Observer> observers_;
  std::map<string16, CachedOriginInfo> origins_info_map_;

  FRIEND_TEST(DatabaseTrackerTest, TestIt);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_TRACKER_H_
