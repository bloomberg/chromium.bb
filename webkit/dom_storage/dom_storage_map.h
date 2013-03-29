// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_MAP_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_MAP_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace dom_storage {

// A wrapper around a std::map that adds refcounting and
// tracks the size in bytes of the keys/values, enforcing a quota.
// See class comments for DomStorageContext for a larger overview.
class WEBKIT_STORAGE_EXPORT DomStorageMap
    : public base::RefCountedThreadSafe<DomStorageMap> {
 public:
  explicit DomStorageMap(size_t quota);

  unsigned Length() const;
  NullableString16 Key(unsigned index);
  NullableString16 GetItem(const base::string16& key) const;
  bool SetItem(const base::string16& key, const base::string16& value,
               NullableString16* old_value);
  bool RemoveItem(const base::string16& key, base::string16* old_value);

  // Swaps this instances values_ with |map|.
  // Note: to grandfather in pre-existing files that are overbudget,
  // this method does not do quota checking.
  void SwapValues(ValuesMap* map);

  // Writes a copy of the current set of values_ to the |map|.
  void ExtractValues(ValuesMap* map) const { *map = values_; }

  // Creates a new instance of DomStorageMap containing
  // a deep copy of values_.
  DomStorageMap* DeepCopy() const;

  size_t bytes_used() const { return bytes_used_; }
  size_t quota() const { return quota_; }
  void set_quota(size_t quota) { quota_ = quota; }

 private:
  friend class base::RefCountedThreadSafe<DomStorageMap>;
  ~DomStorageMap();

  void ResetKeyIterator();

  ValuesMap values_;
  ValuesMap::const_iterator key_iterator_;
  unsigned last_key_index_;
  size_t bytes_used_;
  size_t quota_;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_MAP_H_
