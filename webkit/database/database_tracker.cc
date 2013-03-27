// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_tracker.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"
#include "webkit/database/database_quota_client.h"
#include "webkit/database/database_util.h"
#include "webkit/database/databases_table.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

namespace webkit_database {

const base::FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases");
const base::FilePath::CharType kIncognitoDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases-incognito");
const base::FilePath::CharType kTrackerDatabaseFileName[] =
    FILE_PATH_LITERAL("Databases.db");
static const int kCurrentVersion = 2;
static const int kCompatibleVersion = 1;

const base::FilePath::CharType kTemporaryDirectoryPrefix[] =
    FILE_PATH_LITERAL("DeleteMe");
const base::FilePath::CharType kTemporaryDirectoryPattern[] =
    FILE_PATH_LITERAL("DeleteMe*");

OriginInfo::OriginInfo()
    : total_size_(0) {}

OriginInfo::OriginInfo(const OriginInfo& origin_info)
    : origin_(origin_info.origin_),
      total_size_(origin_info.total_size_),
      database_info_(origin_info.database_info_) {}

OriginInfo::~OriginInfo() {}

void OriginInfo::GetAllDatabaseNames(std::vector<string16>* databases) const {
  for (DatabaseInfoMap::const_iterator it = database_info_.begin();
       it != database_info_.end(); it++) {
    databases->push_back(it->first);
  }
}

int64 OriginInfo::GetDatabaseSize(const string16& database_name) const {
  DatabaseInfoMap::const_iterator it = database_info_.find(database_name);
  if (it != database_info_.end())
    return it->second.first;
  return 0;
}

string16 OriginInfo::GetDatabaseDescription(
    const string16& database_name) const {
  DatabaseInfoMap::const_iterator it = database_info_.find(database_name);
  if (it != database_info_.end())
    return it->second.second;
  return string16();
}

OriginInfo::OriginInfo(const string16& origin, int64 total_size)
    : origin_(origin), total_size_(total_size) {}

DatabaseTracker::DatabaseTracker(
    const base::FilePath& profile_path,
    bool is_incognito,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::MessageLoopProxy* db_tracker_thread)
    : is_initialized_(false),
      is_incognito_(is_incognito),
      force_keep_session_state_(false),
      shutting_down_(false),
      profile_path_(profile_path),
      db_dir_(is_incognito_ ?
              profile_path_.Append(kIncognitoDatabaseDirectoryName) :
              profile_path_.Append(kDatabaseDirectoryName)),
      db_(new sql::Connection()),
      databases_table_(NULL),
      meta_table_(NULL),
      special_storage_policy_(special_storage_policy),
      quota_manager_proxy_(quota_manager_proxy),
      db_tracker_thread_(db_tracker_thread),
      incognito_origin_directories_generator_(0) {
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(
        new DatabaseQuotaClient(db_tracker_thread, this));
  }
}

DatabaseTracker::~DatabaseTracker() {
  DCHECK(dbs_to_be_deleted_.empty());
  DCHECK(deletion_callbacks_.empty());
}

void DatabaseTracker::DatabaseOpened(const string16& origin_identifier,
                                     const string16& database_name,
                                     const string16& database_description,
                                     int64 estimated_size,
                                     int64* database_size) {
  if (shutting_down_ || !LazyInit()) {
    *database_size = 0;
    return;
  }

  if (quota_manager_proxy_)
    quota_manager_proxy_->NotifyStorageAccessed(
        quota::QuotaClient::kDatabase,
        DatabaseUtil::GetOriginFromIdentifier(origin_identifier),
        quota::kStorageTypeTemporary);

  InsertOrUpdateDatabaseDetails(origin_identifier, database_name,
                                database_description, estimated_size);
  if (database_connections_.AddConnection(origin_identifier, database_name)) {
    *database_size = SeedOpenDatabaseInfo(origin_identifier,
                                          database_name,
                                          database_description);
    return;
  }
  *database_size  = UpdateOpenDatabaseInfoAndNotify(origin_identifier,
                                                    database_name,
                                                    &database_description);
}

void DatabaseTracker::DatabaseModified(const string16& origin_identifier,
                                       const string16& database_name) {
  if (!LazyInit())
    return;
  UpdateOpenDatabaseSizeAndNotify(origin_identifier, database_name);
}

void DatabaseTracker::DatabaseClosed(const string16& origin_identifier,
                                     const string16& database_name) {
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_);
    return;
  }

  // We call NotifiyStorageAccessed when a db is opened and also when
  // closed because we don't call it for read while open.
  if (quota_manager_proxy_)
    quota_manager_proxy_->NotifyStorageAccessed(
        quota::QuotaClient::kDatabase,
        DatabaseUtil::GetOriginFromIdentifier(origin_identifier),
        quota::kStorageTypeTemporary);

  UpdateOpenDatabaseSizeAndNotify(origin_identifier, database_name);
  if (database_connections_.RemoveConnection(origin_identifier, database_name))
    DeleteDatabaseIfNeeded(origin_identifier, database_name);
}

void DatabaseTracker::HandleSqliteError(
    const string16& origin_identifier,
    const string16& database_name,
    int error) {
  // We only handle errors that indicate corruption and we
  // do so with a heavy hand, we delete it. Any renderers/workers
  // with this database open will receive a message to close it
  // immediately, once all have closed, the files will be deleted.
  // In the interim, all attempts to open a new connection to that
  // database will fail.
  // Note: the client-side filters out all but these two errors as
  // a small optimization, see WebDatabaseObserverImpl::HandleSqliteError.
  if (error == SQLITE_CORRUPT || error == SQLITE_NOTADB) {
    DeleteDatabase(origin_identifier, database_name,
                   net::CompletionCallback());
  }
}

void DatabaseTracker::CloseDatabases(const DatabaseConnections& connections) {
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_ || connections.IsEmpty());
    return;
  }

  // When being closed by this route, there's a chance that
  // the tracker missed some DatabseModified calls. This method is used
  // when a renderer crashes to cleanup its open resources.
  // We need to examine what we have in connections for the
  // size of each open databases and notify any differences between the
  // actual file sizes now.
  std::vector<std::pair<string16, string16> > open_dbs;
  connections.ListConnections(&open_dbs);
  for (std::vector<std::pair<string16, string16> >::iterator it =
           open_dbs.begin(); it != open_dbs.end(); ++it)
    UpdateOpenDatabaseSizeAndNotify(it->first, it->second);

  std::vector<std::pair<string16, string16> > closed_dbs;
  database_connections_.RemoveConnections(connections, &closed_dbs);
  for (std::vector<std::pair<string16, string16> >::iterator it =
           closed_dbs.begin(); it != closed_dbs.end(); ++it) {
    DeleteDatabaseIfNeeded(it->first, it->second);
  }
}

void DatabaseTracker::DeleteDatabaseIfNeeded(const string16& origin_identifier,
                                             const string16& database_name) {
  DCHECK(!database_connections_.IsDatabaseOpened(origin_identifier,
                                                 database_name));
  if (IsDatabaseScheduledForDeletion(origin_identifier, database_name)) {
    DeleteClosedDatabase(origin_identifier, database_name);
    dbs_to_be_deleted_[origin_identifier].erase(database_name);
    if (dbs_to_be_deleted_[origin_identifier].empty())
      dbs_to_be_deleted_.erase(origin_identifier);

    PendingDeletionCallbacks::iterator callback = deletion_callbacks_.begin();
    while (callback != deletion_callbacks_.end()) {
      DatabaseSet::iterator found_origin =
          callback->second.find(origin_identifier);
      if (found_origin != callback->second.end()) {
        std::set<string16>& databases = found_origin->second;
        databases.erase(database_name);
        if (databases.empty()) {
          callback->second.erase(found_origin);
          if (callback->second.empty()) {
            net::CompletionCallback cb = callback->first;
            cb.Run(net::OK);
            callback = deletion_callbacks_.erase(callback);
            continue;
          }
        }
      }

      ++callback;
    }
  }
}

void DatabaseTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DatabaseTracker::RemoveObserver(Observer* observer) {
  // When we remove a listener, we do not know which cached information
  // is still needed and which information can be discarded. So we just
  // clear all caches and re-populate them as needed.
  observers_.RemoveObserver(observer);
  ClearAllCachedOriginInfo();
}

void DatabaseTracker::CloseTrackerDatabaseAndClearCaches() {
  ClearAllCachedOriginInfo();

  if (!is_incognito_) {
    meta_table_.reset(NULL);
    databases_table_.reset(NULL);
    db_->Close();
    is_initialized_ = false;
  }
}

string16 DatabaseTracker::GetOriginDirectory(
    const string16& origin_identifier) {
  if (!is_incognito_)
    return origin_identifier;

  OriginDirectoriesMap::const_iterator it =
      incognito_origin_directories_.find(origin_identifier);
  if (it != incognito_origin_directories_.end())
    return it->second;

  string16 origin_directory =
      base::IntToString16(incognito_origin_directories_generator_++);
  incognito_origin_directories_[origin_identifier] = origin_directory;
  return origin_directory;
}

base::FilePath DatabaseTracker::GetFullDBFilePath(
    const string16& origin_identifier,
    const string16& database_name) {
  DCHECK(!origin_identifier.empty());
  if (!LazyInit())
    return base::FilePath();

  int64 id = databases_table_->GetDatabaseID(
      origin_identifier, database_name);
  if (id < 0)
    return base::FilePath();

  base::FilePath file_name = base::FilePath::FromWStringHack(
      UTF8ToWide(base::Int64ToString(id)));
  return db_dir_.Append(base::FilePath::FromWStringHack(
      UTF16ToWide(GetOriginDirectory(origin_identifier)))).Append(file_name);
}

bool DatabaseTracker::GetOriginInfo(const string16& origin_identifier,
                                    OriginInfo* info) {
  DCHECK(info);
  CachedOriginInfo* cached_info = GetCachedOriginInfo(origin_identifier);
  if (!cached_info)
    return false;
  *info = OriginInfo(*cached_info);
  return true;
}

bool DatabaseTracker::GetAllOriginIdentifiers(
    std::vector<string16>* origin_identifiers) {
  DCHECK(origin_identifiers);
  DCHECK(origin_identifiers->empty());
  if (!LazyInit())
    return false;
  return databases_table_->GetAllOrigins(origin_identifiers);
}

bool DatabaseTracker::GetAllOriginsInfo(std::vector<OriginInfo>* origins_info) {
  DCHECK(origins_info);
  DCHECK(origins_info->empty());

  std::vector<string16> origins;
  if (!GetAllOriginIdentifiers(&origins))
    return false;

  for (std::vector<string16>::const_iterator it = origins.begin();
       it != origins.end(); it++) {
    CachedOriginInfo* origin_info = GetCachedOriginInfo(*it);
    if (!origin_info) {
      // Restore 'origins_info' to its initial state.
      origins_info->clear();
      return false;
    }
    origins_info->push_back(OriginInfo(*origin_info));
  }

  return true;
}

bool DatabaseTracker::DeleteClosedDatabase(const string16& origin_identifier,
                                           const string16& database_name) {
  if (!LazyInit())
    return false;

  // Check if the database is opened by any renderer.
  if (database_connections_.IsDatabaseOpened(origin_identifier, database_name))
    return false;

  int64 db_file_size = quota_manager_proxy_ ?
      GetDBFileSize(origin_identifier, database_name) : 0;

  // Try to delete the file on the hard drive.
  base::FilePath db_file = GetFullDBFilePath(origin_identifier, database_name);
  if (file_util::PathExists(db_file) && !file_util::Delete(db_file, false))
    return false;

  // Also delete any orphaned journal file.
  DCHECK(db_file.Extension().empty());
  file_util::Delete(db_file.InsertBeforeExtensionASCII(
      DatabaseUtil::kJournalFileSuffix), false);

  if (quota_manager_proxy_ && db_file_size)
    quota_manager_proxy_->NotifyStorageModified(
        quota::QuotaClient::kDatabase,
        DatabaseUtil::GetOriginFromIdentifier(origin_identifier),
        quota::kStorageTypeTemporary,
        -db_file_size);

  // Clean up the main database and invalidate the cached record.
  databases_table_->DeleteDatabaseDetails(origin_identifier, database_name);
  origins_info_map_.erase(origin_identifier);

  std::vector<DatabaseDetails> details;
  if (databases_table_->GetAllDatabaseDetailsForOrigin(
          origin_identifier, &details) && details.empty()) {
    // Try to delete the origin in case this was the last database.
    DeleteOrigin(origin_identifier, false);
  }
  return true;
}

bool DatabaseTracker::DeleteOrigin(const string16& origin_identifier,
                                   bool force) {
  if (!LazyInit())
    return false;

  // Check if any database in this origin is opened by any renderer.
  if (database_connections_.IsOriginUsed(origin_identifier) && !force)
    return false;

  int64 deleted_size = 0;
  if (quota_manager_proxy_) {
    CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
    if (origin_info)
      deleted_size = origin_info->TotalSize();
  }

  origins_info_map_.erase(origin_identifier);
  base::FilePath origin_dir = db_dir_.Append(base::FilePath::FromWStringHack(
      UTF16ToWide(origin_identifier)));

  // Create a temporary directory to move possibly still existing databases to,
  // as we can't delete the origin directory on windows if it contains opened
  // files.
  base::FilePath new_origin_dir;
  file_util::CreateTemporaryDirInDir(db_dir_,
                                     kTemporaryDirectoryPrefix,
                                     &new_origin_dir);
  file_util::FileEnumerator databases(
      origin_dir,
      false,
      file_util::FileEnumerator::FILES);
  for (base::FilePath database = databases.Next(); !database.empty();
       database = databases.Next()) {
    base::FilePath new_file = new_origin_dir.Append(database.BaseName());
    file_util::Move(database, new_file);
  }
  file_util::Delete(origin_dir, true);
  file_util::Delete(new_origin_dir, true); // might fail on windows.

  databases_table_->DeleteOrigin(origin_identifier);

  if (quota_manager_proxy_ && deleted_size) {
    quota_manager_proxy_->NotifyStorageModified(
        quota::QuotaClient::kDatabase,
        DatabaseUtil::GetOriginFromIdentifier(origin_identifier),
        quota::kStorageTypeTemporary,
        -deleted_size);
  }

  return true;
}

bool DatabaseTracker::IsDatabaseScheduledForDeletion(
    const string16& origin_identifier,
    const string16& database_name) {
  DatabaseSet::iterator it = dbs_to_be_deleted_.find(origin_identifier);
  if (it == dbs_to_be_deleted_.end())
    return false;

  std::set<string16>& databases = it->second;
  return (databases.find(database_name) != databases.end());
}

bool DatabaseTracker::LazyInit() {
  if (!is_initialized_ && !shutting_down_) {
    DCHECK(!db_->is_open());
    DCHECK(!databases_table_.get());
    DCHECK(!meta_table_.get());

    // If there are left-over directories from failed deletion attempts, clean
    // them up.
    if (file_util::DirectoryExists(db_dir_)) {
      file_util::FileEnumerator directories(
          db_dir_,
          false,
          file_util::FileEnumerator::DIRECTORIES,
          kTemporaryDirectoryPattern);
      for (base::FilePath directory = directories.Next(); !directory.empty();
           directory = directories.Next()) {
        file_util::Delete(directory, true);
      }
    }

    // If the tracker database exists, but it's corrupt or doesn't
    // have a meta table, delete the database directory.
    const base::FilePath kTrackerDatabaseFullPath =
        db_dir_.Append(base::FilePath(kTrackerDatabaseFileName));
    if (file_util::DirectoryExists(db_dir_) &&
        file_util::PathExists(kTrackerDatabaseFullPath) &&
        (!db_->Open(kTrackerDatabaseFullPath) ||
         !sql::MetaTable::DoesTableExist(db_.get()))) {
      db_->Close();
      if (!file_util::Delete(db_dir_, true))
        return false;
    }

    db_->set_error_histogram_name("Sqlite.DatabaseTracker.Error");

    databases_table_.reset(new DatabasesTable(db_.get()));
    meta_table_.reset(new sql::MetaTable());

    is_initialized_ =
        file_util::CreateDirectory(db_dir_) &&
        (db_->is_open() ||
         (is_incognito_ ? db_->OpenInMemory() :
          db_->Open(kTrackerDatabaseFullPath))) &&
        UpgradeToCurrentVersion();
    if (!is_initialized_) {
      databases_table_.reset(NULL);
      meta_table_.reset(NULL);
      db_->Close();
    }
  }
  return is_initialized_;
}

bool DatabaseTracker::UpgradeToCurrentVersion() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin() ||
      !meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion) ||
      (meta_table_->GetCompatibleVersionNumber() > kCurrentVersion) ||
      !databases_table_->Init())
    return false;

  if (meta_table_->GetVersionNumber() < kCurrentVersion)
    meta_table_->SetVersionNumber(kCurrentVersion);

  return transaction.Commit();
}

void DatabaseTracker::InsertOrUpdateDatabaseDetails(
    const string16& origin_identifier,
    const string16& database_name,
    const string16& database_description,
    int64 estimated_size) {
  DatabaseDetails details;
  if (!databases_table_->GetDatabaseDetails(
          origin_identifier, database_name, &details)) {
    details.origin_identifier = origin_identifier;
    details.database_name = database_name;
    details.description = database_description;
    details.estimated_size = estimated_size;
    databases_table_->InsertDatabaseDetails(details);
  } else if ((details.description != database_description) ||
             (details.estimated_size != estimated_size)) {
    details.description = database_description;
    details.estimated_size = estimated_size;
    databases_table_->UpdateDatabaseDetails(details);
  }
}

void DatabaseTracker::ClearAllCachedOriginInfo() {
  origins_info_map_.clear();
}

DatabaseTracker::CachedOriginInfo* DatabaseTracker::MaybeGetCachedOriginInfo(
    const string16& origin_identifier, bool create_if_needed) {
  if (!LazyInit())
    return NULL;

  // Populate the cache with data for this origin if needed.
  if (origins_info_map_.find(origin_identifier) == origins_info_map_.end()) {
    if (!create_if_needed)
      return NULL;

    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOrigin(
            origin_identifier, &details)) {
      return NULL;
    }

    CachedOriginInfo& origin_info = origins_info_map_[origin_identifier];
    origin_info.SetOrigin(origin_identifier);
    for (std::vector<DatabaseDetails>::const_iterator it = details.begin();
         it != details.end(); it++) {
      int64 db_file_size;
      if (database_connections_.IsDatabaseOpened(
              origin_identifier, it->database_name)) {
        db_file_size = database_connections_.GetOpenDatabaseSize(
            origin_identifier, it->database_name);
      } else {
        db_file_size = GetDBFileSize(origin_identifier, it->database_name);
      }
      origin_info.SetDatabaseSize(it->database_name, db_file_size);
      origin_info.SetDatabaseDescription(it->database_name, it->description);
    }
  }

  return &origins_info_map_[origin_identifier];
}

int64 DatabaseTracker::GetDBFileSize(const string16& origin_identifier,
                                     const string16& database_name) {
  base::FilePath db_file_name = GetFullDBFilePath(origin_identifier,
                                                  database_name);
  int64 db_file_size = 0;
  if (!file_util::GetFileSize(db_file_name, &db_file_size))
    db_file_size = 0;
  return db_file_size;
}

int64 DatabaseTracker::SeedOpenDatabaseInfo(
    const string16& origin_id, const string16& name,
    const string16& description) {
  DCHECK(database_connections_.IsDatabaseOpened(origin_id, name));
  int64 size = GetDBFileSize(origin_id, name);
  database_connections_.SetOpenDatabaseSize(origin_id, name,  size);
  CachedOriginInfo* info = MaybeGetCachedOriginInfo(origin_id, false);
  if (info) {
    info->SetDatabaseSize(name, size);
    info->SetDatabaseDescription(name, description);
  }
  return size;
}

int64 DatabaseTracker::UpdateOpenDatabaseInfoAndNotify(
    const string16& origin_id, const string16& name,
    const string16* opt_description) {
  DCHECK(database_connections_.IsDatabaseOpened(origin_id, name));
  int64 new_size = GetDBFileSize(origin_id, name);
  int64 old_size = database_connections_.GetOpenDatabaseSize(origin_id, name);
  CachedOriginInfo* info = MaybeGetCachedOriginInfo(origin_id, false);
  if (info && opt_description)
    info->SetDatabaseDescription(name, *opt_description);
  if (old_size != new_size) {
    database_connections_.SetOpenDatabaseSize(origin_id, name, new_size);
    if (info)
      info->SetDatabaseSize(name, new_size);
    if (quota_manager_proxy_)
      quota_manager_proxy_->NotifyStorageModified(
          quota::QuotaClient::kDatabase,
          DatabaseUtil::GetOriginFromIdentifier(origin_id),
          quota::kStorageTypeTemporary,
          new_size - old_size);
    FOR_EACH_OBSERVER(Observer, observers_, OnDatabaseSizeChanged(
        origin_id, name, new_size));
  }
  return new_size;
}

void DatabaseTracker::ScheduleDatabaseForDeletion(
    const string16& origin_identifier,
    const string16& database_name) {
  DCHECK(database_connections_.IsDatabaseOpened(origin_identifier,
                                                database_name));
  dbs_to_be_deleted_[origin_identifier].insert(database_name);
  FOR_EACH_OBSERVER(Observer, observers_, OnDatabaseScheduledForDeletion(
      origin_identifier, database_name));
}

void DatabaseTracker::ScheduleDatabasesForDeletion(
    const DatabaseSet& databases,
    const net::CompletionCallback& callback) {
  DCHECK(!databases.empty());

  if (!callback.is_null())
    deletion_callbacks_.push_back(std::make_pair(callback, databases));
  for (DatabaseSet::const_iterator ori = databases.begin();
       ori != databases.end(); ++ori) {
    for (std::set<string16>::const_iterator db = ori->second.begin();
         db != ori->second.end(); ++db)
      ScheduleDatabaseForDeletion(ori->first, *db);
  }
}

int DatabaseTracker::DeleteDatabase(const string16& origin_identifier,
                                    const string16& database_name,
                                    const net::CompletionCallback& callback) {
  if (!LazyInit())
    return net::ERR_FAILED;

  if (database_connections_.IsDatabaseOpened(origin_identifier,
                                             database_name)) {
    if (!callback.is_null()) {
      DatabaseSet set;
      set[origin_identifier].insert(database_name);
      deletion_callbacks_.push_back(std::make_pair(callback, set));
    }
    ScheduleDatabaseForDeletion(origin_identifier, database_name);
    return net::ERR_IO_PENDING;
  }
  DeleteClosedDatabase(origin_identifier, database_name);
  return net::OK;
}

int DatabaseTracker::DeleteDataModifiedSince(
    const base::Time& cutoff,
    const net::CompletionCallback& callback) {
  if (!LazyInit())
    return net::ERR_FAILED;

  DatabaseSet to_be_deleted;

  std::vector<string16> origins_identifiers;
  if (!databases_table_->GetAllOrigins(&origins_identifiers))
    return net::ERR_FAILED;
  int rv = net::OK;
  for (std::vector<string16>::const_iterator ori = origins_identifiers.begin();
       ori != origins_identifiers.end(); ++ori) {
    if (special_storage_policy_.get() &&
        special_storage_policy_->IsStorageProtected(
            DatabaseUtil::GetOriginFromIdentifier(*ori))) {
      continue;
    }

    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOrigin(*ori, &details))
      rv = net::ERR_FAILED;
    for (std::vector<DatabaseDetails>::const_iterator db = details.begin();
         db != details.end(); ++db) {
      base::FilePath db_file = GetFullDBFilePath(*ori, db->database_name);
      base::PlatformFileInfo file_info;
      file_util::GetFileInfo(db_file, &file_info);
      if (file_info.last_modified < cutoff)
        continue;

      // Check if the database is opened by any renderer.
      if (database_connections_.IsDatabaseOpened(*ori, db->database_name))
        to_be_deleted[*ori].insert(db->database_name);
      else
        DeleteClosedDatabase(*ori, db->database_name);
    }
  }

  if (rv != net::OK)
    return rv;

  if (!to_be_deleted.empty()) {
    ScheduleDatabasesForDeletion(to_be_deleted, callback);
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int DatabaseTracker::DeleteDataForOrigin(
    const string16& origin, const net::CompletionCallback& callback) {
  if (!LazyInit())
    return net::ERR_FAILED;

  DatabaseSet to_be_deleted;

  std::vector<DatabaseDetails> details;
  if (!databases_table_->GetAllDatabaseDetailsForOrigin(origin, &details))
    return net::ERR_FAILED;
  for (std::vector<DatabaseDetails>::const_iterator db = details.begin();
       db != details.end(); ++db) {
    // Check if the database is opened by any renderer.
    if (database_connections_.IsDatabaseOpened(origin, db->database_name))
      to_be_deleted[origin].insert(db->database_name);
    else
      DeleteClosedDatabase(origin, db->database_name);
  }

  if (!to_be_deleted.empty()) {
    ScheduleDatabasesForDeletion(to_be_deleted, callback);
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

void DatabaseTracker::GetIncognitoFileHandle(
    const string16& vfs_file_name, base::PlatformFile* file_handle) const {
  DCHECK(is_incognito_);
  FileHandlesMap::const_iterator it =
      incognito_file_handles_.find(vfs_file_name);
  if (it != incognito_file_handles_.end())
    *file_handle = it->second;
  else
    *file_handle = base::kInvalidPlatformFileValue;
}

void DatabaseTracker::SaveIncognitoFileHandle(
    const string16& vfs_file_name, const base::PlatformFile& file_handle) {
  DCHECK(is_incognito_);
  DCHECK(incognito_file_handles_.find(vfs_file_name) ==
         incognito_file_handles_.end());
  if (file_handle != base::kInvalidPlatformFileValue)
    incognito_file_handles_[vfs_file_name] = file_handle;
}

bool DatabaseTracker::CloseIncognitoFileHandle(const string16& vfs_file_name) {
  DCHECK(is_incognito_);
  DCHECK(incognito_file_handles_.find(vfs_file_name) !=
         incognito_file_handles_.end());

  bool handle_closed = false;
  FileHandlesMap::iterator it = incognito_file_handles_.find(vfs_file_name);
  if (it != incognito_file_handles_.end()) {
    handle_closed = base::ClosePlatformFile(it->second);
    if (handle_closed)
      incognito_file_handles_.erase(it);
  }
  return handle_closed;
}

bool DatabaseTracker::HasSavedIncognitoFileHandle(
    const string16& vfs_file_name) const {
  return (incognito_file_handles_.find(vfs_file_name) !=
          incognito_file_handles_.end());
}

void DatabaseTracker::DeleteIncognitoDBDirectory() {
  shutting_down_ = true;
  is_initialized_ = false;

  for (FileHandlesMap::iterator it = incognito_file_handles_.begin();
       it != incognito_file_handles_.end(); it++)
    base::ClosePlatformFile(it->second);

  base::FilePath incognito_db_dir =
      profile_path_.Append(kIncognitoDatabaseDirectoryName);
  if (file_util::DirectoryExists(incognito_db_dir))
    file_util::Delete(incognito_db_dir, true);
}

void DatabaseTracker::ClearSessionOnlyOrigins() {
  shutting_down_ = true;

  bool has_session_only_databases =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  // Clearing only session-only databases, and there are none.
  if (!has_session_only_databases)
    return;

  if (!LazyInit())
    return;

  std::vector<string16> origin_identifiers;
  GetAllOriginIdentifiers(&origin_identifiers);

  for (std::vector<string16>::iterator origin = origin_identifiers.begin();
       origin != origin_identifiers.end(); ++origin) {
    GURL origin_url =
        webkit_database::DatabaseUtil::GetOriginFromIdentifier(*origin);
    if (!special_storage_policy_->IsStorageSessionOnly(origin_url))
      continue;
    if (special_storage_policy_->IsStorageProtected(origin_url))
      continue;
    webkit_database::OriginInfo origin_info;
    std::vector<string16> databases;
    GetOriginInfo(*origin, &origin_info);
    origin_info.GetAllDatabaseNames(&databases);

    for (std::vector<string16>::iterator database = databases.begin();
         database != databases.end(); ++database) {
      base::PlatformFile file_handle = base::CreatePlatformFile(
          GetFullDBFilePath(*origin, *database),
          base::PLATFORM_FILE_OPEN_ALWAYS |
          base::PLATFORM_FILE_SHARE_DELETE |
          base::PLATFORM_FILE_DELETE_ON_CLOSE |
          base::PLATFORM_FILE_READ,
          NULL, NULL);
      base::ClosePlatformFile(file_handle);
    }
    DeleteOrigin(*origin, true);
  }
}


void DatabaseTracker::Shutdown() {
  DCHECK(db_tracker_thread_.get());
  DCHECK(db_tracker_thread_->BelongsToCurrentThread());
  if (shutting_down_) {
    NOTREACHED();
    return;
  }
  if (is_incognito_)
    DeleteIncognitoDBDirectory();
  else if (!force_keep_session_state_)
    ClearSessionOnlyOrigins();
}

void DatabaseTracker::SetForceKeepSessionState() {
  DCHECK(db_tracker_thread_.get());
  if (!db_tracker_thread_->BelongsToCurrentThread()) {
    db_tracker_thread_->PostTask(
        FROM_HERE,
        base::Bind(&DatabaseTracker::SetForceKeepSessionState, this));
    return;
  }
  force_keep_session_state_ = true;
}

}  // namespace webkit_database
