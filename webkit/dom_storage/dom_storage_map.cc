// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_map.h"

namespace dom_storage {

DomStorageMap::DomStorageMap() {
  ResetKeyIterator();
}

DomStorageMap::~DomStorageMap() {}

unsigned DomStorageMap::Length() const {
  return values_.size();
}

NullableString16 DomStorageMap::Key(unsigned index) {
  if (index >= values_.size())
    return NullableString16(true);
  while (last_key_index_ != index) {
    if (last_key_index_ > index) {
      --key_iterator_;
      --last_key_index_;
    } else {
      ++key_iterator_;
      ++last_key_index_;
    }
  }
  return NullableString16(key_iterator_->first, false);
}

NullableString16 DomStorageMap::GetItem(const string16& key) const {
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
  ResetKeyIterator();
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
  ResetKeyIterator();
  return true;
}

void DomStorageMap::SwapValues(ValuesMap* values) {
  values_.swap(*values);
  ResetKeyIterator();
}

DomStorageMap* DomStorageMap::DeepCopy() const {
  DomStorageMap* copy = new DomStorageMap;
  copy->values_ = values_;
  copy->ResetKeyIterator();
  return copy;
}

void DomStorageMap::ResetKeyIterator() {
  key_iterator_ = values_.begin();
  last_key_index_ = 0;
}

}  // namespace dom_storage
