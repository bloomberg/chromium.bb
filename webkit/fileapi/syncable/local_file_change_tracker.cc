// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_change_tracker.h"

#include <queue>

#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
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
  SyncStatusCode GetDirtyEntries(std::queue<FileSystemURL>* dirty_files);

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

LocalFileChangeTracker::ChangeInfo::ChangeInfo() : change_seq(-1) {}
LocalFileChangeTracker::ChangeInfo::~ChangeInfo() {}

// LocalFileChangeTracker ------------------------------------------------------

LocalFileChangeTracker::LocalFileChangeTracker(
    const FilePath& base_path,
    base::SequencedTaskRunner* file_task_runner)
    : initialized_(false),
      file_task_runner_(file_task_runner),
      tracker_db_(new TrackerDB(base_path)),
      current_change_seq_(0),
      num_changes_(0) {
}

LocalFileChangeTracker::~LocalFileChangeTracker() {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  tracker_db_.reset();
}

void LocalFileChangeTracker::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  if (ContainsKey(changes_, url))
    return;
  // TODO(nhiroki): propagate the error code (see http://crbug.com/152127).
  MarkDirtyOnDatabase(url);
}

void LocalFileChangeTracker::OnEndUpdate(const FileSystemURL& url) {}

void LocalFileChangeTracker::OnCreateFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateFileFrom(const FileSystemURL& url,
                                              const FileSystemURL& src) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnRemoveFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnModifyFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_DIRECTORY));
}

void LocalFileChangeTracker::OnRemoveDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               SYNC_FILE_TYPE_DIRECTORY));
}

void LocalFileChangeTracker::GetNextChangedURLs(
    std::deque<FileSystemURL>* urls, int max_urls) {
  DCHECK(urls);
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  urls->clear();
  // Mildly prioritizes the URLs that older changes and have not been updated
  // for a while.
  for (ChangeSeqMap::iterator iter = change_seqs_.begin();
       iter != change_seqs_.end() &&
       (max_urls == 0 || urls->size() < static_cast<size_t>(max_urls));
       ++iter) {
    urls->push_back(iter->second);
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
  *changes = found->second.change_list;
}

void LocalFileChangeTracker::ClearChangesForURL(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  // TODO(nhiroki): propagate the error code (see http://crbug.com/152127).
  ClearDirtyOnDatabase(url);

  FileChangeMap::iterator found = changes_.find(url);
  if (found == changes_.end())
    return;
  change_seqs_.erase(found->second.change_seq);
  changes_.erase(found);
  UpdateNumChanges();
}

SyncStatusCode LocalFileChangeTracker::Initialize(
    FileSystemContext* file_system_context) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!initialized_);
  DCHECK(file_system_context);

  SyncStatusCode status = CollectLastDirtyChanges(file_system_context);
  if (status == SYNC_STATUS_OK)
    initialized_ = true;
  return status;
}

void LocalFileChangeTracker::UpdateNumChanges() {
  base::AutoLock lock(num_changes_lock_);
  num_changes_ = static_cast<int64>(change_seqs_.size());
}

void LocalFileChangeTracker::GetAllChangedURLs(FileSystemURLSet* urls) {
  std::deque<FileSystemURL> url_deque;
  GetNextChangedURLs(&url_deque, 0);
  urls->clear();
  urls->insert(url_deque.begin(), url_deque.end());
}

void LocalFileChangeTracker::DropAllChanges() {
  changes_.clear();
  change_seqs_.clear();
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

SyncStatusCode LocalFileChangeTracker::CollectLastDirtyChanges(
    FileSystemContext* file_system_context) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());

  std::queue<FileSystemURL> dirty_files;
  const SyncStatusCode status = tracker_db_->GetDirtyEntries(&dirty_files);
  if (status != SYNC_STATUS_OK)
    return status;

  FileSystemFileUtil* file_util =
      file_system_context->GetFileUtil(kFileSystemTypeSyncable);
  DCHECK(file_util);
  scoped_ptr<FileSystemOperationContext> context(
      new FileSystemOperationContext(file_system_context));

  base::PlatformFileInfo file_info;
  FilePath platform_path;

  while (!dirty_files.empty()) {
    const FileSystemURL url = dirty_files.front();
    dirty_files.pop();
    DCHECK_EQ(url.type(), kFileSystemTypeSyncable);

    switch (file_util->GetFileInfo(context.get(), url,
                                   &file_info, &platform_path)) {
      case base::PLATFORM_FILE_OK: {
        if (!file_info.is_directory) {
          RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                       SYNC_FILE_TYPE_FILE));
          break;
        }

        RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                     SYNC_FILE_TYPE_DIRECTORY));

        // Push files and directories in this directory into |dirty_files|.
        scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
            file_util->CreateFileEnumerator(context.get(),
                                            url,
                                            false /* recursive */));
        FilePath path_each;
        while (!(path_each = enumerator->Next()).empty()) {
          dirty_files.push(CreateSyncableFileSystemURL(
              url.origin(), url.filesystem_id(), path_each));
        }
        break;
      }
      case base::PLATFORM_FILE_ERROR_NOT_FOUND: {
        // File represented by |url| has already been deleted. Since we cannot
        // figure out if this file was directory or not from the URL, file
        // type is treated as SYNC_FILE_TYPE_UNKNOWN.
        //
        // NOTE: Directory to have been reverted (that is, ADD -> DELETE) is
        // also treated as FILE_CHANGE_DELETE.
        RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                                     SYNC_FILE_TYPE_UNKNOWN));
        break;
      }
      case base::PLATFORM_FILE_ERROR_FAILED:
      default:
        // TODO(nhiroki): handle file access error (http://crbug.com/155251).
        LOG(WARNING) << "Failed to access local file.";
        break;
    }
  }
  return SYNC_STATUS_OK;
}

void LocalFileChangeTracker::RecordChange(
    const FileSystemURL& url, const FileChange& change) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  ChangeInfo& info = changes_[url];
  if (info.change_seq >= 0)
    change_seqs_.erase(info.change_seq);
  info.change_list.Update(change);
  if (info.change_list.empty()) {
    changes_.erase(url);
    UpdateNumChanges();
    return;
  }
  info.change_seq = current_change_seq_++;
  change_seqs_[info.change_seq] = url;
  UpdateNumChanges();
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
  return SYNC_DATABASE_ERROR_FAILED;
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

SyncStatusCode LocalFileChangeTracker::TrackerDB::GetDirtyEntries(
    std::queue<FileSystemURL>* dirty_files) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  db_status_ = Init(REPAIR_ON_CORRUPTION);
  if (db_status_ != SYNC_STATUS_OK) {
    db_.reset();
    return db_status_;
  }

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(leveldb::ReadOptions()));
  iter->SeekToFirst();
  FileSystemURL url;
  while (iter->Valid()) {
    if (!DeserializeSyncableFileSystemURL(iter->key().ToString(), &url)) {
      LOG(WARNING) << "Failed to deserialize an URL. "
                   << "TrackerDB might be corrupted.";
      db_status_ = SYNC_DATABASE_ERROR_CORRUPTION;
      db_.reset();
      return db_status_;
    }
    dirty_files->push(url);
    iter->Next();
  }
  return SYNC_STATUS_OK;
}

}  // namespace fileapi
