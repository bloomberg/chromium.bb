// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_area.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "webkit/dom_storage/dom_storage_map.h"
#include "webkit/dom_storage/dom_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/fileapi/file_system_util.h"

namespace dom_storage {

DomStorageArea::DomStorageArea(
    int64 namespace_id, const GURL& origin,
    const FilePath& directory, DomStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id), origin_(origin),
      directory_(directory),
      task_runner_(task_runner),
      map_(new DomStorageMap(kPerAreaQuota)),
      backing_(NULL),
      initial_import_done_(false),
      clear_all_next_commit_(false),
      commit_in_flight_(false) {

  if (namespace_id == kLocalStorageNamespaceId && !directory.empty()) {
    FilePath path = directory.Append(DatabaseFileNameFromOrigin(origin_));
    backing_.reset(new DomStorageDatabase(path));
  } else {
    // Not a local storage area or no directory specified for backing
    // database, (i.e. it's an incognito profile).
    initial_import_done_ = true;
  }
}

DomStorageArea::~DomStorageArea() {
  if (clear_all_next_commit_ || !changed_values_.empty()) {
    // Still some data left that was not committed to disk, try now.
    // We do this regardless of whether we think a commit is in flight
    // as there is no guarantee that that commit will actually get
    // processed. For example the task_runner_'s message loop could
    // unexpectedly quit before the delayed task is fired and leave the
    // commit_in_flight_ flag set. But there's no way for us to determine
    // that has happened so force a commit now.

    CommitChanges();

    // TODO(benm): It's possible that the commit failed, and in
    // that case we're going to lose data. Integrate with UMA
    // to gather stats about how often this actually happens,
    // so that we can figure out a contingency plan.
  }
}

unsigned DomStorageArea::Length() {
  InitialImportIfNeeded();
  return map_->Length();
}

NullableString16 DomStorageArea::Key(unsigned index) {
  InitialImportIfNeeded();
  return map_->Key(index);
}

NullableString16 DomStorageArea::GetItem(const string16& key) {
  InitialImportIfNeeded();
  return map_->GetItem(key);
}

bool DomStorageArea::SetItem(const string16& key,
                             const string16& value,
                             NullableString16* old_value) {
  InitialImportIfNeeded();

  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  bool success = map_->SetItem(key, value, old_value);
  if (success && backing_.get()) {
    changed_values_[key] = NullableString16(value, false);
    ScheduleCommitChanges();
  }
  return success;
}

bool DomStorageArea::RemoveItem(const string16& key, string16* old_value) {
  InitialImportIfNeeded();
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  bool success = map_->RemoveItem(key, old_value);
  if (success && backing_.get()) {
    changed_values_[key] = NullableString16(true);
    ScheduleCommitChanges();
  }
  return success;
}

bool DomStorageArea::Clear() {
  InitialImportIfNeeded();
  if (map_->Length() == 0)
    return false;

  map_ = new DomStorageMap(kPerAreaQuota);

  if (backing_.get()) {
    changed_values_.clear();
    clear_all_next_commit_ = true;
    ScheduleCommitChanges();
  }

  return true;
}

DomStorageArea* DomStorageArea::ShallowCopy(int64 destination_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, destination_namespace_id);
  // SessionNamespaces aren't backed by files on disk.
  DCHECK(!backing_.get());

  DomStorageArea* copy = new DomStorageArea(destination_namespace_id, origin_,
                                            FilePath(), task_runner_);
  copy->map_ = map_;
  return copy;
}

void DomStorageArea::InitialImportIfNeeded() {
  if (initial_import_done_)
    return;

  DCHECK_EQ(kLocalStorageNamespaceId, namespace_id_);
  DCHECK(backing_.get());

  ValuesMap initial_values;
  backing_->ReadAllValues(&initial_values);
  map_->SwapValues(&initial_values);
  initial_import_done_ = true;
}

void DomStorageArea::ScheduleCommitChanges() {
  DCHECK_EQ(kLocalStorageNamespaceId, namespace_id_);
  DCHECK(backing_.get());
  DCHECK(clear_all_next_commit_ || !changed_values_.empty());
  DCHECK(task_runner_.get());

  if (commit_in_flight_)
    return;

  commit_in_flight_ = task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&DomStorageArea::CommitChanges, this),
      base::TimeDelta::FromSeconds(1));
  DCHECK(commit_in_flight_);
}

void DomStorageArea::CommitChanges() {
  DCHECK(backing_.get());
  if (backing_->CommitChanges(clear_all_next_commit_, changed_values_)) {
    clear_all_next_commit_ = false;
    changed_values_.clear();
  }
  commit_in_flight_ = false;
}

// static
FilePath DomStorageArea::DatabaseFileNameFromOrigin(const GURL& origin) {
  std::string filename = fileapi::GetOriginIdentifierFromURL(origin)
      + ".localstorage";
  return FilePath().AppendASCII(filename);
}

}  // namespace dom_storage
