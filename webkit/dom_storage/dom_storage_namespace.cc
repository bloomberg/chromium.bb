// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_namespace.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"

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
    DomStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id),
      task_runner_(task_runner) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
}

DomStorageNamespace::~DomStorageNamespace() {
}

DomStorageArea* DomStorageNamespace::OpenStorageArea(const GURL& origin) {
  if (AreaHolder* holder = GetAreaHolder(origin)) {
    ++(holder->open_count_);
    return holder->area_;
  }
  DomStorageArea* area = new DomStorageArea(namespace_id_, origin,
                                            directory_, task_runner_);
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

DomStorageNamespace* DomStorageNamespace::Clone(int64 clone_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, clone_namespace_id);
  DomStorageNamespace* clone =
      new DomStorageNamespace(clone_namespace_id, task_runner_);
  AreaMap::const_iterator it = areas_.begin();
  for (; it != areas_.end(); ++it) {
    DomStorageArea* area = it->second.area_->ShallowCopy(clone_namespace_id);
    clone->areas_[it->first] = AreaHolder(area, 0);
  }
  return clone;
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
