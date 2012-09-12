// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_change_tracker.h"

#include "base/sequenced_task_runner.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"

namespace fileapi {

LocalFileChangeTracker::LocalFileChangeTracker(
    LocalFileSyncStatus* sync_status,
    base::SequencedTaskRunner* file_task_runner)
    : sync_status_(sync_status),
      file_task_runner_(file_task_runner) {}

LocalFileChangeTracker::~LocalFileChangeTracker() {
  // file_task_runner_->PostTask(FROM_HERE, base::Bind(&DropDatabase));
  // TODO(kinuko): implement.
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
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateFileFrom(const FileSystemURL& url,
                                              const FileSystemURL& src) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnRemoveFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnModifyFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_UPDATE,
                               FileChange::FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD,
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
  changes_[url].Update(change);
}

void LocalFileChangeTracker::MarkDirtyOnDatabase(const FileSystemURL& url) {
  // TODO(kinuko): implement.
}

void LocalFileChangeTracker::ClearDirtyOnDatabase(const FileSystemURL& url) {
  // TODO(kinuko): implement.
}

}  // namespace fileapi
