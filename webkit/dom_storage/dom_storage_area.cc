// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_map.h"
#include "webkit/dom_storage/dom_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

DomStorageArea::DomStorageArea(
    int64 namespace_id, const GURL& origin,
    const FilePath& directory, DomStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id),
      origin_(origin),
      directory_(directory),
      task_runner_(task_runner),
      map_(new DomStorageMap(kPerAreaQuota)) {
}

DomStorageArea::~DomStorageArea() {
}

unsigned DomStorageArea::Length() {
  return map_->Length();
}

NullableString16 DomStorageArea::Key(unsigned index) {
  return map_->Key(index);
}

NullableString16 DomStorageArea::GetItem(const string16& key) {
  return map_->GetItem(key);
}

bool DomStorageArea::SetItem(
    const string16& key, const string16& value,
    NullableString16* old_value) {
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  return map_->SetItem(key, value, old_value);
}

bool DomStorageArea::RemoveItem(
    const string16& key,
    string16* old_value) {
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  return map_->RemoveItem(key, old_value);
}

bool DomStorageArea::Clear() {
  if (map_->Length() == 0)
    return false;
  map_ = new DomStorageMap(kPerAreaQuota);
  return true;
}

DomStorageArea* DomStorageArea::ShallowCopy(int64 destination_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, destination_namespace_id);
  // SessionNamespaces aren't backed by files on disk.
  DomStorageArea* copy = new DomStorageArea(destination_namespace_id, origin_,
                                            FilePath(), task_runner_);
  copy->map_ = map_;
  return copy;
}

}  // namespace dom_storage
