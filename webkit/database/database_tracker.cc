// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_tracker.h"

#include <algorithm>
#include <vector>

#include "app/sql/connection.h"
#include "app/sql/diagnostic_error_delegate.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "webkit/database/database_util.h"
#include "webkit/database/databases_table.h"
#include "webkit/database/quota_table.h"
#include "webkit/quota/special_storage_policy.h"

namespace {

class HistogramUniquifier {
 public:
  static const char* name() { return "Sqlite.DatabaseTracker.Error"; }
};

sql::ErrorDelegate* GetErrorHandlerForTrackerDb() {
  return new sql::DiagnosticErrorDelegate<HistogramUniquifier>();
}

}  // anon namespace

namespace webkit_database {

const FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases");
const FilePath::CharType kIncognitoDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases-incognito");
const FilePath::CharType kTrackerDatabaseFileName[] =
    FILE_PATH_LITERAL("Databases.db");
static const int kCurrentVersion = 2;
static const int kCompatibleVersion = 1;
static const char* kExtensionOriginIdentifierPrefix = "chrome-extension_";

OriginInfo::OriginInfo(const OriginInfo& origin_info)
    : origin_(origin_info.origin_),
      total_size_(origin_info.total_size_),
      quota_(origin_info.quota_),
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

OriginInfo::OriginInfo(const string16& origin, int64 total_size, int64 quota)
    : origin_(origin), total_size_(total_size), quota_(quota) {}

DatabaseTracker::DatabaseTracker(
    const FilePath& profile_path,
    bool is_incognito,
    quota::SpecialStoragePolicy* special_storage_policy)
    : is_initialized_(false),
      is_incognito_(is_incognito),
      shutting_down_(false),
      profile_path_(profile_path),
      db_dir_(is_incognito_ ?
              profile_path_.Append(kIncognitoDatabaseDirectoryName) :
              profile_path_.Append(kDatabaseDirectoryName)),
      db_(new sql::Connection()),
      databases_table_(NULL),
      meta_table_(NULL),
      default_quota_(5 * 1024 * 1024),
      special_storage_policy_(special_storage_policy),
      incognito_origin_directories_generator_(0) {
}

DatabaseTracker::~DatabaseTracker() {
  DCHECK(dbs_to_be_deleted_.empty());
  DCHECK(deletion_callbacks_.empty());
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
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_);
    return;
  }
  database_connections_.RemoveConnection(origin_identifier, database_name);
  if (!database_connections_.IsDatabaseOpened(origin_identifier, database_name))
    DeleteDatabaseIfNeeded(origin_identifier, database_name);
}

void DatabaseTracker::CloseDatabases(const DatabaseConnections& connections) {
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_ || connections.IsEmpty());
    return;
  }
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

    std::vector<net::CompletionCallback*> to_be_deleted;
    for (PendingCompletionMap::iterator callback = deletion_callbacks_.begin();
         callback != deletion_callbacks_.end(); ++callback) {
      DatabaseSet::iterator found_origin =
          callback->second.find(origin_identifier);
      if (found_origin != callback->second.end()) {
        std::set<string16>& databases = found_origin->second;
        databases.erase(database_name);
        if (databases.empty()) {
          callback->second.erase(found_origin);
          if (callback->second.empty()) {
            net::CompletionCallback* cb = callback->first;
            cb->Run(net::OK);
            to_be_deleted.push_back(cb);
          }
        }
      }
    }
    for (std::vector<net::CompletionCallback*>::iterator cb =
         to_be_deleted.begin(); cb != to_be_deleted.end(); ++cb)
      deletion_callbacks_.erase(*cb);
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
    quota_table_.reset(NULL);
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

FilePath DatabaseTracker::GetFullDBFilePath(
    const string16& origin_identifier,
    const string16& database_name) {
  DCHECK(!origin_identifier.empty());
  DCHECK(!database_name.empty());
  if (!LazyInit())
    return FilePath();

  int64 id = databases_table_->GetDatabaseID(
      origin_identifier, database_name);
  if (id < 0)
    return FilePath();

  FilePath file_name = FilePath::FromWStringHack(
      UTF8ToWide(base::Int64ToString(id)));
  return db_dir_.Append(FilePath::FromWStringHack(
      UTF16ToWide(GetOriginDirectory(origin_identifier)))).Append(file_name);
}

bool DatabaseTracker::GetAllOriginsInfo(std::vector<OriginInfo>* origins_info) {
  DCHECK(origins_info);
  DCHECK(origins_info->empty());
  if (!LazyInit())
    return false;

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
  if (!LazyInit())
    return;

  if (quota_table_->SetOriginQuota(origin_identifier, new_quota) &&
      (origins_info_map_.find(origin_identifier) != origins_info_map_.end())) {
    origins_info_map_[origin_identifier].SetQuota(new_quota);
  }
}


bool DatabaseTracker::DeleteClosedDatabase(const string16& origin_identifier,
                                           const string16& database_name) {
  if (!LazyInit())
    return false;

  // Check if the database is opened by any renderer.
  if (database_connections_.IsDatabaseOpened(origin_identifier, database_name))
    return false;

  // Try to delete the file on the hard drive.
  // TODO(jochen): Delete journal files associated with this database.
  FilePath db_file = GetFullDBFilePath(origin_identifier, database_name);
  if (file_util::PathExists(db_file) && !file_util::Delete(db_file, false))
    return false;

  // Clean up the main database and invalidate the cached record.
  databases_table_->DeleteDatabaseDetails(origin_identifier, database_name);
  origins_info_map_.erase(origin_identifier);

  // Try to delete the origin in case this was the last database.
  std::vector<DatabaseDetails> details;
  if (databases_table_->GetAllDatabaseDetailsForOrigin(
          origin_identifier, &details) && details.empty())
    DeleteOrigin(origin_identifier);
  return true;
}

bool DatabaseTracker::DeleteOrigin(const string16& origin_identifier) {
  if (!LazyInit())
    return false;

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

    db_->set_error_delegate(GetErrorHandlerForTrackerDb());

    databases_table_.reset(new DatabasesTable(db_.get()));
    quota_table_.reset(new QuotaTable(db_.get()));
    meta_table_.reset(new sql::MetaTable());

    is_initialized_ =
        file_util::CreateDirectory(db_dir_) &&
        (db_->is_open() ||
         (is_incognito_ ? db_->OpenInMemory() :
          db_->Open(kTrackerDatabaseFullPath))) &&
        UpgradeToCurrentVersion();
    if (!is_initialized_) {
      databases_table_.reset(NULL);
      quota_table_.reset(NULL);
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
      origin_info.SetDatabaseDescription(it->database_name, it->description);
    }

    if (special_storage_policy_.get() &&
        special_storage_policy_->IsStorageUnlimited(
            DatabaseUtil::GetOriginFromIdentifier(origin_identifier))) {
      // TODO(michaeln): handle the case where it changes status sometime after
      // the cached origin_info has been established
      origin_info.SetQuota(kint64max);
    } else {
      int64 origin_quota = quota_table_->GetOriginQuota(origin_identifier);
      if (origin_quota > 0)
        origin_info.SetQuota(origin_quota);
      else
        origin_info.SetQuota(default_quota_);
    }
  }

  return &origins_info_map_[origin_identifier];
}

int64 DatabaseTracker::GetDBFileSize(const string16& origin_identifier,
                                     const string16& database_name) {
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
    net::CompletionCallback* callback) {
  DCHECK(!callback ||
         deletion_callbacks_.find(callback) == deletion_callbacks_.end());
  DCHECK(!databases.empty());
  if (callback)
    deletion_callbacks_[callback] = databases;
  for (DatabaseSet::const_iterator ori = databases.begin();
       ori != databases.end(); ++ori) {
    for (std::set<string16>::const_iterator db = ori->second.begin();
         db != ori->second.end(); ++db)
      ScheduleDatabaseForDeletion(ori->first, *db);
  }
}

int DatabaseTracker::DeleteDatabase(const string16& origin_identifier,
                                    const string16& database_name,
                                    net::CompletionCallback* callback) {
  if (!LazyInit())
    return net::ERR_FAILED;

  DCHECK(!callback ||
         deletion_callbacks_.find(callback) == deletion_callbacks_.end());

  if (database_connections_.IsDatabaseOpened(origin_identifier,
                                             database_name)) {
    if (callback)
      deletion_callbacks_[callback][origin_identifier].insert(database_name);
    ScheduleDatabaseForDeletion(origin_identifier, database_name);
    return net::ERR_IO_PENDING;
  }
  DeleteClosedDatabase(origin_identifier, database_name);
  return net::OK;
}

int DatabaseTracker::DeleteDataModifiedSince(
    const base::Time& cutoff,
    net::CompletionCallback* callback) {
  if (!LazyInit())
    return net::ERR_FAILED;

  DCHECK(!callback ||
         deletion_callbacks_.find(callback) == deletion_callbacks_.end());
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
      FilePath db_file = GetFullDBFilePath(*ori, db->database_name);
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

int DatabaseTracker::DeleteDataForOrigin(const string16& origin,
                                         net::CompletionCallback* callback) {
  if (!LazyInit())
    return net::ERR_FAILED;

  DCHECK(!callback ||
         deletion_callbacks_.find(callback) == deletion_callbacks_.end());
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

  FilePath incognito_db_dir =
      profile_path_.Append(kIncognitoDatabaseDirectoryName);
  if (file_util::DirectoryExists(incognito_db_dir))
    file_util::Delete(incognito_db_dir, true);
}

// static
void DatabaseTracker::ClearLocalState(const FilePath& profile_path) {
  FilePath db_dir = profile_path.Append(FilePath(kDatabaseDirectoryName));
  FilePath db_tracker = db_dir.Append(FilePath(kTrackerDatabaseFileName));
  if (file_util::DirectoryExists(db_dir) &&
      file_util::PathExists(db_tracker)) {
    scoped_ptr<sql::Connection> db_(new sql::Connection);
    if (!db_->Open(db_tracker) ||
        !db_->DoesTableExist("Databases")) {
      db_->Close();
      file_util::Delete(db_dir, true);
      return;
    } else {
      sql::Statement delete_statement(db_->GetCachedStatement(
            SQL_FROM_HERE, "DELETE FROM Databases WHERE origin NOT LIKE ?"));
      std::string filter(kExtensionOriginIdentifierPrefix);
      filter += "%";
      delete_statement.BindString(0, filter);
      if (!delete_statement.Run()) {
        db_->Close();
        file_util::Delete(db_dir, true);
        return;
      }
    }
  }
  file_util::FileEnumerator file_enumerator(db_dir, false,
      file_util::FileEnumerator::DIRECTORIES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.BaseName() != FilePath(kTrackerDatabaseFileName)) {
      if (!StartsWith(file_path.BaseName().ToWStringHack(),
                      ASCIIToWide(kExtensionOriginIdentifierPrefix), true))
        file_util::Delete(file_path, true);
    }
  }
}

}  // namespace webkit_database
