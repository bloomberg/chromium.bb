// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_map.h"

namespace dom_storage {

DomStorageMap::DomStorageMap() {}
DomStorageMap::~DomStorageMap() {}

unsigned DomStorageMap::Length() {
  return values_.size();
}

NullableString16 DomStorageMap::Key(unsigned index) {
  if (index >= values_.size())
    return NullableString16(true);
  ValuesMap::const_iterator it = values_.begin();
  for (unsigned i = 0; i < index; ++i, ++it) {
    // TODO(benm): Alter this so we don't have to walk
    // up to the key from the beginning on each call.
  }
  return NullableString16(it->first, false);
}

NullableString16 DomStorageMap::GetItem(const string16& key) {
  ValuesMap::const_iterator found = values_.find(key);
  if (found == values_.end())
    return NullableString16(true);
  return found->second;
}

bool DomStorageMap::SetItem(
    const string16& key, const string16& value,
    NullableString16* old_value) {
  // TODO(benm): check quota and return false if exceeded
  ValuesMap::const_iterator found = values_.find(key);
  if (found == values_.end())
    *old_value = NullableString16(true);
  else
    *old_value = found->second;
  values_[key] = NullableString16(value, false);
  return true;
}

bool DomStorageMap::RemoveItem(
    const string16& key,
    string16* old_value) {
  ValuesMap::iterator found = values_.find(key);
  if (found == values_.end())
    return false;
  *old_value = found->second.string();
  values_.erase(found);
  return true;
}

void DomStorageMap::SwapValues(ValuesMap* values) {
  values_.swap(*values);
}

DomStorageMap* DomStorageMap::DeepCopy() {
  DomStorageMap* copy = new DomStorageMap;
  copy->values_ = values_;
  return copy;
}

}  // namespace dom_storage
