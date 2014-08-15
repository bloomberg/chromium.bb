// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "sync/syncable/deferred_on_disk_directory_backing_store.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/on_disk_directory_backing_store.h"
#include "sync/syncable/syncable_delete_journal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {
namespace {

static const base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

class DeferredOnDiskDirectoryBackingStoreTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    CHECK(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.path().Append(kSyncDataFolderName);
  }

  virtual void TearDown() OVERRIDE {
    STLDeleteValues(&handles_map_);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  Directory::MetahandlesMap handles_map_;
  JournalIndex delete_journals_;
  Directory::KernelLoadInfo kernel_load_info_;
};

// Test initialization of root entry when calling Load().
TEST_F(DeferredOnDiskDirectoryBackingStoreTest, Load) {
  DeferredOnDiskDirectoryBackingStore store("test", db_path_);
  EXPECT_EQ(OPENED, store.Load(&handles_map_, &delete_journals_,
                               &kernel_load_info_));
  EXPECT_TRUE(delete_journals_.empty());
  ASSERT_EQ(1u, handles_map_.size());   // root node
  ASSERT_TRUE(handles_map_.count(1));
  EntryKernel* root = handles_map_[1];
  EXPECT_TRUE(root->ref(ID).IsRoot());
  EXPECT_EQ(1, root->ref(META_HANDLE));
  EXPECT_TRUE(root->ref(IS_DIR));
}

// Test on-disk DB is not created if SaveChanges() is not called.
TEST_F(DeferredOnDiskDirectoryBackingStoreTest,
       DontPersistIfSavingChangesNotCalled) {
  {
    // Open and close.
    DeferredOnDiskDirectoryBackingStore store("test", db_path_);
    EXPECT_EQ(OPENED, store.Load(&handles_map_, &delete_journals_,
                                 &kernel_load_info_));
  }

  EXPECT_FALSE(base::PathExists(db_path_));
}

// Test on-disk DB is not created when there are no changes.
TEST_F(DeferredOnDiskDirectoryBackingStoreTest,
       DontPersistWhenSavingNoChanges) {
  {
    // Open and close.
    DeferredOnDiskDirectoryBackingStore store("test", db_path_);
    EXPECT_EQ(OPENED, store.Load(&handles_map_, &delete_journals_,
                                 &kernel_load_info_));

    Directory::SaveChangesSnapshot snapshot;
    store.SaveChanges(snapshot);
  }

  EXPECT_FALSE(base::PathExists(db_path_));
}

// Test changes are persisted to disk when SaveChanges() is called.
TEST_F(DeferredOnDiskDirectoryBackingStoreTest, PersistWhenSavingValidChanges) {
  {
    // Open and close.
    DeferredOnDiskDirectoryBackingStore store("test", db_path_);
    EXPECT_EQ(OPENED, store.Load(&handles_map_, &delete_journals_,
                                 &kernel_load_info_));

    Directory::SaveChangesSnapshot snapshot;
    EntryKernel* entry = new EntryKernel();
    entry->put(ID, Id::CreateFromClientString("test_entry"));
    entry->put(META_HANDLE, 2);
    entry->mark_dirty(NULL);
    snapshot.dirty_metas.insert(entry);
    store.SaveChanges(snapshot);
  }

  STLDeleteValues(&handles_map_);

  ASSERT_TRUE(base::PathExists(db_path_));
  OnDiskDirectoryBackingStore read_store("test", db_path_);
  EXPECT_EQ(OPENED, read_store.Load(&handles_map_, &delete_journals_,
                                    &kernel_load_info_));
  ASSERT_EQ(2u, handles_map_.size());
  ASSERT_TRUE(handles_map_.count(1));     // root node
  ASSERT_TRUE(handles_map_.count(2));
  EntryKernel* entry = handles_map_[2];
  EXPECT_EQ(Id::CreateFromClientString("test_entry"), entry->ref(ID));
}

}  // namespace
}  // namespace syncable
}  // namespace syncer
