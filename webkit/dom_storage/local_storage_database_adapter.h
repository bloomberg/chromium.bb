// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_LOCAL_STORAGE_DATABASE_ADAPTER_H_
#define WEBKIT_DOM_STORAGE_LOCAL_STORAGE_DATABASE_ADAPTER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/dom_storage/dom_storage_database_adapter.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class FilePath;
}

namespace dom_storage {

class DomStorageDatabase;

class WEBKIT_STORAGE_EXPORT LocalStorageDatabaseAdapter :
      public DomStorageDatabaseAdapter {
 public:
  explicit LocalStorageDatabaseAdapter(const base::FilePath& path);
  virtual ~LocalStorageDatabaseAdapter();
  virtual void ReadAllValues(ValuesMap* result) OVERRIDE;
  virtual bool CommitChanges(bool clear_all_first,
                             const ValuesMap& changes) OVERRIDE;
  virtual void DeleteFiles() OVERRIDE;
  virtual void Reset() OVERRIDE;

 protected:
  // Constructor that uses an in-memory sqlite database, for testing.
  LocalStorageDatabaseAdapter();

 private:
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, CommitChangesAtShutdown);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, DeleteOrigin);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, PurgeMemory);

  scoped_ptr<DomStorageDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageDatabaseAdapter);
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_LOCAL_STORAGE_DATABASE_ADAPTER_H_
