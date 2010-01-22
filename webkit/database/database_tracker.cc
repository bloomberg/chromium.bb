// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_tracker.h"

#include <vector>

#include "app/sql/connection.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "webkit/database/databases_table.h"
#include "webkit/database/quota_table.h"

namespace webkit_database {

const FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases");
const FilePath::CharType kTrackerDatabaseFileName[] =
    FILE_PATH_LITERAL("Databases.db");
const int kCurrentVersion = 2;
const int kCompatibleVersion = 1;
const int64 kDefaultExtensionQuota = 50 * 1024 * 1024;
const char* kExtensionOriginIdentifierPrefix = "chrome-extension_";

DatabaseTracker::DatabaseTracker(const FilePath& profile_path)
    : initialized_(false),
      db_dir_(profile_path.Append(FilePath(kDatabaseDirectoryName))),
      db_(new sql::Connection()),
      databases_table_(NULL),
      meta_table_(NULL),
      default_quota_(5 * 1024 * 1024) {
}

DatabaseTracker::~DatabaseTracker() {
  DCHECK(observers_.size() == 0);
}

void DatabaseTracker::SetDefaultQuota(int64 quota) {
  default_quota_ = quota;
  ClearAllCachedOriginInfo();
}

void DatabaseTracker::DatabaseOpened(const string16& origin_identifier,
                                     const string16& database_name,
                                     const string16& database_description,
                                     int64 estimated_size,
                                     int64* database_size,
                                     int64* space_available) {
  if (!LazyInit()) {
    *database_size = 0;
    *space_available = 0;
    return;
  }

  InsertOrUpdateDatabaseDetails(origin_identifier, database_name,
                                database_description, estimated_size);
  database_connections_.AddConnection(origin_identifier, database_name);

  CachedOriginInfo* info = GetCachedOriginInfo(origin_identifier);
  *database_size = (info ? info->GetDatabaseSize(database_name) : 0);
  *space_available = GetOriginSpaceAvailable(origin_identifier);
}

void DatabaseTracker::DatabaseModified(const string16& origin_identifier,
                                       const string16& database_name) {
  if (!LazyInit())
    return;

  int64 updated_db_size =
      UpdateCachedDatabaseFileSize(origin_identifier, database_name);
  int64 space_available = GetOriginSpaceAvailable(origin_identifier);
  FOR_EACH_OBSERVER(Observer, observers_, OnDatabaseSizeChanged(
      origin_identifier, database_name, updated_db_size, space_available));
}

void DatabaseTracker::DatabaseClosed(const string16& origin_identifier,
                                     const string16& database_name) {
  database_connections_.RemoveConnection(origin_identifier, database_name);
}

void DatabaseTracker::CloseDatabases(const DatabaseConnections& connections) {
  database_connections_.RemoveConnections(connections);
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
  meta_table_.reset(NULL);
  databases_table_.reset(NULL);
  quota_table_.reset(NULL);
  db_->Close();
  initialized_ = false;
}

FilePath DatabaseTracker::GetFullDBFilePath(
    const string16& origin_identifier,
    const string16& database_name) const {
  DCHECK(!origin_identifier.empty());
  DCHECK(!database_name.empty());
  int64 id = databases_table_->GetDatabaseID(
      origin_identifier, database_name);
  if (id < 0)
    return FilePath();

  FilePath file_name = FilePath::FromWStringHack(Int64ToWString(id));
  return db_dir_.Append(FilePath::FromWStringHack(
      UTF16ToWide(origin_identifier))).Append(file_name);
}

bool DatabaseTracker::GetAllOriginsInfo(std::vector<OriginInfo>* origins_info) {
  DCHECK(origins_info);
  DCHECK(origins_info->empty());
  std::vector<string16> origins;
  if (!databases_table_->GetAllOrigins(&origins))
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

void DatabaseTracker::SetOriginQuota(const string16& origin_identifier,
                                     int64 new_quota) {
  if (quota_table_->SetOriginQuota(origin_identifier, new_quota) &&
      (origins_info_map_.find(origin_identifier) != origins_info_map_.end())) {
    origins_info_map_[origin_identifier].SetQuota(new_quota);
  }
}

bool DatabaseTracker::DeleteDatabase(const string16& origin_identifier,
                                     const string16& database_name) {
  // Check if the database is opened by any renderer.
  if (database_connections_.IsDatabaseOpened(origin_identifier, database_name))
    return false;

  // Try to delete the file on the hard drive.
  FilePath db_file = GetFullDBFilePath(origin_identifier, database_name);
  if (file_util::PathExists(db_file) && !file_util::Delete(db_file, false))
    return false;

  // Clean up the main database and invalidate the cached record.
  databases_table_->DeleteDatabaseDetails(origin_identifier, database_name);
  origins_info_map_.erase(origin_identifier);
  return true;
}

bool DatabaseTracker::DeleteOrigin(const string16& origin_identifier) {
  // Check if any database in this origin is opened by any renderer.
  if (database_connections_.IsOriginUsed(origin_identifier))
    return false;

  // We need to invalidate the cached record whether file_util::Delete()
  // succeeds or not, because even if it fails, it might still delete some
  // DB files on the hard drive.
  origins_info_map_.erase(origin_identifier);
  FilePath origin_dir = db_dir_.Append(FilePath::FromWStringHack(
      UTF16ToWide(origin_identifier)));
  if (!file_util::Delete(origin_dir, true))
    return false;

  databases_table_->DeleteOrigin(origin_identifier);
  return true;
}

bool DatabaseTracker::LazyInit() {
  if (!initialized_) {
    DCHECK(!db_->is_open());
    DCHECK(!databases_table_.get());
    DCHECK(!quota_table_.get());
    DCHECK(!meta_table_.get());

    // If the tracker database exists, but it's corrupt or doesn't
    // have a meta table, delete the database directory.
    const FilePath kTrackerDatabaseFullPath =
        db_dir_.Append(FilePath(kTrackerDatabaseFileName));
    if (file_util::DirectoryExists(db_dir_) &&
        file_util::PathExists(kTrackerDatabaseFullPath) &&
        (!db_->Open(kTrackerDatabaseFullPath) ||
         !sql::MetaTable::DoesTableExist(db_.get()))) {
      db_->Close();
      if (!file_util::Delete(db_dir_, true))
        return false;
    }

    databases_table_.reset(new DatabasesTable(db_.get()));
    quota_table_.reset(new QuotaTable(db_.get()));
    meta_table_.reset(new sql::MetaTable());

    initialized_ =
        file_util::CreateDirectory(db_dir_) &&
        (db_->is_open() || db_->Open(kTrackerDatabaseFullPath)) &&
        UpgradeToCurrentVersion();
    if (!initialized_) {
      databases_table_.reset(NULL);
      quota_table_.reset(NULL);
      meta_table_.reset(NULL);
      db_->Close();
    }
  }
  return initialized_;
}

bool DatabaseTracker::UpgradeToCurrentVersion() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin() ||
      !meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion) ||
      (meta_table_->GetCompatibleVersionNumber() > kCurrentVersion) ||
      !databases_table_->Init() ||
      !quota_table_->Init())
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

DatabaseTracker::CachedOriginInfo* DatabaseTracker::GetCachedOriginInfo(
    const string16& origin_identifier) {
  if (!LazyInit())
    return NULL;

  // Populate the cache with data for this origin if needed.
  if (origins_info_map_.find(origin_identifier) == origins_info_map_.end()) {
    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOrigin(
            origin_identifier, &details)) {
      return NULL;
    }

    CachedOriginInfo& origin_info = origins_info_map_[origin_identifier];
    origin_info.SetOrigin(origin_identifier);
    for (std::vector<DatabaseDetails>::const_iterator it = details.begin();
         it != details.end(); it++) {
      int64 db_file_size =
          GetDBFileSize(origin_identifier, it->database_name);
      origin_info.SetDatabaseSize(it->database_name, db_file_size);
    }

    int64 origin_quota = quota_table_->GetOriginQuota(origin_identifier);
    if (origin_quota > 0) {
      origin_info.SetQuota(origin_quota);
    } else if (StartsWith(origin_identifier,
                          ASCIIToUTF16(kExtensionOriginIdentifierPrefix),
                          true)) {
      origin_info.SetQuota(kDefaultExtensionQuota);
    } else {
      origin_info.SetQuota(default_quota_);
    }
  }

  return &origins_info_map_[origin_identifier];
}

int64 DatabaseTracker::GetDBFileSize(const string16& origin_identifier,
                                     const string16& database_name) const {
  FilePath db_file_name = GetFullDBFilePath(origin_identifier, database_name);
  int64 db_file_size = 0;
  if (!file_util::GetFileSize(db_file_name, &db_file_size))
    db_file_size = 0;
  return db_file_size;
}

int64 DatabaseTracker::GetOriginSpaceAvailable(
    const string16& origin_identifier) {
  CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
  if (!origin_info)
    return 0;
  int64 space_available = origin_info->Quota() - origin_info->TotalSize();
  return (space_available < 0 ? 0 : space_available);
}

int64 DatabaseTracker::UpdateCachedDatabaseFileSize(
    const string16& origin_identifier,
    const string16& database_name) {
  int64 new_size = GetDBFileSize(origin_identifier, database_name);
  CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
  if (origin_info)
    origin_info->SetDatabaseSize(database_name, new_size);
  return new_size;
}

}  // namespace webkit_database
