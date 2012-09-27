// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_change_tracker.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"

namespace {
const FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("LocalFileChangeTracker");
const char kMark[] = "d";
}  // namespace

namespace fileapi {

// A database class that stores local file changes in a local database. This
// object must be destructed on file_task_runner.
class LocalFileChangeTracker::TrackerDB {
 public:
  explicit TrackerDB(const FilePath& profile_path);

  bool MarkDirty(const std::string& url);
  bool ClearDirty(const std::string& url);

 private:
  enum InitStatus {
    INIT_STATUS_OK = 0,
    INIT_STATUS_CORRUPTION,
    INIT_STATUS_IO_ERROR,
    INIT_STATUS_UNKNOWN_ERROR,
    INIT_STATUS_MAX,
  };

  enum RecoveryOption {
    REPAIR_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  bool Init(RecoveryOption recovery_option);
  bool Repair(const std::string& db_path);
  void HandleError(const tracked_objects::Location& from_here,
                   const leveldb::Status& status);

  const FilePath profile_path_;
  scoped_ptr<leveldb::DB> db_;
  bool db_disabled_;

  DISALLOW_COPY_AND_ASSIGN(TrackerDB);
};

// LocalFileChangeTracker ------------------------------------------------------

LocalFileChangeTracker::LocalFileChangeTracker(
    LocalFileSyncStatus* sync_status,
    const FilePath& profile_path,
    base::SequencedTaskRunner* file_task_runner)
    : sync_status_(sync_status),
      file_task_runner_(file_task_runner),
      tracker_db_(new TrackerDB(profile_path)) {}

LocalFileChangeTracker::~LocalFileChangeTracker() {
  if (tracker_db_.get())
    file_task_runner_->DeleteSoon(FROM_HERE, tracker_db_.release());
}

void LocalFileChangeTracker::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  // TODO(kinuko): we may want to reduce the number of this call if
  // the URL is already marked dirty.
  MarkDirtyOnDatabase(url);
}

void LocalFileChangeTracker::OnEndUpdate(const FileSystemURL& url) {
  // TODO(kinuko): implement. Probably we could call
  // sync_status_->DecrementWriting() here.
}

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
  DCHECK(!sync_status_->IsWritable(url));
  DCHECK(changes);
  changes->clear();
  FileChangeMap::iterator found = changes_.find(url);
  if (found == changes_.end())
    return;
  *changes = found->second;
}

void LocalFileChangeTracker::FinalizeSyncForURL(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  ClearDirtyOnDatabase(url);
  changes_.erase(url);
  sync_status_->EnableWriting(url);
}

void LocalFileChangeTracker::CollectLastDirtyChanges(FileChangeMap* changes) {
  // TODO(kinuko): implement.
  NOTREACHED();
}

void LocalFileChangeTracker::RecordChange(
    const FileSystemURL& url, const FileChange& change) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  // TODO(nhiroki): propagate the error code (see http://crbug.com/152127).
  MarkDirtyOnDatabase(url);
  changes_[url].Update(change);
}

void LocalFileChangeTracker::MarkDirtyOnDatabase(const FileSystemURL& url) {
  tracker_db_->MarkDirty(SerializeExternalFileSystemURL(url));
}

void LocalFileChangeTracker::ClearDirtyOnDatabase(const FileSystemURL& url) {
  tracker_db_->ClearDirty(SerializeExternalFileSystemURL(url));
}

std::string LocalFileChangeTracker::SerializeExternalFileSystemURL(
    const FileSystemURL& url) {
  return GetFileSystemRootURI(url.origin(), kFileSystemTypeExternal).spec() +
      url.filesystem_id() + "/" + url.path().AsUTF8Unsafe();
}

bool LocalFileChangeTracker::DeserializeExternalFileSystemURL(
    const std::string& serialized_url, FileSystemURL* url) {
  *url = FileSystemURL(GURL(serialized_url));
  return url->is_valid();
}

// TrackerDB -------------------------------------------------------------------

LocalFileChangeTracker::TrackerDB::TrackerDB(const FilePath& profile_path)
  : profile_path_(profile_path),
    db_disabled_(false) {}

bool LocalFileChangeTracker::TrackerDB::Init(RecoveryOption recovery_option) {
  if (db_.get())
    return true;

  std::string path = FilePathToString(profile_path_.Append(kDatabaseName));
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  if (status.ok()) {
    db_.reset(db);
    return true;
  }

  HandleError(FROM_HERE, status);
  if (!status.IsCorruption())
    return false;

  switch (recovery_option) {
    case FAIL_ON_CORRUPTION:
      return false;
    case REPAIR_ON_CORRUPTION:
      return Repair(path);
  }
  NOTREACHED();
  return false;
}

bool LocalFileChangeTracker::TrackerDB::Repair(const std::string& db_path) {
  DCHECK(!db_.get());
  LOG(WARNING) << "Attempting to repair TrackerDB.";

  if (leveldb::RepairDB(db_path, leveldb::Options()).ok() &&
      Init(FAIL_ON_CORRUPTION)) {
    // TODO(nhiroki): perform some consistency checks between TrackerDB and
    // syncable file system.
    LOG(WARNING) << "Repairing TrackerDB completed.";
    return true;
  }

  LOG(WARNING) << "Failed to repair TrackerDB.";
  return false;
}

// TODO(nhiroki): factor out the common methods into somewhere else.
void LocalFileChangeTracker::TrackerDB::HandleError(
    const tracked_objects::Location& from_here,
    const leveldb::Status& status) {
  LOG(ERROR) << "LocalFileChangeTracker::TrackerDB failed at: "
             << from_here.ToString() << " with error: " << status.ToString();
}

bool LocalFileChangeTracker::TrackerDB::MarkDirty(const std::string& url) {
  if (db_disabled_)
    return false;

  if (!Init(REPAIR_ON_CORRUPTION)) {
    db_disabled_ = true;
    db_.reset();
    return false;
  }

  leveldb::Status status = db_->Put(leveldb::WriteOptions(), url, kMark);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    db_disabled_ = true;
    db_.reset();
    return false;
  }
  return true;
}

bool LocalFileChangeTracker::TrackerDB::ClearDirty(const std::string& url) {
  if (db_disabled_)
    return false;

  // Should not reach here before initializing the database. The database should
  // be cleared after read, and should be initialized during read if
  // uninitialized.
  DCHECK(db_.get());

  leveldb::Status status = db_->Delete(leveldb::WriteOptions(), url);
  if (!status.ok() && !status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    db_disabled_ = true;
    db_.reset();
    return false;
  }
  return true;
}

}  // namespace fileapi
