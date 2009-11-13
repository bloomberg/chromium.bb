// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_tracker.h"

#include <vector>

#include "app/sql/connection.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "webkit/database/databases_table.h"

namespace webkit_database {

const FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases");
const FilePath::CharType kTrackerDatabaseFileName[] =
    FILE_PATH_LITERAL("Databases.db");
const int kCurrentVersion = 1;
const int kCompatibleVersion = 1;
const int64 kDefaultQuota = 5 * 1024 * 1024;

DatabaseTracker::DatabaseTracker(const FilePath& profile_path)
    : initialized_(false),
      db_dir_(profile_path.Append(FilePath(kDatabaseDirectoryName))),
      db_(new sql::Connection()),
      databases_table_(NULL),
      meta_table_(NULL) {
}

DatabaseTracker::~DatabaseTracker() {
  DCHECK(observers_.size() == 0);
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

  *database_size = GetCachedDatabaseFileSize(origin_identifier, database_name);
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
  // TODO(dumi): figure out how to use this information at a later time
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

bool DatabaseTracker::LazyInit() {
  if (!initialized_) {
    // If the tracker database exists, but it's corrupt or doesn't
    // have a meta table, delete the database directory
    const FilePath kTrackerDatabaseFullPath =
        db_dir_.Append(FilePath(kTrackerDatabaseFileName));
    if (file_util::DirectoryExists(db_dir_) &&
        file_util::PathExists(kTrackerDatabaseFullPath) &&
        (!db_->Open(kTrackerDatabaseFullPath) ||
         !db_->DoesTableExist("meta"))) {
      db_->Close();
      if (!file_util::Delete(db_dir_, true))
        return false;
    }

    databases_table_.reset(new DatabasesTable(db_.get()));
    meta_table_.reset(new sql::MetaTable());

    initialized_ =
        file_util::CreateDirectory(db_dir_) &&
        db_->Open(kTrackerDatabaseFullPath) &&
        meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion) &&
        (meta_table_->GetCompatibleVersionNumber() <= kCurrentVersion) &&
        databases_table_->Init();
  }
  return initialized_;
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

int64 DatabaseTracker::GetDBFileSize(const string16& origin_identifier,
                                     const string16& database_name) const {
  FilePath db_file_name = GetFullDBFilePath(origin_identifier, database_name);
  int64 db_file_size = 0;
  if (!file_util::GetFileSize(db_file_name, &db_file_size))
    db_file_size = 0;
  return db_file_size;
}

void DatabaseTracker::ClearAllCachedOriginInfo() {
  origins_info_map_.clear();
}

DatabaseTracker::CachedOriginInfo* DatabaseTracker::GetCachedOriginInfo(
    const string16& origin_identifier) {
  // Populate the cache with data for this origin if needed.
  if (origins_info_map_.find(origin_identifier) == origins_info_map_.end()) {
    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOrigin(
            origin_identifier, &details)) {
      return NULL;
    }

    CachedOriginInfo& origin_info = origins_info_map_[origin_identifier];
    for (std::vector<DatabaseDetails>::const_iterator it = details.begin();
         it != details.end(); it++) {
      int64 db_file_size =
          GetDBFileSize(it->origin_identifier, it->database_name);
      origin_info.SetCachedDatabaseSize(it->database_name, db_file_size);
    }
  }

  return &origins_info_map_[origin_identifier];
}

int64 DatabaseTracker::GetCachedDatabaseFileSize(
    const string16& origin_identifier,
    const string16& database_name) {
  CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
  if (!origin_info)
    return 0;
  return origin_info->GetCachedDatabaseSize(database_name);
}

int64 DatabaseTracker::UpdateCachedDatabaseFileSize(
    const string16& origin_identifier,
    const string16& database_name) {
  int64 new_size = GetDBFileSize(origin_identifier, database_name);
  CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
  if (origin_info)
    origin_info->SetCachedDatabaseSize(database_name, new_size);
  return new_size;
}

int64 DatabaseTracker::GetOriginUsage(const string16& origin_identifier) {
  CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
  if (!origin_info)
    return kint64max;
  return origin_info->TotalSize();
}

int64 DatabaseTracker::GetOriginQuota(
    const string16& /*origin_identifier*/) const {
  return kDefaultQuota;
}

int64 DatabaseTracker::GetOriginSpaceAvailable(
    const string16& origin_identifier) {
  int64 space_available = GetOriginQuota(origin_identifier) -
      GetOriginUsage(origin_identifier);
  return (space_available < 0 ? 0 : space_available);
}

}  // namespace webkit_database
