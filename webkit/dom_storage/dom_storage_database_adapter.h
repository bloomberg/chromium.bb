// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_DATABASE_ADAPTER_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_DATABASE_ADAPTER_H_

// Database interface used by DomStorageArea. Abstracts the differences between
// the per-origin DomStorageDatabases for localStorage and
// SessionStorageDatabase which stores multiple origins.

#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace dom_storage {

class WEBKIT_STORAGE_EXPORT DomStorageDatabaseAdapter {
 public:
  virtual ~DomStorageDatabaseAdapter() {}
  virtual void ReadAllValues(ValuesMap* result) = 0;
  virtual bool CommitChanges(
      bool clear_all_first, const ValuesMap& changes) = 0;
  virtual void DeleteFiles() {}
  virtual void Reset() {}
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_DATABASE_ADAPTER_H_
