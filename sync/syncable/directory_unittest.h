// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_DIRECTORY_UNITTEST_H_
#define SYNC_SYNCABLE_DIRECTORY_UNITTEST_H_

#include <string>

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/fake_encryptor.h"
#include "sync/test/null_directory_change_delegate.h"
#include "sync/test/null_transaction_observer.h"
#include "sync/util/test_unrecoverable_error_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace syncable {

class BaseTransaction;

// A test fixture for syncable::Directory.  Uses an in-memory database to keep
// the unit tests fast.
//
// Serves as base class for several other test fixtures.
class SyncableDirectoryTest : public testing::Test {
 protected:
  static const char kDirectoryName[];

  SyncableDirectoryTest();
  virtual ~SyncableDirectoryTest();

  virtual void SetUp();
  virtual void TearDown();

  // Destroys any currently opened directory, creates and opens a new one.
  //
  // Returns result of the Open call.
  DirOpenResult ReopenDirectory();

  // Creates an empty entry and sets the ID field to a default one.
  void CreateEntry(const ModelType& model_type, const std::string& entryname);

  // Creates an empty entry and sets the ID field to id.
  void CreateEntry(const ModelType& model_type,
                   const std::string& entryname,
                   const int id);

  void CreateEntry(const ModelType& model_type,
                   const std::string& entryname,
                   const Id& id);

  void CreateEntryWithAttachmentMetadata(
      const ModelType& model_type,
      const std::string& entryname,
      const Id& id,
      const sync_pb::AttachmentMetadata& attachment_metadata);

  void DeleteEntry(const Id& id);

  // When a directory is saved then loaded from disk, it will pass through
  // DropDeletedEntries().  This will remove some entries from the directory.
  // This function is intended to simulate that process.
  //
  // WARNING: The directory will be deleted by this operation.  You should
  // not have any pointers to the directory (open transactions included)
  // when you call this.
  DirOpenResult SimulateSaveAndReloadDir();

  // This function will close and re-open the directory without saving any
  // pending changes.  This is intended to simulate the recovery from a crash
  // scenario.  The same warnings for SimulateSaveAndReloadDir apply here.
  DirOpenResult SimulateCrashAndReloadDir();

  void GetAllMetaHandles(BaseTransaction* trans, MetahandleSet* result);
  void CheckPurgeEntriesWithTypeInSucceeded(ModelTypeSet types_to_purge,
                                            bool before_reload);
  bool IsInDirtyMetahandles(int64 metahandle);
  bool IsInMetahandlesToPurge(int64 metahandle);

  scoped_ptr<Directory>& dir();
  DirectoryChangeDelegate* directory_change_delegate();
  Encryptor* encryptor();
  UnrecoverableErrorHandler* unrecoverable_error_handler();

 private:
  void ValidateEntry(BaseTransaction* trans,
                     int64 id,
                     bool check_name,
                     const std::string& name,
                     int64 base_version,
                     int64 server_version,
                     bool is_del);

  base::MessageLoop message_loop_;
  scoped_ptr<Directory> dir_;
  NullDirectoryChangeDelegate delegate_;
  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  sql::Connection connection_;
};

}  // namespace syncable

}  // namespace syncer

#endif  // SYNC_SYNCABLE_DIRECTORY_UNITTEST_H_
