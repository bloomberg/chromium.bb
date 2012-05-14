// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
#pragma once

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/dom_storage/dom_storage_database.h"
#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

class DomStorageMap;
class DomStorageTaskRunner;

// Container for a per-origin Map of key/value pairs potentially
// backed by storage on disk and lazily commits changes to disk.
// See class comments for DomStorageContext for a larger overview.
class DomStorageArea
    : public base::RefCountedThreadSafe<DomStorageArea> {

 public:
  static const FilePath::CharType kDatabaseFileExtension[];
  static FilePath DatabaseFileNameFromOrigin(const GURL& origin);
  static GURL OriginFromDatabaseFileName(const FilePath& file_name);

  DomStorageArea(int64 namespace_id,
                 const GURL& origin,
                 const FilePath& directory,
                 DomStorageTaskRunner* task_runner);

  const GURL& origin() const { return origin_; }
  int64 namespace_id() const { return namespace_id_; }

  // Writes a copy of the current set of values in the area to the |map|.
  void ExtractValues(ValuesMap* map);

  unsigned Length();
  NullableString16 Key(unsigned index);
  NullableString16 GetItem(const string16& key);
  bool SetItem(const string16& key, const string16& value,
               NullableString16* old_value);
  bool RemoveItem(const string16& key, string16* old_value);
  bool Clear();

  DomStorageArea* ShallowCopy(int64 destination_namespace_id);

  bool HasUncommittedChanges() const;

  // Similar to Clear() but more optimized for just deleting
  // without raising events.
  void DeleteOrigin();

  // Frees up memory when possible. Typically, this method returns
  // the object to its just constructed state, however if uncommitted
  // changes are pending, it does nothing.
  void PurgeMemory();

  // Schedules the commit of any unsaved changes and enters a
  // shutdown state such that the value getters and setters will
  // no longer do anything.
  void Shutdown();

 private:
  friend class DomStorageAreaTest;
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, DomStorageAreaBasics);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, TestDatabaseFilePath);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, CommitChangesAtShutdown);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, DeleteOrigin);
  FRIEND_TEST_ALL_PREFIXES(DomStorageAreaTest, PurgeMemory);
  friend class base::RefCountedThreadSafe<DomStorageArea>;

  struct CommitBatch {
    bool clear_all_first;
    ValuesMap changed_values;
    CommitBatch();
    ~CommitBatch();
  };

  ~DomStorageArea();

  // If we haven't done so already and this is a local storage area,
  // will attempt to read any values for this origin currently
  // stored on disk.
  void InitialImportIfNeeded();

  // Post tasks to defer writing a batch of changed values to
  // disk on the commit sequence, and to call back on the primary
  // task sequence when complete.
  CommitBatch* CreateCommitBatchIfNeeded();
  void OnCommitTimer();
  void CommitChanges(const CommitBatch* commit_batch);
  void OnCommitComplete();

  void ShutdownInCommitSequence();

  int64 namespace_id_;
  GURL origin_;
  FilePath directory_;
  scoped_refptr<DomStorageTaskRunner> task_runner_;
  scoped_refptr<DomStorageMap> map_;
  scoped_ptr<DomStorageDatabase> backing_;
  bool is_initial_import_done_;
  bool is_shutdown_;
  scoped_ptr<CommitBatch> commit_batch_;
  int commit_batches_in_flight_;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
