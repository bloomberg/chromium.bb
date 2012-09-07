// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_namespace.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/dom_storage/session_storage_database.h"

namespace dom_storage {

DomStorageNamespace::DomStorageNamespace(
    const FilePath& directory,
    DomStorageTaskRunner* task_runner)
    : namespace_id_(kLocalStorageNamespaceId),
      directory_(directory),
      task_runner_(task_runner) {
}

DomStorageNamespace::DomStorageNamespace(
    int64 namespace_id,
    const std::string& persistent_namespace_id,
    SessionStorageDatabase* session_storage_database,
    DomStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id),
      persistent_namespace_id_(persistent_namespace_id),
      task_runner_(task_runner),
      session_storage_database_(session_storage_database) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
}

DomStorageNamespace::~DomStorageNamespace() {
}

DomStorageArea* DomStorageNamespace::OpenStorageArea(const GURL& origin) {
  if (AreaHolder* holder = GetAreaHolder(origin)) {
    ++(holder->open_count_);
    return holder->area_;
  }
  DomStorageArea* area;
  if (namespace_id_ == kLocalStorageNamespaceId) {
    area = new DomStorageArea(origin, directory_, task_runner_);
  } else {
    area = new DomStorageArea(namespace_id_, persistent_namespace_id_, origin,
                              session_storage_database_, task_runner_);
  }
  areas_[origin] = AreaHolder(area, 1);
  return area;
}

void DomStorageNamespace::CloseStorageArea(DomStorageArea* area) {
  AreaHolder* holder = GetAreaHolder(area->origin());
  DCHECK(holder);
  DCHECK_EQ(holder->area_.get(), area);
  --(holder->open_count_);
  // TODO(michaeln): Clean up areas that aren't needed in memory anymore.
  // The in-process-webkit based impl didn't do this either, but would be nice.
}

DomStorageArea* DomStorageNamespace::GetOpenStorageArea(const GURL& origin) {
  AreaHolder* holder = GetAreaHolder(origin);
  if (holder && holder->open_count_)
    return holder->area_;
  return NULL;
}

DomStorageNamespace* DomStorageNamespace::Clone(
    int64 clone_namespace_id,
    const std::string& clone_persistent_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, clone_namespace_id);
  DomStorageNamespace* clone = new DomStorageNamespace(
      clone_namespace_id, clone_persistent_namespace_id,
      session_storage_database_, task_runner_);
  AreaMap::const_iterator it = areas_.begin();
  // Clone the in-memory structures.
  for (; it != areas_.end(); ++it) {
    DomStorageArea* area = it->second.area_->ShallowCopy(
        clone_namespace_id, clone_persistent_namespace_id);
    clone->areas_[it->first] = AreaHolder(area, 0);
  }
  // And clone the on-disk structures, too.
  if (session_storage_database_.get()) {
    task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DomStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(base::IgnoreResult(&SessionStorageDatabase::CloneNamespace),
                   session_storage_database_.get(), persistent_namespace_id_,
                   clone_persistent_namespace_id));
  }
  return clone;
}

void DomStorageNamespace::DeleteOrigin(const GURL& origin) {
  DCHECK(!session_storage_database_.get());
  AreaHolder* holder = GetAreaHolder(origin);
  if (holder) {
    holder->area_->DeleteOrigin();
    return;
  }
  if (!directory_.empty()) {
    scoped_refptr<DomStorageArea> area =
        new DomStorageArea(origin, directory_, task_runner_);
    area->DeleteOrigin();
  }
}

void DomStorageNamespace::PurgeMemory() {
  if (directory_.empty())
    return;  // We can't purge w/o backing on disk.
  AreaMap::iterator it = areas_.begin();
  while (it != areas_.end()) {
    // Leave it alone if changes are pending
    if (it->second.area_->HasUncommittedChanges()) {
      ++it;
      continue;
    }

    // If not in use, we can shut it down and remove
    // it from our collection entirely.
    if (it->second.open_count_ == 0) {
      it->second.area_->Shutdown();
      areas_.erase(it++);
      continue;
    }

    // Otherwise, we can clear caches and such.
    it->second.area_->PurgeMemory();
    ++it;
  }
}

void DomStorageNamespace::Shutdown() {
  AreaMap::const_iterator it = areas_.begin();
  for (; it != areas_.end(); ++it)
    it->second.area_->Shutdown();
}


DomStorageNamespace::AreaHolder*
DomStorageNamespace::GetAreaHolder(const GURL& origin) {
  AreaMap::iterator found = areas_.find(origin);
  if (found == areas_.end())
    return NULL;
  return &(found->second);
}

// AreaHolder

DomStorageNamespace::AreaHolder::AreaHolder()
    : open_count_(0) {
}

DomStorageNamespace::AreaHolder::AreaHolder(
    DomStorageArea* area, int count)
    : area_(area), open_count_(count) {
}

DomStorageNamespace::AreaHolder::~AreaHolder() {
}

}  // namespace dom_storage
