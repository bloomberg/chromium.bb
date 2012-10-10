// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_change_tracker.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace fileapi {

namespace {
const FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("LocalFileChangeTracker");
const char kMark[] = "d";
}  // namespace

// A database class that stores local file changes in a local database. This
// object must be destructed on file_task_runner.
class LocalFileChangeTracker::TrackerDB {
 public:
  explicit TrackerDB(const FilePath& base_path);

  SyncStatusCode MarkDirty(const std::string& url);
  SyncStatusCode ClearDirty(const std::string& url);

 private:
  enum RecoveryOption {
    REPAIR_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  SyncStatusCode Init(RecoveryOption recovery_option);
  SyncStatusCode Repair(const std::string& db_path);
  void HandleError(const tracked_objects::Location& from_here,
                   const leveldb::Status& status);

  const FilePath base_path_;
  scoped_ptr<leveldb::DB> db_;
  SyncStatusCode db_status_;

  DISALLOW_COPY_AND_ASSIGN(TrackerDB);
};

// LocalFileChangeTracker ------------------------------------------------------

LocalFileChangeTracker::LocalFileChangeTracker(
    const FilePath& base_path,
    base::SequencedTaskRunner* file_task_runner)
    : file_task_runner_(file_task_runner),
      tracker_db_(new TrackerDB(base_path)) {}

LocalFileChangeTracker::~LocalFileChangeTracker() {
  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    file_task_runner_->DeleteSoon(FROM_HERE, tracker_db_.release());
    return;
  }
  tracker_db_.reset();
}

void LocalFileChangeTracker::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  // TODO(kinuko): we may want to reduce the number of this call if
  // the URL is already marked dirty.
  // TODO(nhiroki): propagate the error code (see http://crbug.com/152127).
  MarkDirtyOnDatabase(url);
}

void LocalFileChangeTracker::OnEndUpdate(const FileSystemURL& url) {}

void LocalFileChangeTracker::OnCreateFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateFileFrom(const FileSystemURL& url,
                                              const FileSystemURL& src) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnRemoveFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnModifyFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               FileChange::FILE_TYPE_DIRECTORY));
}

void LocalFileChangeTracker::OnRemoveDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               FileChange::FILE_TYPE_DIRECTORY));
}

void LocalFileChangeTracker::GetChangedURLs(std::vector<FileSystemURL>* urls) {
  DCHECK(urls);
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  urls->clear();
  FileChangeMap::iterator iter = changes_.begin();
  while (iter != changes_.end()) {
    if (iter->second.empty())
      changes_.erase(iter++);
    else
      urls->push_back(iter++->first);
  }
}

void LocalFileChangeTracker::GetChangesForURL(
    const FileSystemURL& url, FileChangeList* changes) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(changes);
  changes->clear();
  FileChangeMap::iterator found = changes_.find(url);
  if (found == changes_.end())
    return;
  *changes = found->second;
}

void LocalFileChangeTracker::FinalizeSyncForURL(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  // TODO(nhiroki): propagate the error code (see http://crbug.com/152127).
  ClearDirtyOnDatabase(url);
  changes_.erase(url);
}

SyncStatusCode LocalFileChangeTracker::CollectLastDirtyChanges(
    FileChangeMap* changes) {
  // TODO(kinuko): implement.
  NOTREACHED();
  return SYNC_DATABASE_ERROR_NOT_FOUND;
}

void LocalFileChangeTracker::RecordChange(
    const FileSystemURL& url, const FileChange& change) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  changes_[url].Update(change);
}

SyncStatusCode LocalFileChangeTracker::MarkDirtyOnDatabase(
    const FileSystemURL& url) {
  std::string serialized_url;
  if (!SerializeSyncableFileSystemURL(url, &serialized_url))
    return SYNC_FILE_ERROR_INVALID_URL;

  return tracker_db_->MarkDirty(serialized_url);
}

SyncStatusCode LocalFileChangeTracker::ClearDirtyOnDatabase(
    const FileSystemURL& url) {
  std::string serialized_url;
  if (!SerializeSyncableFileSystemURL(url, &serialized_url))
    return SYNC_FILE_ERROR_INVALID_URL;

  return tracker_db_->ClearDirty(serialized_url);
}

// TrackerDB -------------------------------------------------------------------

LocalFileChangeTracker::TrackerDB::TrackerDB(const FilePath& base_path)
  : base_path_(base_path),
    db_status_(SYNC_STATUS_OK) {}

SyncStatusCode LocalFileChangeTracker::TrackerDB::Init(
    RecoveryOption recovery_option) {
  if (db_.get() && db_status_ == SYNC_STATUS_OK)
    return SYNC_STATUS_OK;

  std::string path = FilePathToString(base_path_.Append(kDatabaseName));
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (status.ok()) {
    db_.reset(db);
    return SYNC_STATUS_OK;
  }

  HandleError(FROM_HERE, status);
  if (!status.IsCorruption())
    return LevelDBStatusToSyncStatusCode(status);

  // Try to repair the corrupted DB.
  switch (recovery_option) {
    case FAIL_ON_CORRUPTION:
      return SYNC_DATABASE_ERROR_CORRUPTION;
    case REPAIR_ON_CORRUPTION:
      return Repair(path);
  }
  NOTREACHED();
  return SYNC_DATABASE_ERROR_UNKNOWN;
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::Repair(
    const std::string& db_path) {
  DCHECK(!db_.get());
  LOG(WARNING) << "Attempting to repair TrackerDB.";

  if (leveldb::RepairDB(db_path, leveldb::Options()).ok() &&
      Init(FAIL_ON_CORRUPTION) == SYNC_STATUS_OK) {
    // TODO(nhiroki): perform some consistency checks between TrackerDB and
    // syncable file system.
    LOG(WARNING) << "Repairing TrackerDB completed.";
    return SYNC_STATUS_OK;
  }

  LOG(WARNING) << "Failed to repair TrackerDB.";
  return SYNC_DATABASE_ERROR_CORRUPTION;
}

// TODO(nhiroki): factor out the common methods into somewhere else.
void LocalFileChangeTracker::TrackerDB::HandleError(
    const tracked_objects::Location& from_here,
    const leveldb::Status& status) {
  LOG(ERROR) << "LocalFileChangeTracker::TrackerDB failed at: "
             << from_here.ToString() << " with error: " << status.ToString();
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::MarkDirty(
    const std::string& url) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  db_status_ = Init(REPAIR_ON_CORRUPTION);
  if (db_status_ != SYNC_STATUS_OK) {
    db_.reset();
    return db_status_;
  }

  leveldb::Status status = db_->Put(leveldb::WriteOptions(), url, kMark);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    db_status_ = LevelDBStatusToSyncStatusCode(status);
    db_.reset();
    return db_status_;
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::ClearDirty(
    const std::string& url) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  // Should not reach here before initializing the database. The database should
  // be cleared after read, and should be initialized during read if
  // uninitialized.
  DCHECK(db_.get());

  leveldb::Status status = db_->Delete(leveldb::WriteOptions(), url);
  if (!status.ok() && !status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    db_status_ = LevelDBStatusToSyncStatusCode(status);
    db_.reset();
    return db_status_;
  }
  return SYNC_STATUS_OK;
}

}  // namespace fileapi
