// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_host.h"

#include "googleurl/src/gurl.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

DomStorageHost::DomStorageHost(DomStorageContext* context)
    : context_(context) {
}

DomStorageHost::~DomStorageHost() {
  AreaMap::const_iterator it = connections_.begin();
  for (; it != connections_.end(); ++it)
    it->second.namespace_->CloseStorageArea(it->second.area_);
  connections_.clear();  // Clear prior to releasing the context_
}

bool DomStorageHost::OpenStorageArea(int connection_id, int namespace_id,
                                     const GURL& origin) {
  DCHECK(!GetOpenArea(connection_id));
  if (GetOpenArea(connection_id))
    return false;  // Indicates the renderer gave us very bad data.
  NamespaceAndArea references;
  references.namespace_ = context_->GetStorageNamespace(namespace_id);
  if (!references.namespace_) {
    // TODO(michaeln): Fix crbug/134003 and return false here.
    // Until then return true to avoid crashing the renderer for
    // sending a bad message.
    return true;
  }
  references.area_ = references.namespace_->OpenStorageArea(origin);
  DCHECK(references.area_);
  connections_[connection_id] = references;
  return true;
}

void DomStorageHost::CloseStorageArea(int connection_id) {
  AreaMap::iterator found = connections_.find(connection_id);
  if (found == connections_.end())
    return;
  found->second.namespace_->CloseStorageArea(
      found->second.area_);
  connections_.erase(found);
}

bool DomStorageHost::ExtractAreaValues(
    int connection_id, ValuesMap* map) {
  map->clear();
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area) {
    // TODO(michaeln): Fix crbug/134003 and return false here.
    // Until then return true to avoid crashing the renderer
    // for sending a bad message.
    return true;
  }
  area->ExtractValues(map);
  return true;
}

unsigned DomStorageHost::GetAreaLength(int connection_id) {
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return 0;
  return area->Length();
}

NullableString16 DomStorageHost::GetAreaKey(int connection_id, unsigned index) {
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return NullableString16(true);
  return area->Key(index);
}

NullableString16 DomStorageHost::GetAreaItem(int connection_id,
                                             const base::string16& key) {
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return NullableString16(true);
  return area->GetItem(key);
}

bool DomStorageHost::SetAreaItem(
    int connection_id, const base::string16& key,
    const base::string16& value, const GURL& page_url,
    NullableString16* old_value) {
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area) {
    // TODO(michaeln): Fix crbug/134003 and return false here.
    // Until then return true to allow the renderer to operate
    // to a limited degree out of its cache.
    return true;
  }
  if (!area->SetItem(key, value, old_value))
    return false;
  if (old_value->is_null() || old_value->string() != value)
    context_->NotifyItemSet(area, key, value, *old_value, page_url);
  return true;
}

bool DomStorageHost::RemoveAreaItem(
    int connection_id, const base::string16& key, const GURL& page_url,
    base::string16* old_value) {
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return false;
  if (!area->RemoveItem(key, old_value))
    return false;
  context_->NotifyItemRemoved(area, key, *old_value, page_url);
  return true;
}

bool DomStorageHost::ClearArea(int connection_id, const GURL& page_url) {
  DomStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return false;
  if (!area->Clear())
    return false;
  context_->NotifyAreaCleared(area, page_url);
  return true;
}

bool DomStorageHost::HasAreaOpen(
    int namespace_id, const GURL& origin) const {
  AreaMap::const_iterator it = connections_.begin();
  for (; it != connections_.end(); ++it) {
    if (namespace_id == it->second.namespace_->namespace_id() &&
        origin == it->second.area_->origin()) {
      return true;
    }
  }
  return false;
}

DomStorageArea* DomStorageHost::GetOpenArea(int connection_id) {
  AreaMap::iterator found = connections_.find(connection_id);
  if (found == connections_.end())
    return NULL;
  return found->second.area_;
}

// NamespaceAndArea

DomStorageHost::NamespaceAndArea::NamespaceAndArea() {}
DomStorageHost::NamespaceAndArea::~NamespaceAndArea() {}

}  // namespace dom_storage
