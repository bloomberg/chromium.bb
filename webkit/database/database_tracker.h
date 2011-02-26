// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASE_TRACKER_H_
#define WEBKIT_DATABASE_DATABASE_TRACKER_H_

#include <map>
#include <set>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "webkit/database/database_connections.h"

namespace sql {
class Connection;
class MetaTable;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace webkit_database {

extern const FilePath::CharType kDatabaseDirectoryName[];
extern const FilePath::CharType kTrackerDatabaseFileName[];

class DatabasesTable;
class QuotaTable;

// This class is used to store information about all databases in an origin.
class OriginInfo {
 public:
  OriginInfo(const OriginInfo& origin_info);
  ~OriginInfo();

  const string16& GetOrigin() const { return origin_; }
  int64 TotalSize() const { return total_size_; }
  int64 Quota() const { return quota_; }
  void GetAllDatabaseNames(std::vector<string16>* databases) const;
  int64 GetDatabaseSize(const string16& database_name) const;
  string16 GetDatabaseDescription(const string16& database_name) const;

 protected:
  typedef std::map<string16, std::pair<int64, string16> > DatabaseInfoMap;

  OriginInfo(const string16& origin, int64 total_size, int64 quota);

  string16 origin_;
  int64 total_size_;
  int64 quota_;
  DatabaseInfoMap database_info_;
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
    virtual void OnDatabaseScheduledForDeletion(
        const string16& origin_identifier,
        const string16& database_name) = 0;
    virtual ~Observer() {}
  };

  DatabaseTracker(const FilePath& profile_path, bool is_incognito,
                  quota::SpecialStoragePolicy* special_storage_policy);

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
                             const string16& database_name);

  bool GetAllOriginsInfo(std::vector<OriginInfo>* origins_info);
  void SetOriginQuota(const string16& origin_identifier, int64 new_quota);

  int64 GetDefaultQuota() { return default_quota_; }
  // Sets the default quota for all origins. Should be used in tests only.
  void SetDefaultQuota(int64 quota);

  bool IsDatabaseScheduledForDeletion(const string16& origin_identifier,
                                      const string16& database_name);

  // Deletes a single database. Returns net::OK on success, net::FAILED on
  // failure, or net::ERR_IO_PENDING and |callback| is invoked upon completion,
  // if non-NULL.
  int DeleteDatabase(const string16& origin_identifier,
                     const string16& database_name,
                     net::CompletionCallback* callback);

  // Delete any databases that have been touched since the cutoff date that's
  // supplied, omitting any that match IDs within |protected_origins|.
  // Returns net::OK on success, net::FAILED if not all databases could be
  // deleted, and net::ERR_IO_PENDING and |callback| is invoked upon completion,
  // if non-NULL. Protected origins, according the the SpecialStoragePolicy,
  // are not deleted by this method.
  int DeleteDataModifiedSince(const base::Time& cutoff,
                              net::CompletionCallback* callback);

  // Delete all databases that belong to the given origin. Returns net::OK on
  // success, net::FAILED if not all databases could be deleted, and
  // net::ERR_IO_PENDING and |callback| is invoked upon completion, if non-NULL.
  int DeleteDataForOrigin(const string16& origin_identifier,
                          net::CompletionCallback* callback);

  bool IsIncognitoProfile() const { return is_incognito_; }

  void GetIncognitoFileHandle(const string16& vfs_file_path,
                              base::PlatformFile* file_handle) const;
  void SaveIncognitoFileHandle(const string16& vfs_file_path,
                               const base::PlatformFile& file_handle);
  bool CloseIncognitoFileHandle(const string16& vfs_file_path);
  bool HasSavedIncognitoFileHandle(const string16& vfs_file_path) const;

  // Deletes the directory that stores all DBs in incognito mode, if it exists.
  void DeleteIncognitoDBDirectory();

  static void ClearLocalState(const FilePath& profile_path);

 private:
  // Need this here to allow RefCountedThreadSafe to call ~DatabaseTracker().
  friend class base::RefCountedThreadSafe<DatabaseTracker>;

  typedef std::map<string16, std::set<string16> > DatabaseSet;
  typedef std::map<net::CompletionCallback*, DatabaseSet> PendingCompletionMap;
  typedef std::map<string16, base::PlatformFile> FileHandlesMap;
  typedef std::map<string16, string16> OriginDirectoriesMap;

  class CachedOriginInfo : public OriginInfo {
   public:
    CachedOriginInfo() : OriginInfo(string16(), 0, 0) {}
    void SetOrigin(const string16& origin) { origin_ = origin; }
    void SetQuota(int64 new_quota) { quota_ = new_quota; }
    void SetDatabaseSize(const string16& database_name, int64 new_size) {
      int64 old_size = 0;
      if (database_info_.find(database_name) != database_info_.end())
        old_size = database_info_[database_name].first;
      database_info_[database_name].first = new_size;
      if (new_size != old_size)
        total_size_ += new_size - old_size;
    }
    void SetDatabaseDescription(const string16& database_name,
                                const string16& description) {
      database_info_[database_name].second = description;
    }
  };

  ~DatabaseTracker();

  bool DeleteClosedDatabase(const string16& origin_identifier,
                            const string16& database_name);
  bool DeleteOrigin(const string16& origin_identifier);
  void DeleteDatabaseIfNeeded(const string16& origin_identifier,
                              const string16& database_name);

  bool LazyInit();
  bool UpgradeToCurrentVersion();
  void InsertOrUpdateDatabaseDetails(const string16& origin_identifier,
                                     const string16& database_name,
                                     const string16& database_details,
                                     int64 estimated_size);

  void ClearAllCachedOriginInfo();
  CachedOriginInfo* GetCachedOriginInfo(const string16& origin_identifier);

  int64 GetDBFileSize(const string16& origin_identifier,
                      const string16& database_name);

  int64 GetOriginSpaceAvailable(const string16& origin_identifier);

  int64 UpdateCachedDatabaseFileSize(const string16& origin_identifier,
                                     const string16& database_name);
  void ScheduleDatabaseForDeletion(const string16& origin_identifier,
                                   const string16& database_name);
  // Schedule a set of open databases for deletion. If non-null, callback is
  // invoked upon completion.
  void ScheduleDatabasesForDeletion(const DatabaseSet& databases,
                                    net::CompletionCallback* callback);

  // Returns the directory where all DB files for the given origin are stored.
  string16 GetOriginDirectory(const string16& origin_identifier);

  bool is_initialized_;
  const bool is_incognito_;
  bool shutting_down_;
  const FilePath profile_path_;
  const FilePath db_dir_;
  scoped_ptr<sql::Connection> db_;
  scoped_ptr<DatabasesTable> databases_table_;
  scoped_ptr<QuotaTable> quota_table_;
  scoped_ptr<sql::MetaTable> meta_table_;
  ObserverList<Observer, true> observers_;
  std::map<string16, CachedOriginInfo> origins_info_map_;
  DatabaseConnections database_connections_;

  // The set of databases that should be deleted but are still opened
  DatabaseSet dbs_to_be_deleted_;
  PendingCompletionMap deletion_callbacks_;

  // Default quota for all origins; changed only by tests
  int64 default_quota_;

  // Apps and Extensions can have special rights.
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;

  // When in incognito mode, store a DELETE_ON_CLOSE handle to each
  // main DB and journal file that was accessed. When the incognito profile
  // goes away (or when the browser crashes), all these handles will be
  // closed, and the files will be deleted.
  FileHandlesMap incognito_file_handles_;

  // In a non-incognito profile, all DBs in an origin are stored in a directory
  // named after the origin. In an incognito profile though, we do not want the
  // directory structure to reveal the origins visited by the user (in case the
  // browser process crashes and those directories are not deleted). So we use
  // this map to assign directory names that do not reveal this information.
  OriginDirectoriesMap incognito_origin_directories_;
  int incognito_origin_directories_generator_;

  FRIEND_TEST_ALL_PREFIXES(DatabaseTracker, TestHelper);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_TRACKER_H_
