// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace dom_storage {

class DomStorageDatabaseAdapter;
class DomStorageMap;
class DomStorageTaskRunner;
class SessionStorageDatabase;

// Container for a per-origin Map of key/value pairs potentially
// backed by storage on disk and lazily commits changes to disk.
// See class comments for DomStorageContext for a larger overview.
class WEBKIT_STORAGE_EXPORT DomStorageArea
    : public base::RefCountedThreadSafe<DomStorageArea> {

 public:
  static const base::FilePath::CharType kDatabaseFileExtension[];
  static base::FilePath DatabaseFileNameFromOrigin(const GURL& origin);
  static GURL OriginFromDatabaseFileName(const base::FilePath& file_name);

  // Local storage. Backed on disk if directory is nonempty.
  DomStorageArea(const GURL& origin,
                 const base::FilePath& directory,
                 DomStorageTaskRunner* task_runner);

  // Session storage. Backed on disk if |session_storage_backing| is not NULL.
  DomStorageArea(int64 namespace_id,
                 const std::string& persistent_namespace_id,
                 const GURL& origin,
                 SessionStorageDatabase* session_storage_backing,
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
  void FastClear();

  DomStorageArea* ShallowCopy(
      int64 destination_namespace_id,
      const std::string& destination_persistent_namespace_id);

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
  FRIEND_TEST_ALL_PREFIXES(DomStorageContextTest, PersistentIds);
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
  std::string persistent_namespace_id_;
  GURL origin_;
  base::FilePath directory_;
  scoped_refptr<DomStorageTaskRunner> task_runner_;
  scoped_refptr<DomStorageMap> map_;
  scoped_ptr<DomStorageDatabaseAdapter> backing_;
  scoped_refptr<SessionStorageDatabase> session_storage_backing_;
  bool is_initial_import_done_;
  bool is_shutdown_;
  scoped_ptr<CommitBatch> commit_batch_;
  int commit_batches_in_flight_;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
