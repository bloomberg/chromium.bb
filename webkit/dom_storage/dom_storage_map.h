// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_MAP_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_MAP_H_
#pragma once

#include <map>

#include "base/memory/ref_counted.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "webkit/dom_storage/dom_storage_types.h"

class FilePath;
class GURL;

namespace dom_storage {

// A wrapper around a std::map that adds refcounting and
// imposes a limit on the total byte size of the keys/values.
// See class comments for DomStorageContext for a larger overview.
class DomStorageMap
    : public base::RefCountedThreadSafe<DomStorageMap> {
 public:
  DomStorageMap();

  unsigned Length() const;
  NullableString16 Key(unsigned index);
  NullableString16 GetItem(const string16& key) const;
  bool SetItem(const string16& key, const string16& value,
               NullableString16* old_value);
  bool RemoveItem(const string16& key, string16* old_value);

  void SwapValues(ValuesMap* map);
  DomStorageMap* DeepCopy() const;

 private:
  friend class base::RefCountedThreadSafe<DomStorageMap>;
  ~DomStorageMap();

  void ResetKeyIterator();

  ValuesMap values_;
  ValuesMap::const_iterator key_iterator_;
  unsigned last_key_index_;
  // TODO(benm): track usage and enforce a quota limit.
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
