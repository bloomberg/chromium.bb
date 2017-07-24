// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_tracker.h"

#include <stdint.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "net/base/net_errors.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"
#include "storage/browser/database/database_quota_client.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/database/databases_table.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/sqlite/sqlite3.h"

namespace storage {

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
    : origin_identifier_(origin_info.origin_identifier_),
      total_size_(origin_info.total_size_),
      database_info_(origin_info.database_info_) {}

OriginInfo::~OriginInfo() {}

void OriginInfo::GetAllDatabaseNames(
    std::vector<base::string16>* databases) const {
  for (DatabaseInfoMap::const_iterator it = database_info_.begin();
       it != database_info_.end(); it++) {
    databases->push_back(it->first);
  }
}

int64_t OriginInfo::GetDatabaseSize(const base::string16& database_name) const {
  DatabaseInfoMap::const_iterator it = database_info_.find(database_name);
  if (it != database_info_.end())
    return it->second.first;
  return 0;
}

base::string16 OriginInfo::GetDatabaseDescription(
    const base::string16& database_name) const {
  DatabaseInfoMap::const_iterator it = database_info_.find(database_name);
  if (it != database_info_.end())
    return it->second.second;
  return base::string16();
}

OriginInfo::OriginInfo(const std::string& origin_identifier, int64_t total_size)
    : origin_identifier_(origin_identifier), total_size_(total_size) {}

DatabaseTracker::DatabaseTracker(
    const base::FilePath& profile_path,
    bool is_incognito,
    storage::SpecialStoragePolicy* special_storage_policy,
    storage::QuotaManagerProxy* quota_manager_proxy)
    : is_incognito_(is_incognito),
      profile_path_(profile_path),
      db_dir_(is_incognito_
                  ? profile_path_.Append(kIncognitoDatabaseDirectoryName)
                  : profile_path_.Append(kDatabaseDirectoryName)),
      db_(new sql::Connection()),
      special_storage_policy_(special_storage_policy),
      quota_manager_proxy_(quota_manager_proxy),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(new DatabaseQuotaClient(this));
  }
}

DatabaseTracker::~DatabaseTracker() {
  DCHECK(dbs_to_be_deleted_.empty());
  DCHECK(deletion_callbacks_.empty());
}

void DatabaseTracker::DatabaseOpened(const std::string& origin_identifier,
                                     const base::string16& database_name,
                                     const base::string16& database_description,
                                     int64_t estimated_size,
                                     int64_t* database_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (shutting_down_ || !LazyInit()) {
    *database_size = 0;
    return;
  }

  if (quota_manager_proxy_.get())
    quota_manager_proxy_->NotifyStorageAccessed(
        storage::QuotaClient::kDatabase,
        storage::GetOriginFromIdentifier(origin_identifier),
        storage::kStorageTypeTemporary);

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

void DatabaseTracker::DatabaseModified(const std::string& origin_identifier,
                                       const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return;
  UpdateOpenDatabaseSizeAndNotify(origin_identifier, database_name);
}

void DatabaseTracker::DatabaseClosed(const std::string& origin_identifier,
                                     const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_);
    return;
  }

  // We call NotifiyStorageAccessed when a db is opened and also when
  // closed because we don't call it for read while open.
  if (quota_manager_proxy_.get())
    quota_manager_proxy_->NotifyStorageAccessed(
        storage::QuotaClient::kDatabase,
        storage::GetOriginFromIdentifier(origin_identifier),
        storage::kStorageTypeTemporary);

  UpdateOpenDatabaseSizeAndNotify(origin_identifier, database_name);
  if (database_connections_.RemoveConnection(origin_identifier, database_name))
    DeleteDatabaseIfNeeded(origin_identifier, database_name);
}

void DatabaseTracker::HandleSqliteError(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
  std::vector<std::pair<std::string, base::string16> > open_dbs;
  connections.ListConnections(&open_dbs);
  for (std::vector<std::pair<std::string, base::string16> >::iterator it =
           open_dbs.begin(); it != open_dbs.end(); ++it)
    UpdateOpenDatabaseSizeAndNotify(it->first, it->second);

  std::vector<std::pair<std::string, base::string16> > closed_dbs;
  database_connections_.RemoveConnections(connections, &closed_dbs);
  for (std::vector<std::pair<std::string, base::string16> >::iterator it =
           closed_dbs.begin(); it != closed_dbs.end(); ++it) {
    DeleteDatabaseIfNeeded(it->first, it->second);
  }
}

void DatabaseTracker::DeleteDatabaseIfNeeded(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
        std::set<base::string16>& databases = found_origin->second;
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
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  observers_.AddObserver(observer);
}

void DatabaseTracker::RemoveObserver(Observer* observer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // When we remove a listener, we do not know which cached information
  // is still needed and which information can be discarded. So we just
  // clear all caches and re-populate them as needed.
  observers_.RemoveObserver(observer);
  ClearAllCachedOriginInfo();
}

void DatabaseTracker::CloseTrackerDatabaseAndClearCaches() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ClearAllCachedOriginInfo();

  if (!is_incognito_) {
    meta_table_.reset(nullptr);
    databases_table_.reset(nullptr);
    db_->Close();
    is_initialized_ = false;
  }
}

base::string16 DatabaseTracker::GetOriginDirectory(
    const std::string& origin_identifier) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!is_incognito_)
    return base::UTF8ToUTF16(origin_identifier);

  OriginDirectoriesMap::const_iterator it =
      incognito_origin_directories_.find(origin_identifier);
  if (it != incognito_origin_directories_.end())
    return it->second;

  base::string16 origin_directory =
      base::IntToString16(incognito_origin_directories_generator_++);
  incognito_origin_directories_[origin_identifier] = origin_directory;
  return origin_directory;
}

base::FilePath DatabaseTracker::GetFullDBFilePath(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin_identifier.empty());
  if (!LazyInit())
    return base::FilePath();

  int64_t id =
      databases_table_->GetDatabaseID(origin_identifier, database_name);
  if (id < 0)
    return base::FilePath();

  return db_dir_.Append(base::FilePath::FromUTF16Unsafe(
      GetOriginDirectory(origin_identifier))).AppendASCII(
          base::Int64ToString(id));
}

bool DatabaseTracker::GetOriginInfo(const std::string& origin_identifier,
                                    OriginInfo* info) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(info);
  CachedOriginInfo* cached_info = GetCachedOriginInfo(origin_identifier);
  if (!cached_info)
    return false;
  *info = OriginInfo(*cached_info);
  return true;
}

bool DatabaseTracker::GetAllOriginIdentifiers(
    std::vector<std::string>* origin_identifiers) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origin_identifiers);
  DCHECK(origin_identifiers->empty());
  if (!LazyInit())
    return false;
  return databases_table_->GetAllOriginIdentifiers(origin_identifiers);
}

bool DatabaseTracker::GetAllOriginsInfo(
    std::vector<OriginInfo>* origins_info) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origins_info);
  DCHECK(origins_info->empty());

  std::vector<std::string> origins;
  if (!GetAllOriginIdentifiers(&origins))
    return false;

  for (std::vector<std::string>::const_iterator it = origins.begin();
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

bool DatabaseTracker::DeleteClosedDatabase(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return false;

  // Check if the database is opened by any renderer.
  if (database_connections_.IsDatabaseOpened(origin_identifier, database_name))
    return false;

  int64_t db_file_size = quota_manager_proxy_.get()
                             ? GetDBFileSize(origin_identifier, database_name)
                             : 0;

  // Try to delete the file on the hard drive.
  base::FilePath db_file = GetFullDBFilePath(origin_identifier, database_name);
  if (!sql::Connection::Delete(db_file))
    return false;

  if (quota_manager_proxy_.get() && db_file_size)
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kDatabase,
        storage::GetOriginFromIdentifier(origin_identifier),
        storage::kStorageTypeTemporary,
        -db_file_size);

  // Clean up the main database and invalidate the cached record.
  databases_table_->DeleteDatabaseDetails(origin_identifier, database_name);
  origins_info_map_.erase(origin_identifier);

  std::vector<DatabaseDetails> details;
  if (databases_table_->GetAllDatabaseDetailsForOriginIdentifier(
          origin_identifier, &details) && details.empty()) {
    // Try to delete the origin in case this was the last database.
    DeleteOrigin(origin_identifier, false);
  }
  return true;
}

bool DatabaseTracker::DeleteOrigin(const std::string& origin_identifier,
                                   bool force) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return false;

  // Check if any database in this origin is opened by any renderer.
  if (database_connections_.IsOriginUsed(origin_identifier) && !force)
    return false;

  int64_t deleted_size = 0;
  if (quota_manager_proxy_.get()) {
    CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
    if (origin_info)
      deleted_size = origin_info->TotalSize();
  }

  origins_info_map_.erase(origin_identifier);
  base::FilePath origin_dir = db_dir_.AppendASCII(origin_identifier);

  // Create a temporary directory to move possibly still existing databases to,
  // as we can't delete the origin directory on windows if it contains opened
  // files.
  base::FilePath new_origin_dir;
  base::CreateTemporaryDirInDir(db_dir_,
                                kTemporaryDirectoryPrefix,
                                &new_origin_dir);
  base::FileEnumerator databases(
      origin_dir,
      false,
      base::FileEnumerator::FILES);
  for (base::FilePath database = databases.Next(); !database.empty();
       database = databases.Next()) {
    base::FilePath new_file = new_origin_dir.Append(database.BaseName());
    base::Move(database, new_file);
  }
  base::DeleteFile(origin_dir, true);
  base::DeleteFile(new_origin_dir, true); // might fail on windows.

  databases_table_->DeleteOriginIdentifier(origin_identifier);

  if (quota_manager_proxy_.get() && deleted_size) {
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kDatabase,
        storage::GetOriginFromIdentifier(origin_identifier),
        storage::kStorageTypeTemporary,
        -deleted_size);
  }

  return true;
}

bool DatabaseTracker::IsDatabaseScheduledForDeletion(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DatabaseSet::iterator it = dbs_to_be_deleted_.find(origin_identifier);
  if (it == dbs_to_be_deleted_.end())
    return false;

  std::set<base::string16>& databases = it->second;
  return (databases.find(database_name) != databases.end());
}

bool DatabaseTracker::LazyInit() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!is_initialized_ && !shutting_down_) {
    DCHECK(!db_->is_open());
    DCHECK(!databases_table_.get());
    DCHECK(!meta_table_.get());

    // If there are left-over directories from failed deletion attempts, clean
    // them up.
    if (base::DirectoryExists(db_dir_)) {
      base::FileEnumerator directories(
          db_dir_,
          false,
          base::FileEnumerator::DIRECTORIES,
          kTemporaryDirectoryPattern);
      for (base::FilePath directory = directories.Next(); !directory.empty();
           directory = directories.Next()) {
        base::DeleteFile(directory, true);
      }
    }

    db_->set_histogram_tag("DatabaseTracker");

    // If the tracker database exists, but it's corrupt or doesn't
    // have a meta table, delete the database directory.
    const base::FilePath kTrackerDatabaseFullPath =
        db_dir_.Append(base::FilePath(kTrackerDatabaseFileName));
    if (base::DirectoryExists(db_dir_) &&
        base::PathExists(kTrackerDatabaseFullPath) &&
        (!db_->Open(kTrackerDatabaseFullPath) ||
         !sql::MetaTable::DoesTableExist(db_.get()))) {
      db_->Close();
      if (!base::DeleteFile(db_dir_, true))
        return false;
    }

    databases_table_.reset(new DatabasesTable(db_.get()));
    meta_table_.reset(new sql::MetaTable());

    is_initialized_ =
        base::CreateDirectory(db_dir_) &&
        (db_->is_open() ||
         (is_incognito_ ? db_->OpenInMemory() :
          db_->Open(kTrackerDatabaseFullPath))) &&
        UpgradeToCurrentVersion();
    if (!is_initialized_) {
      databases_table_.reset(nullptr);
      meta_table_.reset(nullptr);
      db_->Close();
    }
  }
  return is_initialized_;
}

bool DatabaseTracker::UpgradeToCurrentVersion() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
    const std::string& origin_identifier,
    const base::string16& database_name,
    const base::string16& database_description,
    int64_t estimated_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  origins_info_map_.clear();
}

DatabaseTracker::CachedOriginInfo* DatabaseTracker::MaybeGetCachedOriginInfo(
    const std::string& origin_identifier, bool create_if_needed) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return nullptr;

  // Populate the cache with data for this origin if needed.
  if (origins_info_map_.find(origin_identifier) == origins_info_map_.end()) {
    if (!create_if_needed)
      return nullptr;

    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOriginIdentifier(
            origin_identifier, &details)) {
      return nullptr;
    }

    CachedOriginInfo& origin_info = origins_info_map_[origin_identifier];
    origin_info.SetOriginIdentifier(origin_identifier);
    for (std::vector<DatabaseDetails>::const_iterator it = details.begin();
         it != details.end(); it++) {
      int64_t db_file_size;
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

int64_t DatabaseTracker::GetDBFileSize(const std::string& origin_identifier,
                                       const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::FilePath db_file_name = GetFullDBFilePath(origin_identifier,
                                                  database_name);
  int64_t db_file_size = 0;
  if (!base::GetFileSize(db_file_name, &db_file_size))
    db_file_size = 0;
  return db_file_size;
}

int64_t DatabaseTracker::SeedOpenDatabaseInfo(
    const std::string& origin_id,
    const base::string16& name,
    const base::string16& description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_connections_.IsDatabaseOpened(origin_id, name));
  int64_t size = GetDBFileSize(origin_id, name);
  database_connections_.SetOpenDatabaseSize(origin_id, name,  size);
  CachedOriginInfo* info = MaybeGetCachedOriginInfo(origin_id, false);
  if (info) {
    info->SetDatabaseSize(name, size);
    info->SetDatabaseDescription(name, description);
  }
  return size;
}

int64_t DatabaseTracker::UpdateOpenDatabaseInfoAndNotify(
    const std::string& origin_id,
    const base::string16& name,
    const base::string16* opt_description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_connections_.IsDatabaseOpened(origin_id, name));
  int64_t new_size = GetDBFileSize(origin_id, name);
  int64_t old_size = database_connections_.GetOpenDatabaseSize(origin_id, name);
  CachedOriginInfo* info = MaybeGetCachedOriginInfo(origin_id, false);
  if (info && opt_description)
    info->SetDatabaseDescription(name, *opt_description);
  if (old_size != new_size) {
    database_connections_.SetOpenDatabaseSize(origin_id, name, new_size);
    if (info)
      info->SetDatabaseSize(name, new_size);
    if (quota_manager_proxy_.get())
      quota_manager_proxy_->NotifyStorageModified(
          storage::QuotaClient::kDatabase,
          storage::GetOriginFromIdentifier(origin_id),
          storage::kStorageTypeTemporary,
          new_size - old_size);
    for (auto& observer : observers_)
      observer.OnDatabaseSizeChanged(origin_id, name, new_size);
  }
  return new_size;
}

void DatabaseTracker::ScheduleDatabaseForDeletion(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_connections_.IsDatabaseOpened(origin_identifier,
                                                database_name));
  dbs_to_be_deleted_[origin_identifier].insert(database_name);
  for (auto& observer : observers_)
    observer.OnDatabaseScheduledForDeletion(origin_identifier, database_name);
}

void DatabaseTracker::ScheduleDatabasesForDeletion(
    const DatabaseSet& databases,
    const net::CompletionCallback& callback) {
  DCHECK(!databases.empty());

  if (!callback.is_null())
    deletion_callbacks_.push_back(std::make_pair(callback, databases));
  for (DatabaseSet::const_iterator ori = databases.begin();
       ori != databases.end(); ++ori) {
    for (std::set<base::string16>::const_iterator db = ori->second.begin();
         db != ori->second.end(); ++db)
      ScheduleDatabaseForDeletion(ori->first, *db);
  }
}

int DatabaseTracker::DeleteDatabase(const std::string& origin_identifier,
                                    const base::string16& database_name,
                                    const net::CompletionCallback& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return net::ERR_FAILED;

  DatabaseSet to_be_deleted;

  std::vector<std::string> origins_identifiers;
  if (!databases_table_->GetAllOriginIdentifiers(&origins_identifiers))
    return net::ERR_FAILED;
  int rv = net::OK;
  for (std::vector<std::string>::const_iterator ori =
           origins_identifiers.begin();
       ori != origins_identifiers.end(); ++ori) {
    if (special_storage_policy_.get() &&
        special_storage_policy_->IsStorageProtected(
            storage::GetOriginFromIdentifier(*ori))) {
      continue;
    }

    std::vector<DatabaseDetails> details;
    if (!databases_table_->
            GetAllDatabaseDetailsForOriginIdentifier(*ori, &details))
      rv = net::ERR_FAILED;
    for (std::vector<DatabaseDetails>::const_iterator db = details.begin();
         db != details.end(); ++db) {
      base::FilePath db_file = GetFullDBFilePath(*ori, db->database_name);
      base::File::Info file_info;
      base::GetFileInfo(db_file, &file_info);
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
    const std::string& origin, const net::CompletionCallback& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return net::ERR_FAILED;

  DatabaseSet to_be_deleted;

  std::vector<DatabaseDetails> details;
  if (!databases_table_->
          GetAllDatabaseDetailsForOriginIdentifier(origin, &details))
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

const base::File* DatabaseTracker::GetIncognitoFile(
    const base::string16& vfs_file_name) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_incognito_);
  FileHandlesMap::const_iterator it =
      incognito_file_handles_.find(vfs_file_name);
  if (it != incognito_file_handles_.end())
    return it->second;

  return nullptr;
}

const base::File* DatabaseTracker::SaveIncognitoFile(
    const base::string16& vfs_file_name,
    base::File file) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_incognito_);
  if (!file.IsValid())
    return nullptr;

  base::File* to_insert = new base::File(std::move(file));
  std::pair<FileHandlesMap::iterator, bool> rv =
      incognito_file_handles_.insert(std::make_pair(vfs_file_name, to_insert));
  DCHECK(rv.second);
  return rv.first->second;
}

void DatabaseTracker::CloseIncognitoFileHandle(
    const base::string16& vfs_file_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_incognito_);
  DCHECK(incognito_file_handles_.find(vfs_file_name) !=
         incognito_file_handles_.end());

  FileHandlesMap::iterator it = incognito_file_handles_.find(vfs_file_name);
  if (it != incognito_file_handles_.end()) {
    delete it->second;
    incognito_file_handles_.erase(it);
  }
}

bool DatabaseTracker::HasSavedIncognitoFileHandle(
    const base::string16& vfs_file_name) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return (incognito_file_handles_.find(vfs_file_name) !=
          incognito_file_handles_.end());
}

void DatabaseTracker::DeleteIncognitoDBDirectory() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  is_initialized_ = false;

  for (FileHandlesMap::iterator it = incognito_file_handles_.begin();
       it != incognito_file_handles_.end(); it++) {
    delete it->second;
  }

  base::FilePath incognito_db_dir =
      profile_path_.Append(kIncognitoDatabaseDirectoryName);
  if (base::DirectoryExists(incognito_db_dir))
    base::DeleteFile(incognito_db_dir, true);
}

void DatabaseTracker::ClearSessionOnlyOrigins() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  bool has_session_only_databases =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  // Clearing only session-only databases, and there are none.
  if (!has_session_only_databases)
    return;

  if (!LazyInit())
    return;

  std::vector<std::string> origin_identifiers;
  GetAllOriginIdentifiers(&origin_identifiers);

  for (std::vector<std::string>::iterator origin =
           origin_identifiers.begin();
       origin != origin_identifiers.end(); ++origin) {
    GURL origin_url = storage::GetOriginFromIdentifier(*origin);
    if (!special_storage_policy_->IsStorageSessionOnly(origin_url))
      continue;
    if (special_storage_policy_->IsStorageProtected(origin_url))
      continue;
    storage::OriginInfo origin_info;
    std::vector<base::string16> databases;
    GetOriginInfo(*origin, &origin_info);
    origin_info.GetAllDatabaseNames(&databases);

    for (std::vector<base::string16>::iterator database = databases.begin();
         database != databases.end(); ++database) {
      base::File file(GetFullDBFilePath(*origin, *database),
                      base::File::FLAG_OPEN_ALWAYS |
                          base::File::FLAG_SHARE_DELETE |
                          base::File::FLAG_DELETE_ON_CLOSE |
                          base::File::FLAG_READ);
    }
    DeleteOrigin(*origin, true);
  }
}


void DatabaseTracker::Shutdown() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (shutting_down_) {
    NOTREACHED();
    return;
  }
  shutting_down_ = true;
  if (is_incognito_)
    DeleteIncognitoDBDirectory();
  else if (!force_keep_session_state_)
    ClearSessionOnlyOrigins();
  CloseTrackerDatabaseAndClearCaches();
}

void DatabaseTracker::SetForceKeepSessionState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  force_keep_session_state_ = true;
}

}  // namespace storage
