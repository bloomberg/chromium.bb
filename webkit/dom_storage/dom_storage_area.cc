// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_area.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/dom_storage/dom_storage_map.h"
#include "webkit/dom_storage/dom_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/glue/webkit_glue.h"

using webkit_database::DatabaseUtil;

namespace dom_storage {

static const int kCommitTimerSeconds = 1;

DomStorageArea::CommitBatch::CommitBatch()
  : clear_all_first(false) {
}
DomStorageArea::CommitBatch::~CommitBatch() {}


// static
const FilePath::CharType DomStorageArea::kDatabaseFileExtension[] =
    FILE_PATH_LITERAL(".localstorage");

// static
FilePath DomStorageArea::DatabaseFileNameFromOrigin(const GURL& origin) {
  std::string filename = fileapi::GetOriginIdentifierFromURL(origin);
  // There is no FilePath.AppendExtension() method, so start with just the
  // extension as the filename, and then InsertBeforeExtension the desired
  // name.
  return FilePath().Append(kDatabaseFileExtension).
      InsertBeforeExtensionASCII(filename);
}

// static
GURL DomStorageArea::OriginFromDatabaseFileName(const FilePath& name) {
  DCHECK(name.MatchesExtension(kDatabaseFileExtension));
  WebKit::WebString origin_id = webkit_glue::FilePathToWebString(
      name.BaseName().RemoveExtension());
  return DatabaseUtil::GetOriginFromIdentifier(origin_id);
}

DomStorageArea::DomStorageArea(
    int64 namespace_id, const GURL& origin,
    const FilePath& directory, DomStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id), origin_(origin),
      directory_(directory),
      task_runner_(task_runner),
      map_(new DomStorageMap(kPerAreaQuota)),
      is_initial_import_done_(true),
      is_shutdown_(false),
      commit_batches_in_flight_(0) {
  if (namespace_id == kLocalStorageNamespaceId && !directory.empty()) {
    FilePath path = directory.Append(DatabaseFileNameFromOrigin(origin_));
    backing_.reset(new DomStorageDatabase(path));
    is_initial_import_done_ = false;
  }
}

DomStorageArea::~DomStorageArea() {
}

void DomStorageArea::ExtractValues(ValuesMap* map) {
  if (is_shutdown_)
    return;
  InitialImportIfNeeded();
  map_->ExtractValues(map);
}

unsigned DomStorageArea::Length() {
  if (is_shutdown_)
    return 0;
  InitialImportIfNeeded();
  return map_->Length();
}

NullableString16 DomStorageArea::Key(unsigned index) {
  if (is_shutdown_)
    return NullableString16(true);
  InitialImportIfNeeded();
  return map_->Key(index);
}

NullableString16 DomStorageArea::GetItem(const string16& key) {
  if (is_shutdown_)
    return NullableString16(true);
  InitialImportIfNeeded();
  return map_->GetItem(key);
}

bool DomStorageArea::SetItem(const string16& key,
                             const string16& value,
                             NullableString16* old_value) {
  if (is_shutdown_)
    return false;
  InitialImportIfNeeded();
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  bool success = map_->SetItem(key, value, old_value);
  if (success && backing_.get()) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->changed_values[key] = NullableString16(value, false);
  }
  return success;
}

bool DomStorageArea::RemoveItem(const string16& key, string16* old_value) {
  if (is_shutdown_)
    return false;
  InitialImportIfNeeded();
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  bool success = map_->RemoveItem(key, old_value);
  if (success && backing_.get()) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->changed_values[key] = NullableString16(true);
  }
  return success;
}

bool DomStorageArea::Clear() {
  if (is_shutdown_)
    return false;
  InitialImportIfNeeded();
  if (map_->Length() == 0)
    return false;

  map_ = new DomStorageMap(kPerAreaQuota);

  if (backing_.get()) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->clear_all_first = true;
    commit_batch->changed_values.clear();
  }

  return true;
}

DomStorageArea* DomStorageArea::ShallowCopy(int64 destination_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, destination_namespace_id);
  DCHECK(!backing_.get());  // SessionNamespaces aren't stored on disk.

  DomStorageArea* copy = new DomStorageArea(destination_namespace_id, origin_,
                                            FilePath(), task_runner_);
  copy->map_ = map_;
  copy->is_shutdown_ = is_shutdown_;
  return copy;
}

bool DomStorageArea::HasUncommittedChanges() const {
  DCHECK(!is_shutdown_);
  return commit_batch_.get() || commit_batches_in_flight_;
}

void DomStorageArea::DeleteOrigin() {
  DCHECK(!is_shutdown_);
  if (HasUncommittedChanges()) {
    // TODO(michaeln): This logically deletes the data immediately,
    // and in a matter of a second, deletes the rows from the backing
    // database file, but the file itself will linger until shutdown
    // or purge time. Ideally, this should delete the file more
    // quickly.
    Clear();
    return;
  }
  map_ = new DomStorageMap(kPerAreaQuota);
  if (backing_.get()) {
    is_initial_import_done_ = false;
    backing_.reset(new DomStorageDatabase(backing_->file_path()));
    file_util::Delete(backing_->file_path(), false);
    file_util::Delete(
        DomStorageDatabase::GetJournalFilePath(backing_->file_path()), false);
  }
}

void DomStorageArea::PurgeMemory() {
  DCHECK(!is_shutdown_);
  if (!is_initial_import_done_ ||  // We're not using any memory.
      !backing_.get() ||  // We can't purge anything.
      HasUncommittedChanges())  // We leave things alone with changes pending.
    return;

  // Drop the in memory cache, we'll reload when needed.
  is_initial_import_done_ = false;
  map_ = new DomStorageMap(kPerAreaQuota);

  // Recreate the database object, this frees up the open sqlite connection
  // and its page cache.
  backing_.reset(new DomStorageDatabase(backing_->file_path()));
}

void DomStorageArea::Shutdown() {
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;
  map_ = NULL;
  if (!backing_.get())
    return;

  bool success = task_runner_->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::COMMIT_SEQUENCE,
      base::Bind(&DomStorageArea::ShutdownInCommitSequence, this));
  DCHECK(success);
}

void DomStorageArea::InitialImportIfNeeded() {
  if (is_initial_import_done_)
    return;

  DCHECK_EQ(kLocalStorageNamespaceId, namespace_id_);
  DCHECK(backing_.get());

  ValuesMap initial_values;
  backing_->ReadAllValues(&initial_values);
  map_->SwapValues(&initial_values);
  is_initial_import_done_ = true;
}

DomStorageArea::CommitBatch* DomStorageArea::CreateCommitBatchIfNeeded() {
  DCHECK(!is_shutdown_);
  if (!commit_batch_.get()) {
    commit_batch_.reset(new CommitBatch());

    // Start a timer to commit any changes that accrue in the batch, but only if
    // no commits are currently in flight. In that case the timer will be
    // started after the commits have happened.
    if (!commit_batches_in_flight_) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&DomStorageArea::OnCommitTimer, this),
          base::TimeDelta::FromSeconds(kCommitTimerSeconds));
    }
  }
  return commit_batch_.get();
}

void DomStorageArea::OnCommitTimer() {
  DCHECK_EQ(kLocalStorageNamespaceId, namespace_id_);
  if (is_shutdown_)
    return;

  DCHECK(backing_.get());
  DCHECK(commit_batch_.get());
  DCHECK(!commit_batches_in_flight_);

  // This method executes on the primary sequence, we schedule
  // a task for immediate execution on the commit sequence.
  DCHECK(task_runner_->IsRunningOnPrimarySequence());
  bool success = task_runner_->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::COMMIT_SEQUENCE,
      base::Bind(&DomStorageArea::CommitChanges, this,
                 base::Owned(commit_batch_.release())));
  ++commit_batches_in_flight_;
  DCHECK(success);
}

void DomStorageArea::CommitChanges(const CommitBatch* commit_batch) {
  // This method executes on the commit sequence.
  DCHECK(task_runner_->IsRunningOnCommitSequence());
  bool success = backing_->CommitChanges(commit_batch->clear_all_first,
                                         commit_batch->changed_values);
  DCHECK(success);  // TODO(michaeln): what if it fails?
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DomStorageArea::OnCommitComplete, this));
}

void DomStorageArea::OnCommitComplete() {
  // We're back on the primary sequence in this method.
  DCHECK(task_runner_->IsRunningOnPrimarySequence());
  if (is_shutdown_)
    return;
  --commit_batches_in_flight_;
  if (commit_batch_.get() && !commit_batches_in_flight_) {
    // More changes have accrued, restart the timer.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DomStorageArea::OnCommitTimer, this),
        base::TimeDelta::FromSeconds(kCommitTimerSeconds));
  }
}

void DomStorageArea::ShutdownInCommitSequence() {
  // This method executes on the commit sequence.
  DCHECK(task_runner_->IsRunningOnCommitSequence());
  DCHECK(backing_.get());
  if (commit_batch_.get()) {
    // Commit any changes that accrued prior to the timer firing.
    bool success = backing_->CommitChanges(
        commit_batch_->clear_all_first,
        commit_batch_->changed_values);
    DCHECK(success);
  }
  commit_batch_.reset();
  backing_.reset();
}

}  // namespace dom_storage
