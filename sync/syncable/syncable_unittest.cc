// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/test/values_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "sync/internal_api/public/base/node_ordinal.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/syncable/directory_backing_store.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/metahandle_set.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/on_disk_directory_backing_store.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "sync/test/fake_encryptor.h"
#include "sync/test/null_directory_change_delegate.h"
#include "sync/test/null_transaction_observer.h"
#include "sync/util/test_unrecoverable_error_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {

using base::ExpectDictBooleanValue;
using base::ExpectDictStringValue;

class SyncableKernelTest : public testing::Test {};

// TODO(akalin): Add unit tests for EntryKernel::ContainsString().

TEST_F(SyncableKernelTest, ToValue) {
  EntryKernel kernel;
  scoped_ptr<DictionaryValue> value(kernel.ToValue(NULL));
  if (value.get()) {
    // Not much to check without repeating the ToValue() code.
    EXPECT_TRUE(value->HasKey("isDirty"));
    // The extra +2 is for "isDirty" and "serverModelType".
    EXPECT_EQ(BIT_TEMPS_END - BEGIN_FIELDS + 2,
              static_cast<int>(value->size()));
  } else {
    ADD_FAILURE();
  }
}

namespace {
void PutDataAsBookmarkFavicon(WriteTransaction* wtrans,
                              MutableEntry* e,
                              const char* bytes,
                              size_t bytes_length) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_url("http://demo/");
  specifics.mutable_bookmark()->set_favicon(bytes, bytes_length);
  e->Put(SPECIFICS, specifics);
}

void ExpectDataFromBookmarkFaviconEquals(BaseTransaction* trans,
                                         Entry* e,
                                         const char* bytes,
                                         size_t bytes_length) {
  ASSERT_TRUE(e->good());
  ASSERT_TRUE(e->Get(SPECIFICS).has_bookmark());
  ASSERT_EQ("http://demo/", e->Get(SPECIFICS).bookmark().url());
  ASSERT_EQ(std::string(bytes, bytes_length),
            e->Get(SPECIFICS).bookmark().favicon());
}
}  // namespace

class SyncableGeneralTest : public testing::Test {
 public:
  static const char kIndexTestName[];
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.path().Append(
        FILE_PATH_LITERAL("SyncableTest.sqlite3"));
  }

  virtual void TearDown() {
  }
 protected:
  MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  NullDirectoryChangeDelegate delegate_;
  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  base::FilePath db_path_;
};

const char SyncableGeneralTest::kIndexTestName[] = "IndexTest";

TEST_F(SyncableGeneralTest, General) {
  Directory dir(new InMemoryDirectoryBackingStore("SimpleTest"),
                &handler_,
                NULL,
                NULL,
                NULL);

  ASSERT_EQ(OPENED, dir.Open(
          "SimpleTest", &delegate_, NullTransactionObserver()));

  int64 root_metahandle;
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(e.good());
    root_metahandle = e.Get(META_HANDLE);
  }

  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  // Test simple read operations on an empty DB.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Directory::ChildHandles child_handles;
    dir.GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_TRUE(child_handles.empty());

    dir.GetChildHandlesByHandle(&rtrans, root_metahandle, &child_handles);
    EXPECT_TRUE(child_handles.empty());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(BASE_VERSION, 1);
    written_metahandle = me.Get(META_HANDLE);
  }

  // Test GetChildHandles* after something is now in the DB.
  // Also check that GET_BY_ID works.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Directory::ChildHandles child_handles;
    dir.GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (Directory::ChildHandles::iterator i = child_handles.begin();
         i != child_handles.end(); ++i) {
      EXPECT_EQ(*i, written_metahandle);
    }

    dir.GetChildHandlesByHandle(&rtrans, root_metahandle, &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (Directory::ChildHandles::iterator i = child_handles.begin();
         i != child_handles.end(); ++i) {
      EXPECT_EQ(*i, written_metahandle);
    }
  }

  // Test writing data to an entity. Also check that GET_BY_HANDLE works.
  static const char s[] = "Hello World.";
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, &dir);
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    PutDataAsBookmarkFavicon(&trans, &e, s, sizeof(s));
  }

  // Test reading back the contents that we just wrote.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, &dir);
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    ExpectDataFromBookmarkFaviconEquals(&trans, &e, s, sizeof(s));
  }

  // Verify it exists in the folder.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    EXPECT_EQ(1, CountEntriesWithName(&rtrans, rtrans.root_id(), name));
  }

  // Now delete it.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, &dir);
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    e.Put(IS_DEL, true);

    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), name));
  }

  dir.SaveChanges();
}

TEST_F(SyncableGeneralTest, ChildrenOps) {
  Directory dir(new InMemoryDirectoryBackingStore("SimpleTest"),
                &handler_,
                NULL,
                NULL,
                NULL);
  ASSERT_EQ(OPENED, dir.Open(
          "SimpleTest", &delegate_, NullTransactionObserver()));

  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    EXPECT_FALSE(dir.HasChildren(&rtrans, rtrans.root_id()));
    Id child_id;
    EXPECT_TRUE(dir.GetFirstChildId(&rtrans, rtrans.root_id(), &child_id));
    EXPECT_TRUE(child_id.IsRoot());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(BASE_VERSION, 1);
    written_metahandle = me.Get(META_HANDLE);
  }

  // Test children ops after something is now in the DB.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry child(&rtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(child.good());

    EXPECT_TRUE(dir.HasChildren(&rtrans, rtrans.root_id()));
    Id child_id;
    EXPECT_TRUE(dir.GetFirstChildId(&rtrans, rtrans.root_id(), &child_id));
    EXPECT_EQ(e.Get(ID), child_id);
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(me.good());
    me.Put(IS_DEL, true);
  }

  // Test children ops after the children have been deleted.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    EXPECT_FALSE(dir.HasChildren(&rtrans, rtrans.root_id()));
    Id child_id;
    EXPECT_TRUE(dir.GetFirstChildId(&rtrans, rtrans.root_id(), &child_id));
    EXPECT_TRUE(child_id.IsRoot());
  }

  dir.SaveChanges();
}

TEST_F(SyncableGeneralTest, ClientIndexRebuildsProperly) {
  int64 written_metahandle;
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string name = "cheesepuffs";
  std::string tag = "dietcoke";

  // Test creating a new meta entry.
  {
    Directory dir(new OnDiskDirectoryBackingStore(kIndexTestName, db_path_),
                  &handler_,
                  NULL,
                  NULL,
                  NULL);
    ASSERT_EQ(OPENED, dir.Open(kIndexTestName, &delegate_,
                               NullTransactionObserver()));
    {
      WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
      MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
      ASSERT_TRUE(me.good());
      me.Put(ID, id);
      me.Put(BASE_VERSION, 1);
      me.Put(UNIQUE_CLIENT_TAG, tag);
      written_metahandle = me.Get(META_HANDLE);
    }
    dir.SaveChanges();
  }

  // The DB was closed. Now reopen it. This will cause index regeneration.
  {
    Directory dir(new OnDiskDirectoryBackingStore(kIndexTestName, db_path_),
                  &handler_,
                  NULL,
                  NULL,
                  NULL);
    ASSERT_EQ(OPENED, dir.Open(kIndexTestName,
                               &delegate_, NullTransactionObserver()));

    ReadTransaction trans(FROM_HERE, &dir);
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.Get(ID), id);
    EXPECT_EQ(me.Get(BASE_VERSION), 1);
    EXPECT_EQ(me.Get(UNIQUE_CLIENT_TAG), tag);
    EXPECT_EQ(me.Get(META_HANDLE), written_metahandle);
  }
}

TEST_F(SyncableGeneralTest, ClientIndexRebuildsDeletedProperly) {
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string tag = "dietcoke";

  // Test creating a deleted, unsynced, server meta entry.
  {
    Directory dir(new OnDiskDirectoryBackingStore(kIndexTestName, db_path_),
                  &handler_,
                  NULL,
                  NULL,
                  NULL);
    ASSERT_EQ(OPENED, dir.Open(kIndexTestName, &delegate_,
                                NullTransactionObserver()));
    {
      WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
      MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "deleted");
      ASSERT_TRUE(me.good());
      me.Put(ID, id);
      me.Put(BASE_VERSION, 1);
      me.Put(UNIQUE_CLIENT_TAG, tag);
      me.Put(IS_DEL, true);
      me.Put(IS_UNSYNCED, true);  // Or it might be purged.
    }
    dir.SaveChanges();
  }

  // The DB was closed. Now reopen it. This will cause index regeneration.
  // Should still be present and valid in the client tag index.
  {
    Directory dir(new OnDiskDirectoryBackingStore(kIndexTestName, db_path_),
                  &handler_,
                  NULL,
                  NULL,
                  NULL);
    ASSERT_EQ(OPENED, dir.Open(kIndexTestName, &delegate_,
                               NullTransactionObserver()));

    ReadTransaction trans(FROM_HERE, &dir);
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.Get(ID), id);
    EXPECT_EQ(me.Get(UNIQUE_CLIENT_TAG), tag);
    EXPECT_TRUE(me.Get(IS_DEL));
    EXPECT_TRUE(me.Get(IS_UNSYNCED));
  }
}

TEST_F(SyncableGeneralTest, ToValue) {
  Directory dir(new InMemoryDirectoryBackingStore("SimpleTest"),
                &handler_,
                NULL,
                NULL,
                NULL);
  ASSERT_EQ(OPENED, dir.Open(
          "SimpleTest", &delegate_, NullTransactionObserver()));

  const Id id = TestIdFactory::FromNumber(99);
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    EXPECT_FALSE(e.good());  // Hasn't been written yet.

    scoped_ptr<DictionaryValue> value(e.ToValue(NULL));
    ExpectDictBooleanValue(false, *value, "good");
    EXPECT_EQ(1u, value->size());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "new");
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(BASE_VERSION, 1);

    scoped_ptr<DictionaryValue> value(me.ToValue(NULL));
    ExpectDictBooleanValue(true, *value, "good");
    EXPECT_TRUE(value->HasKey("kernel"));
    ExpectDictStringValue("Bookmarks", *value, "modelType");
    ExpectDictBooleanValue(true, *value, "existsOnClientBecauseNameIsNonEmpty");
    ExpectDictBooleanValue(false, *value, "isRoot");
  }

  dir.SaveChanges();
}

// A test fixture for syncable::Directory.  Uses an in-memory database to keep
// the unit tests fast.
class SyncableDirectoryTest : public testing::Test {
 protected:
  MessageLoop message_loop_;
  static const char kName[];

  virtual void SetUp() {
    dir_.reset(new Directory(new InMemoryDirectoryBackingStore(kName),
                             &handler_,
                             NULL,
                             NULL,
                             NULL));
    ASSERT_TRUE(dir_.get());
    ASSERT_EQ(OPENED, dir_->Open(kName, &delegate_,
                                 NullTransactionObserver()));
    ASSERT_TRUE(dir_->good());
  }

  virtual void TearDown() {
    if (dir_.get())
      dir_->SaveChanges();
    dir_.reset();
  }

  void GetAllMetaHandles(BaseTransaction* trans, MetahandleSet* result) {
    dir_->GetAllMetaHandles(trans, result);
  }

  bool IsInDirtyMetahandles(int64 metahandle) {
    return 1 == dir_->kernel_->dirty_metahandles->count(metahandle);
  }

  bool IsInMetahandlesToPurge(int64 metahandle) {
    return 1 == dir_->kernel_->metahandles_to_purge->count(metahandle);
  }

  void CheckPurgeEntriesWithTypeInSucceeded(ModelTypeSet types_to_purge,
                                            bool before_reload) {
    SCOPED_TRACE(testing::Message("Before reload: ") << before_reload);
    {
      ReadTransaction trans(FROM_HERE, dir_.get());
      MetahandleSet all_set;
      dir_->GetAllMetaHandles(&trans, &all_set);
      EXPECT_EQ(4U, all_set.size());
      if (before_reload)
        EXPECT_EQ(6U, dir_->kernel_->metahandles_to_purge->size());
      for (MetahandleSet::iterator iter = all_set.begin();
           iter != all_set.end(); ++iter) {
        Entry e(&trans, GET_BY_HANDLE, *iter);
        const ModelType local_type = e.GetModelType();
        const ModelType server_type = e.GetServerModelType();

        // Note the dance around incrementing |it|, since we sometimes erase().
        if ((IsRealDataType(local_type) &&
             types_to_purge.Has(local_type)) ||
            (IsRealDataType(server_type) &&
             types_to_purge.Has(server_type))) {
          FAIL() << "Illegal type should have been deleted.";
        }
      }
    }

    for (ModelTypeSet::Iterator it = types_to_purge.First();
         it.Good(); it.Inc()) {
      EXPECT_FALSE(dir_->InitialSyncEndedForType(it.Get()));
    }
    EXPECT_FALSE(types_to_purge.Has(BOOKMARKS));
    EXPECT_TRUE(dir_->InitialSyncEndedForType(BOOKMARKS));
  }

  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  scoped_ptr<Directory> dir_;
  NullDirectoryChangeDelegate delegate_;

  // Creates an empty entry and sets the ID field to a default one.
  void CreateEntry(const std::string& entryname) {
    CreateEntry(entryname, TestIdFactory::FromNumber(-99));
  }

  // Creates an empty entry and sets the ID field to id.
  void CreateEntry(const std::string& entryname, const int id) {
    CreateEntry(entryname, TestIdFactory::FromNumber(id));
  }
  void CreateEntry(const std::string& entryname, Id id) {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), entryname);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(IS_UNSYNCED, true);
  }

  void ValidateEntry(BaseTransaction* trans,
                     int64 id,
                     bool check_name,
                     const std::string& name,
                     int64 base_version,
                     int64 server_version,
                     bool is_del);

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

 private:
  // A helper function for Simulate{Save,Crash}AndReloadDir.
  DirOpenResult ReloadDirImpl();
};

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsMetahandlesToPurge) {
  const int metas_to_create = 50;
  MetahandleSet expected_purges;
  MetahandleSet all_handles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (int i = 0; i < metas_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      e.Put(IS_UNSYNCED, true);
      sync_pb::EntitySpecifics specs;
      if (i % 2 == 0) {
        AddDefaultFieldValue(BOOKMARKS, &specs);
        expected_purges.insert(e.Get(META_HANDLE));
        all_handles.insert(e.Get(META_HANDLE));
      } else {
        AddDefaultFieldValue(PREFERENCES, &specs);
        all_handles.insert(e.Get(META_HANDLE));
      }
      e.Put(SPECIFICS, specs);
      e.Put(SERVER_SPECIFICS, specs);
    }
  }

  ModelTypeSet to_purge(BOOKMARKS);
  dir_->PurgeEntriesWithTypeIn(to_purge, ModelTypeSet());

  Directory::SaveChangesSnapshot snapshot1;
  base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
  dir_->TakeSnapshotForSaveChanges(&snapshot1);
  EXPECT_TRUE(expected_purges == snapshot1.metahandles_to_purge);

  to_purge.Clear();
  to_purge.Put(PREFERENCES);
  dir_->PurgeEntriesWithTypeIn(to_purge, ModelTypeSet());

  dir_->HandleSaveChangesFailure(snapshot1);

  Directory::SaveChangesSnapshot snapshot2;
  dir_->TakeSnapshotForSaveChanges(&snapshot2);
  EXPECT_TRUE(all_handles == snapshot2.metahandles_to_purge);
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsAllDirtyHandlesTest) {
  const int metahandles_to_create = 100;
  std::vector<int64> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.Get(META_HANDLE));
      e.Put(IS_UNSYNCED, true);
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    ASSERT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
        i != snapshot.dirty_metas.end(); ++i) {
      ASSERT_TRUE((*i)->is_dirty());
    }
    dir_->VacuumAfterSaveChanges(snapshot);
  }
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    std::vector<int64> new_dirty_metahandles;
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
        // Change existing entries to directories to dirty them.
        MutableEntry e1(&trans, GET_BY_HANDLE, *i);
        e1.Put(IS_DIR, true);
        e1.Put(IS_UNSYNCED, true);
        // Add new entries
        MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), "bar");
        e2.Put(IS_UNSYNCED, true);
        new_dirty_metahandles.push_back(e2.Get(META_HANDLE));
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
        new_dirty_metahandles.begin(), new_dirty_metahandles.end());
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
        i != snapshot.dirty_metas.end(); ++i) {
      EXPECT_TRUE((*i)->is_dirty());
    }
    dir_->VacuumAfterSaveChanges(snapshot);
  }
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsOnlyDirtyHandlesTest) {
  const int metahandles_to_create = 100;

  // half of 2 * metahandles_to_create
  const unsigned int number_changed = 100u;
  std::vector<int64> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.Get(META_HANDLE));
      e.Put(IS_UNSYNCED, true);
    }
  }
  dir_->SaveChanges();
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    std::vector<int64> new_dirty_metahandles;
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
        // Change existing entries to directories to dirty them.
        MutableEntry e1(&trans, GET_BY_HANDLE, *i);
        ASSERT_TRUE(e1.good());
        e1.Put(IS_DIR, true);
        e1.Put(IS_UNSYNCED, true);
        // Add new entries
        MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), "bar");
        e2.Put(IS_UNSYNCED, true);
        new_dirty_metahandles.push_back(e2.Get(META_HANDLE));
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
        new_dirty_metahandles.begin(), new_dirty_metahandles.end());
  }
  dir_->SaveChanges();
  // Don't make any changes whatsoever and ensure nothing comes back.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
      MutableEntry e(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e.good());
      // We aren't doing anything to dirty these entries.
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there are no dirty_metahandles.
    EXPECT_EQ(0u, snapshot.dirty_metas.size());
    dir_->VacuumAfterSaveChanges(snapshot);
  }
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    bool should_change = false;
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
        // Maybe change entries by flipping IS_DIR.
        MutableEntry e(&trans, GET_BY_HANDLE, *i);
        ASSERT_TRUE(e.good());
        should_change = !should_change;
        if (should_change) {
          bool not_dir = !e.Get(IS_DIR);
          e.Put(IS_DIR, not_dir);
          e.Put(IS_UNSYNCED, true);
        }
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each changed metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(number_changed, snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
        i != snapshot.dirty_metas.end(); ++i) {
      EXPECT_TRUE((*i)->is_dirty());
    }
    dir_->VacuumAfterSaveChanges(snapshot);
  }
}

// Test delete journals management.
TEST_F(SyncableDirectoryTest, ManageDeleteJournals) {
  sync_pb::EntitySpecifics bookmark_specifics;
  AddDefaultFieldValue(BOOKMARKS, &bookmark_specifics);
  bookmark_specifics.mutable_bookmark()->set_url("url");

  Id id1 = TestIdFactory::FromNumber(-1);
  Id id2 = TestIdFactory::FromNumber(-2);
  int64 handle1 = 0;
  int64 handle2 = 0;
  {
    // Create two bookmark entries and save in database.
    CreateEntry("item1", id1);
    CreateEntry("item2", id2);
    {
      WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
      MutableEntry item1(&trans, GET_BY_ID, id1);
      ASSERT_TRUE(item1.good());
      handle1 = item1.Get(META_HANDLE);
      item1.Put(SPECIFICS, bookmark_specifics);
      item1.Put(SERVER_SPECIFICS, bookmark_specifics);
      MutableEntry item2(&trans, GET_BY_ID, id2);
      ASSERT_TRUE(item2.good());
      handle2 = item2.Get(META_HANDLE);
      item2.Put(SPECIFICS, bookmark_specifics);
      item2.Put(SERVER_SPECIFICS, bookmark_specifics);
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  { // Test adding and saving delete journals.
    DeleteJournal* delete_journal = dir_->delete_journal();
    {
      WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
      EntryKernelSet journal_entries;
      delete_journal->GetDeleteJournals(&trans, BOOKMARKS, &journal_entries);
      ASSERT_EQ(0u, journal_entries.size());

      // Set SERVER_IS_DEL of the entries to true and they should be added to
      // delete journals.
      MutableEntry item1(&trans, GET_BY_ID, id1);
      ASSERT_TRUE(item1.good());
      item1.Put(SERVER_IS_DEL, true);
      MutableEntry item2(&trans, GET_BY_ID, id2);
      ASSERT_TRUE(item2.good());
      item2.Put(SERVER_IS_DEL, true);
      EntryKernel tmp;
      tmp.put(ID, id1);
      EXPECT_TRUE(delete_journal->delete_journals_.count(&tmp));
      tmp.put(ID, id2);
      EXPECT_TRUE(delete_journal->delete_journals_.count(&tmp));
    }

    // Save delete journals in database and verify memory clearing.
    ASSERT_TRUE(dir_->SaveChanges());
    {
      ReadTransaction trans(FROM_HERE, dir_.get());
      EXPECT_EQ(0u, delete_journal->GetDeleteJournalSize(&trans));
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  {
    {
      // Test reading delete journals from database.
      WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
      DeleteJournal* delete_journal = dir_->delete_journal();
      EntryKernelSet journal_entries;
      delete_journal->GetDeleteJournals(&trans, BOOKMARKS, &journal_entries);
      ASSERT_EQ(2u, journal_entries.size());
      EntryKernel tmp;
      tmp.put(META_HANDLE, handle1);
      EXPECT_TRUE(journal_entries.count(&tmp));
      tmp.put(META_HANDLE, handle2);
      EXPECT_TRUE(journal_entries.count(&tmp));

      // Purge item2.
      MetahandleSet to_purge;
      to_purge.insert(handle2);
      delete_journal->PurgeDeleteJournals(&trans, to_purge);

      // Verify that item2 is purged from journals in memory and will be
      // purged from database.
      tmp.put(ID, id2);
      EXPECT_FALSE(delete_journal->delete_journals_.count(&tmp));
      EXPECT_EQ(1u, delete_journal->delete_journals_to_purge_.size());
      EXPECT_TRUE(delete_journal->delete_journals_to_purge_.count(handle2));
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  {
    {
      // Verify purged entry is gone in database.
      WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
      DeleteJournal* delete_journal = dir_->delete_journal();
      EntryKernelSet journal_entries;
      delete_journal->GetDeleteJournals(&trans, BOOKMARKS, &journal_entries);
      ASSERT_EQ(1u, journal_entries.size());
      EntryKernel tmp;
      tmp.put(ID, id1);
      tmp.put(META_HANDLE, handle1);
      EXPECT_TRUE(journal_entries.count(&tmp));

      // Undelete item1.
      MutableEntry item1(&trans, GET_BY_ID, id1);
      ASSERT_TRUE(item1.good());
      item1.Put(SERVER_IS_DEL, false);
      EXPECT_TRUE(delete_journal->delete_journals_.empty());
      EXPECT_EQ(1u, delete_journal->delete_journals_to_purge_.size());
      EXPECT_TRUE(delete_journal->delete_journals_to_purge_.count(handle1));
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  {
    // Verify undeleted entry is gone from database.
    ReadTransaction trans(FROM_HERE, dir_.get());
    DeleteJournal* delete_journal = dir_->delete_journal();
    ASSERT_EQ(0u, delete_journal->GetDeleteJournalSize(&trans));
  }
}

const char SyncableDirectoryTest::kName[] = "Foo";

namespace {

TEST_F(SyncableDirectoryTest, TestBasicLookupNonExistantID) {
  ReadTransaction rtrans(FROM_HERE, dir_.get());
  Entry e(&rtrans, GET_BY_ID, TestIdFactory::FromNumber(-99));
  ASSERT_FALSE(e.good());
}

TEST_F(SyncableDirectoryTest, TestBasicLookupValidID) {
  CreateEntry("rtc");
  ReadTransaction rtrans(FROM_HERE, dir_.get());
  Entry e(&rtrans, GET_BY_ID, TestIdFactory::FromNumber(-99));
  ASSERT_TRUE(e.good());
}

TEST_F(SyncableDirectoryTest, TestDelete) {
  std::string name = "peanut butter jelly time";
  WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e1.good());
  ASSERT_TRUE(e1.Put(IS_DEL, true));
  MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e2.good());
  ASSERT_TRUE(e2.Put(IS_DEL, true));
  MutableEntry e3(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e3.good());
  ASSERT_TRUE(e3.Put(IS_DEL, true));

  ASSERT_TRUE(e1.Put(IS_DEL, false));
  ASSERT_TRUE(e2.Put(IS_DEL, false));
  ASSERT_TRUE(e3.Put(IS_DEL, false));

  ASSERT_TRUE(e1.Put(IS_DEL, true));
  ASSERT_TRUE(e2.Put(IS_DEL, true));
  ASSERT_TRUE(e3.Put(IS_DEL, true));
}

TEST_F(SyncableDirectoryTest, TestGetUnsynced) {
  Directory::UnsyncedMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));

    MutableEntry e2(&trans, CREATE, BOOKMARKS, e1.Get(ID), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.Get(META_HANDLE);
    e2.Put(BASE_VERSION, 1);
    e2.Put(ID, TestIdFactory::FromNumber(102));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(2 == handles.size());
    if (handle1 == handles[0]) {
      ASSERT_TRUE(handle2 == handles[1]);
    } else {
      ASSERT_TRUE(handle2 == handles[0]);
      ASSERT_TRUE(handle1 == handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    ASSERT_TRUE(e5.Get(IS_UNSYNCED));
    ASSERT_TRUE(e5.Put(IS_UNSYNCED, false));
    ASSERT_FALSE(e5.Get(IS_UNSYNCED));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, TestGetUnappliedUpdates) {
  std::vector<int64> handles;
  int64 handle1, handle2;
  const FullModelTypeSet all_types = FullModelTypeSet::All();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(IS_UNAPPLIED_UPDATE, false);
    e1.Put(BASE_VERSION, 1);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    e1.Put(IS_DIR, true);

    MutableEntry e2(&trans, CREATE, BOOKMARKS, e1.Get(ID), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.Get(META_HANDLE);
    e2.Put(IS_UNAPPLIED_UPDATE, false);
    e2.Put(BASE_VERSION, 1);
    e2.Put(ID, TestIdFactory::FromNumber(102));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(2 == handles.size());
    if (handle1 == handles[0]) {
      ASSERT_TRUE(handle2 == handles[1]);
    } else {
      ASSERT_TRUE(handle2 == handles[0]);
      ASSERT_TRUE(handle1 == handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    e5.Put(IS_UNAPPLIED_UPDATE, false);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}


TEST_F(SyncableDirectoryTest, DeleteBug_531383) {
  // Try to evoke a check failure...
  TestIdFactory id_factory;
  int64 grandchild_handle;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, id_factory.root(), "Bob");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(ID, id_factory.NewServerId());
    parent.Put(BASE_VERSION, 1);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.Get(ID), "Bob");
    ASSERT_TRUE(child.good());
    child.Put(IS_DIR, true);
    child.Put(ID, id_factory.NewServerId());
    child.Put(BASE_VERSION, 1);
    MutableEntry grandchild(&wtrans, CREATE, BOOKMARKS, child.Get(ID), "Bob");
    ASSERT_TRUE(grandchild.good());
    grandchild.Put(ID, id_factory.NewServerId());
    grandchild.Put(BASE_VERSION, 1);
    ASSERT_TRUE(grandchild.Put(IS_DEL, true));
    MutableEntry twin(&wtrans, CREATE, BOOKMARKS, child.Get(ID), "Bob");
    ASSERT_TRUE(twin.good());
    ASSERT_TRUE(twin.Put(IS_DEL, true));
    ASSERT_TRUE(grandchild.Put(IS_DEL, false));

    grandchild_handle = grandchild.Get(META_HANDLE);
  }
  dir_->SaveChanges();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry grandchild(&wtrans, GET_BY_HANDLE, grandchild_handle);
    grandchild.Put(IS_DEL, true);  // Used to CHECK fail here.
  }
}

static inline bool IsLegalNewParent(const Entry& a, const Entry& b) {
  return IsLegalNewParent(a.trans(), a.Get(ID), b.Get(ID));
}

TEST_F(SyncableDirectoryTest, TestIsLegalNewParent) {
  TestIdFactory id_factory;
  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
  Entry root(&wtrans, GET_BY_ID, id_factory.root());
  ASSERT_TRUE(root.good());
  MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root.Get(ID), "Bob");
  ASSERT_TRUE(parent.good());
  parent.Put(IS_DIR, true);
  parent.Put(ID, id_factory.NewServerId());
  parent.Put(BASE_VERSION, 1);
  MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.Get(ID), "Bob");
  ASSERT_TRUE(child.good());
  child.Put(IS_DIR, true);
  child.Put(ID, id_factory.NewServerId());
  child.Put(BASE_VERSION, 1);
  MutableEntry grandchild(&wtrans, CREATE, BOOKMARKS, child.Get(ID), "Bob");
  ASSERT_TRUE(grandchild.good());
  grandchild.Put(ID, id_factory.NewServerId());
  grandchild.Put(BASE_VERSION, 1);

  MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, root.Get(ID), "Pete");
  ASSERT_TRUE(parent2.good());
  parent2.Put(IS_DIR, true);
  parent2.Put(ID, id_factory.NewServerId());
  parent2.Put(BASE_VERSION, 1);
  MutableEntry child2(&wtrans, CREATE, BOOKMARKS, parent2.Get(ID), "Pete");
  ASSERT_TRUE(child2.good());
  child2.Put(IS_DIR, true);
  child2.Put(ID, id_factory.NewServerId());
  child2.Put(BASE_VERSION, 1);
  MutableEntry grandchild2(&wtrans, CREATE, BOOKMARKS, child2.Get(ID), "Pete");
  ASSERT_TRUE(grandchild2.good());
  grandchild2.Put(ID, id_factory.NewServerId());
  grandchild2.Put(BASE_VERSION, 1);
  // resulting tree
  //           root
  //           /  |
  //     parent    parent2
  //          |    |
  //      child    child2
  //          |    |
  // grandchild    grandchild2
  ASSERT_TRUE(IsLegalNewParent(child, root));
  ASSERT_TRUE(IsLegalNewParent(child, parent));
  ASSERT_FALSE(IsLegalNewParent(child, child));
  ASSERT_FALSE(IsLegalNewParent(child, grandchild));
  ASSERT_TRUE(IsLegalNewParent(child, parent2));
  ASSERT_TRUE(IsLegalNewParent(child, grandchild2));
  ASSERT_FALSE(IsLegalNewParent(parent, grandchild));
  ASSERT_FALSE(IsLegalNewParent(root, grandchild));
  ASSERT_FALSE(IsLegalNewParent(parent, grandchild));
}

TEST_F(SyncableDirectoryTest, TestEntryIsInFolder) {
  // Create a subdir and an entry.
  int64 entry_handle;
  syncable::Id folder_id;
  syncable::Id entry_id;
  std::string entry_name = "entry";

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "folder");
    ASSERT_TRUE(folder.good());
    EXPECT_TRUE(folder.Put(IS_DIR, true));
    EXPECT_TRUE(folder.Put(IS_UNSYNCED, true));
    folder_id = folder.Get(ID);

    MutableEntry entry(&trans, CREATE, BOOKMARKS, folder.Get(ID), entry_name);
    ASSERT_TRUE(entry.good());
    entry_handle = entry.Get(META_HANDLE);
    entry.Put(IS_UNSYNCED, true);
    entry_id = entry.Get(ID);
  }

  // Make sure we can find the entry in the folder.
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), entry_name));
    EXPECT_EQ(1, CountEntriesWithName(&trans, folder_id, entry_name));

    Entry entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(entry_handle, entry.Get(META_HANDLE));
    EXPECT_TRUE(entry.Get(NON_UNIQUE_NAME) == entry_name);
    EXPECT_TRUE(entry.Get(PARENT_ID) == folder_id);
  }
}

TEST_F(SyncableDirectoryTest, TestParentIdIndexUpdate) {
  std::string child_name = "child";

  WriteTransaction wt(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry parent_folder(&wt, CREATE, BOOKMARKS, wt.root_id(), "folder1");
  parent_folder.Put(IS_UNSYNCED, true);
  EXPECT_TRUE(parent_folder.Put(IS_DIR, true));

  MutableEntry parent_folder2(&wt, CREATE, BOOKMARKS, wt.root_id(), "folder2");
  parent_folder2.Put(IS_UNSYNCED, true);
  EXPECT_TRUE(parent_folder2.Put(IS_DIR, true));

  MutableEntry child(&wt, CREATE, BOOKMARKS, parent_folder.Get(ID), child_name);
  EXPECT_TRUE(child.Put(IS_DIR, true));
  child.Put(IS_UNSYNCED, true);

  ASSERT_TRUE(child.good());

  EXPECT_EQ(0, CountEntriesWithName(&wt, wt.root_id(), child_name));
  EXPECT_EQ(parent_folder.Get(ID), child.Get(PARENT_ID));
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder.Get(ID), child_name));
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder2.Get(ID), child_name));
  child.Put(PARENT_ID, parent_folder2.Get(ID));
  EXPECT_EQ(parent_folder2.Get(ID), child.Get(PARENT_ID));
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder.Get(ID), child_name));
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder2.Get(ID), child_name));
}

TEST_F(SyncableDirectoryTest, TestNoReindexDeletedItems) {
  std::string folder_name = "folder";
  std::string new_name = "new_name";

  WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), folder_name);
  ASSERT_TRUE(folder.good());
  ASSERT_TRUE(folder.Put(IS_DIR, true));
  ASSERT_TRUE(folder.Put(IS_DEL, true));

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));

  MutableEntry deleted(&trans, GET_BY_ID, folder.Get(ID));
  ASSERT_TRUE(deleted.good());
  ASSERT_TRUE(deleted.Put(PARENT_ID, trans.root_id()));
  ASSERT_TRUE(deleted.Put(NON_UNIQUE_NAME, new_name));

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));
  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), new_name));
}

TEST_F(SyncableDirectoryTest, TestCaseChangeRename) {
  WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "CaseChange");
  ASSERT_TRUE(folder.good());
  EXPECT_TRUE(folder.Put(PARENT_ID, trans.root_id()));
  EXPECT_TRUE(folder.Put(NON_UNIQUE_NAME, "CASECHANGE"));
  EXPECT_TRUE(folder.Put(IS_DEL, true));
}

// Create items of each model type, and check that GetModelType and
// GetServerModelType return the right value.
TEST_F(SyncableDirectoryTest, GetModelType) {
  TestIdFactory id_factory;
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    ModelType datatype = iter.Get();
    SCOPED_TRACE(testing::Message("Testing model type ") << datatype);
    switch (datatype) {
      case UNSPECIFIED:
      case TOP_LEVEL_FOLDER:
        continue;  // Datatype isn't a function of Specifics.
      default:
        break;
    }
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(datatype, &specifics);

    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "Folder");
    ASSERT_TRUE(folder.good());
    folder.Put(ID, id_factory.NewServerId());
    folder.Put(SPECIFICS, specifics);
    folder.Put(BASE_VERSION, 1);
    folder.Put(IS_DIR, true);
    folder.Put(IS_DEL, false);
    ASSERT_EQ(datatype, folder.GetModelType());

    MutableEntry item(&trans, CREATE, BOOKMARKS, trans.root_id(), "Item");
    ASSERT_TRUE(item.good());
    item.Put(ID, id_factory.NewServerId());
    item.Put(SPECIFICS, specifics);
    item.Put(BASE_VERSION, 1);
    item.Put(IS_DIR, false);
    item.Put(IS_DEL, false);
    ASSERT_EQ(datatype, item.GetModelType());

    // It's critical that deletion records retain their datatype, so that
    // they can be dispatched to the appropriate change processor.
    MutableEntry deleted_item(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "Deleted Item");
    ASSERT_TRUE(item.good());
    deleted_item.Put(ID, id_factory.NewServerId());
    deleted_item.Put(SPECIFICS, specifics);
    deleted_item.Put(BASE_VERSION, 1);
    deleted_item.Put(IS_DIR, false);
    deleted_item.Put(IS_DEL, true);
    ASSERT_EQ(datatype, deleted_item.GetModelType());

    MutableEntry server_folder(&trans, CREATE_NEW_UPDATE_ITEM,
        id_factory.NewServerId());
    ASSERT_TRUE(server_folder.good());
    server_folder.Put(SERVER_SPECIFICS, specifics);
    server_folder.Put(BASE_VERSION, 1);
    server_folder.Put(SERVER_IS_DIR, true);
    server_folder.Put(SERVER_IS_DEL, false);
    ASSERT_EQ(datatype, server_folder.GetServerModelType());

    MutableEntry server_item(&trans, CREATE_NEW_UPDATE_ITEM,
        id_factory.NewServerId());
    ASSERT_TRUE(server_item.good());
    server_item.Put(SERVER_SPECIFICS, specifics);
    server_item.Put(BASE_VERSION, 1);
    server_item.Put(SERVER_IS_DIR, false);
    server_item.Put(SERVER_IS_DEL, false);
    ASSERT_EQ(datatype, server_item.GetServerModelType());

    sync_pb::SyncEntity folder_entity;
    folder_entity.set_id_string(SyncableIdToProto(id_factory.NewServerId()));
    folder_entity.set_deleted(false);
    folder_entity.set_folder(true);
    folder_entity.mutable_specifics()->CopyFrom(specifics);
    ASSERT_EQ(datatype, GetModelType(folder_entity));

    sync_pb::SyncEntity item_entity;
    item_entity.set_id_string(SyncableIdToProto(id_factory.NewServerId()));
    item_entity.set_deleted(false);
    item_entity.set_folder(false);
    item_entity.mutable_specifics()->CopyFrom(specifics);
    ASSERT_EQ(datatype, GetModelType(item_entity));
  }
}

// A test that roughly mimics the directory interaction that occurs when a
// bookmark folder and entry are created then synced for the first time.  It is
// a more common variant of the 'DeletedAndUnsyncedChild' scenario tested below.
TEST_F(SyncableDirectoryTest, ChangeEntryIDAndUpdateChildren_ParentAndChild) {
  TestIdFactory id_factory;
  Id orig_parent_id;
  Id orig_child_id;

  {
    // Create two client-side items, a parent and child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.Get(ID), "child");
    child.Put(IS_UNSYNCED, true);

    orig_parent_id = parent.Get(ID);
    orig_child_id = child.Get(ID);
  }

  {
    // Simulate what happens after committing two items.  Their IDs will be
    // replaced with server IDs.  The child is renamed first, then the parent.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);
    MutableEntry child(&trans, GET_BY_ID, orig_child_id);

    ChangeEntryIDAndUpdateChildren(&trans, &child, id_factory.NewServerId());
    child.Put(IS_UNSYNCED, false);
    child.Put(BASE_VERSION, 1);
    child.Put(SERVER_VERSION, 1);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.Put(IS_UNSYNCED, false);
    parent.Put(BASE_VERSION, 1);
    parent.Put(SERVER_VERSION, 1);
  }

  // Final check for validity.
  EXPECT_EQ(OPENED, SimulateSaveAndReloadDir());
}

// A test based on the scenario where we create a bookmark folder and entry
// locally, but with a twist.  In this case, the bookmark is deleted before we
// are able to sync either it or its parent folder.  This scenario used to cause
// directory corruption, see crbug.com/125381.
TEST_F(SyncableDirectoryTest,
       ChangeEntryIDAndUpdateChildren_DeletedAndUnsyncedChild) {
  TestIdFactory id_factory;
  Id orig_parent_id;
  Id orig_child_id;

  {
    // Create two client-side items, a parent and child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.Get(ID), "child");
    child.Put(IS_UNSYNCED, true);

    orig_parent_id = parent.Get(ID);
    orig_child_id = child.Get(ID);
  }

  {
    // Delete the child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry child(&trans, GET_BY_ID, orig_child_id);
    child.Put(IS_DEL, true);
  }

  {
    // Simulate what happens after committing the parent.  Its ID will be
    // replaced with server a ID.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.Put(IS_UNSYNCED, false);
    parent.Put(BASE_VERSION, 1);
    parent.Put(SERVER_VERSION, 1);
  }

  // Final check for validity.
  EXPECT_EQ(OPENED, SimulateSaveAndReloadDir());
}

// Ask the directory to generate a unique ID.  Close and re-open the database
// without saving, then ask for another unique ID.  Verify IDs are not reused.
// This scenario simulates a crash within the first few seconds of operation.
TEST_F(SyncableDirectoryTest, LocalIdReuseTest) {
  Id pre_crash_id = dir_->NextId();
  SimulateCrashAndReloadDir();
  Id post_crash_id = dir_->NextId();
  EXPECT_NE(pre_crash_id, post_crash_id);
}

// Ask the directory to generate a unique ID.  Save the directory.  Close and
// re-open the database without saving, then ask for another unique ID.  Verify
// IDs are not reused.  This scenario simulates a steady-state crash.
TEST_F(SyncableDirectoryTest, LocalIdReuseTestWithSave) {
  Id pre_crash_id = dir_->NextId();
  dir_->SaveChanges();
  SimulateCrashAndReloadDir();
  Id post_crash_id = dir_->NextId();
  EXPECT_NE(pre_crash_id, post_crash_id);
}

// Ensure that the unsynced, is_del and server unkown entries that may have been
// left in the database by old clients will be deleted when we open the old
// database.
TEST_F(SyncableDirectoryTest, OldClientLeftUnsyncedDeletedLocalItem) {
  // We must create an entry with the offending properties.  This is done with
  // some abuse of the MutableEntry's API; it doesn't expect us to modify an
  // item after it is deleted.  If this hack becomes impractical we will need to
  // find a new way to simulate this scenario.

  TestIdFactory id_factory;

  // Happy-path: These valid entries should not get deleted.
  Id server_knows_id = id_factory.NewServerId();
  Id not_is_del_id = id_factory.NewLocalId();

  // The ID of the entry which will be unsynced, is_del and !ServerKnows().
  Id zombie_id = id_factory.NewLocalId();

  // We're about to do some bad things.  Tell the directory verification
  // routines to look the other way.
  dir_->SetInvariantCheckLevel(OFF);

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    // Create an uncommitted tombstone entry.
    MutableEntry server_knows(&trans, CREATE, BOOKMARKS, id_factory.root(),
                              "server_knows");
    server_knows.Put(ID, server_knows_id);
    server_knows.Put(IS_UNSYNCED, true);
    server_knows.Put(IS_DEL, true);
    server_knows.Put(BASE_VERSION, 5);
    server_knows.Put(SERVER_VERSION, 4);

    // Create a valid update entry.
    MutableEntry not_is_del(
        &trans, CREATE, BOOKMARKS, id_factory.root(), "not_is_del");
    not_is_del.Put(ID, not_is_del_id);
    not_is_del.Put(IS_DEL, false);
    not_is_del.Put(IS_UNSYNCED, true);

    // Create a tombstone which should never be sent to the server because the
    // server never knew about the item's existence.
    //
    // New clients should never put entries into this state.  We work around
    // this by setting IS_DEL before setting IS_UNSYNCED, something which the
    // client should never do in practice.
    MutableEntry zombie(&trans, CREATE, BOOKMARKS, id_factory.root(), "zombie");
    zombie.Put(ID, zombie_id);
    zombie.Put(IS_DEL, true);
    zombie.Put(IS_UNSYNCED, true);
  }

  ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());

  {
    ReadTransaction trans(FROM_HERE, dir_.get());

    // The directory loading routines should have cleaned things up, making it
    // safe to check invariants once again.
    dir_->FullyCheckTreeInvariants(&trans);

    Entry server_knows(&trans, GET_BY_ID, server_knows_id);
    EXPECT_TRUE(server_knows.good());

    Entry not_is_del(&trans, GET_BY_ID, not_is_del_id);
    EXPECT_TRUE(not_is_del.good());

    Entry zombie(&trans, GET_BY_ID, zombie_id);
    EXPECT_FALSE(zombie.good());
  }
}

TEST_F(SyncableDirectoryTest, OrdinalWithNullSurvivesSaveAndReload) {
  TestIdFactory id_factory;
  Id null_child_id;
  const char null_cstr[] = "\0null\0test";
  std::string null_str(null_cstr, arraysize(null_cstr) - 1);
  NodeOrdinal null_ord = NodeOrdinal(null_str);

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.Get(ID), "child");
    child.Put(IS_UNSYNCED, true);
    child.Put(SERVER_ORDINAL_IN_PARENT, null_ord);

    null_child_id = child.Get(ID);
  }

  EXPECT_EQ(OPENED, SimulateSaveAndReloadDir());

  {
    ReadTransaction trans(FROM_HERE, dir_.get());

    Entry null_ordinal_child(&trans, GET_BY_ID, null_child_id);
    EXPECT_TRUE(
        null_ord.Equals(null_ordinal_child.Get(SERVER_ORDINAL_IN_PARENT)));
  }

}

// An OnDirectoryBackingStore that can be set to always fail SaveChanges.
class TestBackingStore : public OnDiskDirectoryBackingStore {
 public:
  TestBackingStore(const std::string& dir_name,
                   const base::FilePath& backing_filepath);

  virtual ~TestBackingStore();

  virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot)
      OVERRIDE;

   void StartFailingSaveChanges() {
     fail_save_changes_ = true;
   }

 private:
   bool fail_save_changes_;
};

TestBackingStore::TestBackingStore(const std::string& dir_name,
                                   const base::FilePath& backing_filepath)
  : OnDiskDirectoryBackingStore(dir_name, backing_filepath),
    fail_save_changes_(false) {
}

TestBackingStore::~TestBackingStore() { }

bool TestBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot){
  if (fail_save_changes_) {
    return false;
  } else {
    return OnDiskDirectoryBackingStore::SaveChanges(snapshot);
  }
}

// A directory whose Save() function can be set to always fail.
class TestDirectory : public Directory {
 public:
  // A factory function used to work around some initialization order issues.
  static TestDirectory* Create(
      Encryptor *encryptor,
      UnrecoverableErrorHandler *handler,
      const std::string& dir_name,
      const base::FilePath& backing_filepath);

  virtual ~TestDirectory();

  void StartFailingSaveChanges() {
    backing_store_->StartFailingSaveChanges();
  }

 private:
  TestDirectory(Encryptor* encryptor,
                UnrecoverableErrorHandler* handler,
                TestBackingStore* backing_store);

  TestBackingStore* backing_store_;
};

TestDirectory* TestDirectory::Create(
    Encryptor *encryptor,
    UnrecoverableErrorHandler *handler,
    const std::string& dir_name,
    const base::FilePath& backing_filepath) {
  TestBackingStore* backing_store =
      new TestBackingStore(dir_name, backing_filepath);
  return new TestDirectory(encryptor, handler, backing_store);
}

TestDirectory::TestDirectory(Encryptor* encryptor,
                             UnrecoverableErrorHandler* handler,
                             TestBackingStore* backing_store)
    : Directory(backing_store, handler, NULL, NULL, NULL),
      backing_store_(backing_store) {
}

TestDirectory::~TestDirectory() { }

TEST(OnDiskSyncableDirectory, FailInitialWrite) {
  FakeEncryptor encryptor;
  TestUnrecoverableErrorHandler handler;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.path().Append(
      FILE_PATH_LITERAL("Test.sqlite3"));
  std::string name = "user@x.com";
  NullDirectoryChangeDelegate delegate;

  scoped_ptr<TestDirectory> test_dir(
      TestDirectory::Create(&encryptor, &handler, name, file_path));

  test_dir->StartFailingSaveChanges();
  ASSERT_EQ(FAILED_INITIAL_WRITE, test_dir->Open(name, &delegate,
                                                 NullTransactionObserver()));
}

// A variant of SyncableDirectoryTest that uses a real sqlite database.
class OnDiskSyncableDirectoryTest : public SyncableDirectoryTest {
 protected:
  // SetUp() is called before each test case is run.
  // The sqlite3 DB is deleted before each test is run.
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.path().Append(
        FILE_PATH_LITERAL("Test.sqlite3"));
    file_util::Delete(file_path_, true);
    CreateDirectory();
  }

  virtual void TearDown() {
    // This also closes file handles.
    dir_->SaveChanges();
    dir_.reset();
    file_util::Delete(file_path_, true);
  }

  // Creates a new directory.  Deletes the old directory, if it exists.
  void CreateDirectory() {
    test_directory_ =
        TestDirectory::Create(&encryptor_, &handler_, kName, file_path_);
    dir_.reset(test_directory_);
    ASSERT_TRUE(dir_.get());
    ASSERT_EQ(OPENED, dir_->Open(kName, &delegate_,
                                 NullTransactionObserver()));
    ASSERT_TRUE(dir_->good());
  }

  void SaveAndReloadDir() {
    dir_->SaveChanges();
    CreateDirectory();
  }

  void StartFailingSaveChanges() {
    test_directory_->StartFailingSaveChanges();
  }

  TestDirectory *test_directory_;  // mirrors scoped_ptr<Directory> dir_
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
};

TEST_F(OnDiskSyncableDirectoryTest, TestPurgeEntriesWithTypeIn) {
  sync_pb::EntitySpecifics bookmark_specs;
  sync_pb::EntitySpecifics autofill_specs;
  sync_pb::EntitySpecifics preference_specs;
  AddDefaultFieldValue(BOOKMARKS, &bookmark_specs);
  AddDefaultFieldValue(PREFERENCES, &preference_specs);
  AddDefaultFieldValue(AUTOFILL, &autofill_specs);

  ModelTypeSet types_to_purge(PREFERENCES, AUTOFILL);

  TestIdFactory id_factory;
  // Create some items for each type.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    // Make it look like these types have completed initial sync.
    CreateTypeRoot(&trans, dir_.get(), BOOKMARKS);
    CreateTypeRoot(&trans, dir_.get(), PREFERENCES);
    CreateTypeRoot(&trans, dir_.get(), AUTOFILL);

    // Add more nodes for this type.  Technically, they should be placed under
    // the proper type root nodes but the assertions in this test won't notice
    // if their parent isn't quite right.
    MutableEntry item1(&trans, CREATE, BOOKMARKS, trans.root_id(), "Item");
    ASSERT_TRUE(item1.good());
    item1.Put(SERVER_SPECIFICS, bookmark_specs);
    item1.Put(IS_UNSYNCED, true);

    MutableEntry item2(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item2.good());
    item2.Put(SERVER_SPECIFICS, bookmark_specs);
    item2.Put(IS_UNAPPLIED_UPDATE, true);

    MutableEntry item3(&trans, CREATE, PREFERENCES,
                       trans.root_id(), "Item");
    ASSERT_TRUE(item3.good());
    item3.Put(SPECIFICS, preference_specs);
    item3.Put(SERVER_SPECIFICS, preference_specs);
    item3.Put(IS_UNSYNCED, true);

    MutableEntry item4(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item4.good());
    item4.Put(SERVER_SPECIFICS, preference_specs);
    item4.Put(IS_UNAPPLIED_UPDATE, true);

    MutableEntry item5(&trans, CREATE, AUTOFILL,
                       trans.root_id(), "Item");
    ASSERT_TRUE(item5.good());
    item5.Put(SPECIFICS, autofill_specs);
    item5.Put(SERVER_SPECIFICS, autofill_specs);
    item5.Put(IS_UNSYNCED, true);

    MutableEntry item6(&trans, CREATE_NEW_UPDATE_ITEM,
      id_factory.NewServerId());
    ASSERT_TRUE(item6.good());
    item6.Put(SERVER_SPECIFICS, autofill_specs);
    item6.Put(IS_UNAPPLIED_UPDATE, true);
  }

  dir_->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    MetahandleSet all_set;
    GetAllMetaHandles(&trans, &all_set);
    ASSERT_EQ(10U, all_set.size());
  }

  dir_->PurgeEntriesWithTypeIn(types_to_purge, ModelTypeSet());

  // We first query the in-memory data, and then reload the directory (without
  // saving) to verify that disk does not still have the data.
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, true);
  SaveAndReloadDir();
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, false);
}

TEST_F(OnDiskSyncableDirectoryTest, TestShareInfo) {
  dir_->set_store_birthday("Jan 31st");
  const char* const bag_of_chips_array = "\0bag of chips";
  const std::string bag_of_chips_string =
      std::string(bag_of_chips_array, sizeof(bag_of_chips_array));
  dir_->set_bag_of_chips(bag_of_chips_string);
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_EQ("Jan 31st", dir_->store_birthday());
    EXPECT_EQ(bag_of_chips_string, dir_->bag_of_chips());
  }
  dir_->set_store_birthday("April 10th");
  const char* const bag_of_chips2_array = "\0bag of chips2";
  const std::string bag_of_chips2_string =
      std::string(bag_of_chips2_array, sizeof(bag_of_chips2_array));
  dir_->set_bag_of_chips(bag_of_chips2_string);
  dir_->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_EQ("April 10th", dir_->store_birthday());
    EXPECT_EQ(bag_of_chips2_string, dir_->bag_of_chips());
  }
  const char* const bag_of_chips3_array = "\0bag of chips3";
  const std::string bag_of_chips3_string =
      std::string(bag_of_chips3_array, sizeof(bag_of_chips3_array));
  dir_->set_bag_of_chips(bag_of_chips3_string);
  // Restore the directory from disk.  Make sure that nothing's changed.
  SaveAndReloadDir();
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_EQ("April 10th", dir_->store_birthday());
    EXPECT_EQ(bag_of_chips3_string, dir_->bag_of_chips());
  }
}

TEST_F(OnDiskSyncableDirectoryTest,
       TestSimpleFieldsPreservedDuringSaveChanges) {
  Id update_id = TestIdFactory::FromNumber(1);
  Id create_id;
  EntryKernel create_pre_save, update_pre_save;
  EntryKernel create_post_save, update_post_save;
  std::string create_name =  "Create";

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry create(
        &trans, CREATE, BOOKMARKS, trans.root_id(), create_name);
    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, update_id);
    create.Put(IS_UNSYNCED, true);
    update.Put(IS_UNAPPLIED_UPDATE, true);
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_favicon("PNG");
    specifics.mutable_bookmark()->set_url("http://nowhere");
    create.Put(SPECIFICS, specifics);
    update.Put(SPECIFICS, specifics);
    create_pre_save = create.GetKernelCopy();
    update_pre_save = update.GetKernelCopy();
    create_id = create.Get(ID);
  }

  dir_->SaveChanges();
  dir_.reset(new Directory(new OnDiskDirectoryBackingStore(kName, file_path_),
                           &handler_,
                           NULL,
                           NULL,
                           NULL));

  ASSERT_TRUE(dir_.get());
  ASSERT_EQ(OPENED, dir_->Open(kName, &delegate_, NullTransactionObserver()));
  ASSERT_TRUE(dir_->good());

  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    Entry create(&trans, GET_BY_ID, create_id);
    EXPECT_EQ(1, CountEntriesWithName(&trans, trans.root_id(), create_name));
    Entry update(&trans, GET_BY_ID, update_id);
    create_post_save = create.GetKernelCopy();
    update_post_save = update.GetKernelCopy();
  }
  int i = BEGIN_FIELDS;
  for ( ; i < INT64_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((Int64Field)i) +
                  (i == TRANSACTION_VERSION ? 1 : 0),
              create_post_save.ref((Int64Field)i))
              << "int64 field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((Int64Field)i) +
              (i == TRANSACTION_VERSION ? 1 : 0),
              update_post_save.ref((Int64Field)i))
              << "int64 field #" << i << " changed during save/load";
  }
  for ( ; i < TIME_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((TimeField)i),
              create_post_save.ref((TimeField)i))
              << "time field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((TimeField)i),
              update_post_save.ref((TimeField)i))
              << "time field #" << i << " changed during save/load";
  }
  for ( ; i < ID_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((IdField)i),
              create_post_save.ref((IdField)i))
              << "id field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((IdField)i),
              update_pre_save.ref((IdField)i))
              << "id field #" << i << " changed during save/load";
  }
  for ( ; i < BIT_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((BitField)i),
              create_post_save.ref((BitField)i))
              << "Bit field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((BitField)i),
              update_post_save.ref((BitField)i))
              << "Bit field #" << i << " changed during save/load";
  }
  for ( ; i < STRING_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((StringField)i),
              create_post_save.ref((StringField)i))
              << "String field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((StringField)i),
              update_post_save.ref((StringField)i))
              << "String field #" << i << " changed during save/load";
  }
  for ( ; i < PROTO_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((ProtoField)i).SerializeAsString(),
              create_post_save.ref((ProtoField)i).SerializeAsString())
              << "Blob field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((ProtoField)i).SerializeAsString(),
              update_post_save.ref((ProtoField)i).SerializeAsString())
              << "Blob field #" << i << " changed during save/load";
  }
  for ( ; i < ORDINAL_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((OrdinalField)i).ToInternalValue(),
              create_post_save.ref((OrdinalField)i).ToInternalValue())
              << "Blob field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((OrdinalField)i).ToInternalValue(),
              update_post_save.ref((OrdinalField)i).ToInternalValue())
              << "Blob field #" << i << " changed during save/load";
  }
}

TEST_F(OnDiskSyncableDirectoryTest, TestSaveChangesFailure) {
  int64 handle1 = 0;
  // Set up an item using a regular, saveable directory.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  // Make sure the item is no longer dirty after saving,
  // and make a modification.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.Get(NON_UNIQUE_NAME), "aguilera");
    aguilera.Put(NON_UNIQUE_NAME, "overwritten");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  // Now do some operations when SaveChanges() will fail.
  StartFailingSaveChanges();
  ASSERT_TRUE(dir_->good());

  int64 handle2 = 0;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.Get(NON_UNIQUE_NAME), "overwritten");
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_FALSE(IsInDirtyMetahandles(handle1));
    aguilera.Put(NON_UNIQUE_NAME, "christina");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));

    // New item.
    MutableEntry kids_on_block(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "kids");
    ASSERT_TRUE(kids_on_block.good());
    handle2 = kids_on_block.Get(META_HANDLE);
    kids_on_block.Put(BASE_VERSION, 1);
    kids_on_block.Put(IS_DIR, true);
    kids_on_block.Put(ID, TestIdFactory::FromNumber(102));
    EXPECT_TRUE(kids_on_block.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle2));
  }

  // We are using an unsaveable directory, so this can't succeed.  However,
  // the HandleSaveChangesFailure code path should have been triggered.
  ASSERT_FALSE(dir_->SaveChanges());

  // Make sure things were rolled back and the world is as it was before call.
  {
     ReadTransaction trans(FROM_HERE, dir_.get());
     Entry e1(&trans, GET_BY_HANDLE, handle1);
     ASSERT_TRUE(e1.good());
     EntryKernel aguilera = e1.GetKernelCopy();
     Entry kids(&trans, GET_BY_HANDLE, handle2);
     ASSERT_TRUE(kids.good());
     EXPECT_TRUE(kids.GetKernelCopy().is_dirty());
     EXPECT_TRUE(IsInDirtyMetahandles(handle2));
     EXPECT_TRUE(aguilera.is_dirty());
     EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
}

TEST_F(OnDiskSyncableDirectoryTest, TestSaveChangesFailureWithPurge) {
  int64 handle1 = 0;
  // Set up an item using a regular, saveable directory.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    sync_pb::EntitySpecifics bookmark_specs;
    AddDefaultFieldValue(BOOKMARKS, &bookmark_specs);
    e1.Put(SPECIFICS, bookmark_specs);
    e1.Put(SERVER_SPECIFICS, bookmark_specs);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  // Now do some operations while SaveChanges() is set to fail.
  StartFailingSaveChanges();
  ASSERT_TRUE(dir_->good());

  ModelTypeSet set(BOOKMARKS);
  dir_->PurgeEntriesWithTypeIn(set, ModelTypeSet());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
  ASSERT_FALSE(dir_->SaveChanges());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
}

}  // namespace

void SyncableDirectoryTest::ValidateEntry(BaseTransaction* trans,
                                          int64 id,
                                          bool check_name,
                                          const std::string& name,
                                          int64 base_version,
                                          int64 server_version,
                                          bool is_del) {
  Entry e(trans, GET_BY_ID, TestIdFactory::FromNumber(id));
  ASSERT_TRUE(e.good());
  if (check_name)
    ASSERT_TRUE(name == e.Get(NON_UNIQUE_NAME));
  ASSERT_TRUE(base_version == e.Get(BASE_VERSION));
  ASSERT_TRUE(server_version == e.Get(SERVER_VERSION));
  ASSERT_TRUE(is_del == e.Get(IS_DEL));
}

DirOpenResult SyncableDirectoryTest::SimulateSaveAndReloadDir() {
  if (!dir_->SaveChanges())
    return FAILED_IN_UNITTEST;

  return ReloadDirImpl();
}

DirOpenResult SyncableDirectoryTest::SimulateCrashAndReloadDir() {
  return ReloadDirImpl();
}

DirOpenResult SyncableDirectoryTest::ReloadDirImpl() {
  // Do some tricky things to preserve the backing store.
  DirectoryBackingStore* saved_store = dir_->store_.release();

  // Close the current directory.
  dir_->Close();
  dir_.reset();

  dir_.reset(new Directory(saved_store,
                           &handler_,
                           NULL,
                           NULL,
                           NULL));
  DirOpenResult result = dir_->OpenImpl(kName, &delegate_,
                                        NullTransactionObserver());

  // If something went wrong, we need to clear this member.  If we don't,
  // TearDown() will be guaranteed to crash when it calls SaveChanges().
  if (result != OPENED)
    dir_.reset();

  return result;
}

namespace {

class SyncableDirectoryManagement : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
  }
 protected:
  MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  NullDirectoryChangeDelegate delegate_;
};

TEST_F(SyncableDirectoryManagement, TestFileRelease) {
  base::FilePath path = temp_dir_.path().Append(
      Directory::kSyncDatabaseFilename);

  syncable::Directory dir(new OnDiskDirectoryBackingStore("ScopeTest", path),
                          &handler_,
                          NULL,
                          NULL,
                          NULL);
  DirOpenResult result =
      dir.Open("ScopeTest", &delegate_, NullTransactionObserver());
  ASSERT_EQ(result, OPENED);
  dir.Close();

  // Closing the directory should have released the backing database file.
  ASSERT_TRUE(file_util::Delete(path, true));
}

class StressTransactionsDelegate : public base::PlatformThread::Delegate {
 public:
  StressTransactionsDelegate(Directory* dir, int thread_number)
      : dir_(dir),
        thread_number_(thread_number) {}

 private:
  Directory* const dir_;
  const int thread_number_;

  // PlatformThread::Delegate methods:
  virtual void ThreadMain() OVERRIDE {
    int entry_count = 0;
    std::string path_name;

    for (int i = 0; i < 20; ++i) {
      const int rand_action = rand() % 10;
      if (rand_action < 4 && !path_name.empty()) {
        ReadTransaction trans(FROM_HERE, dir_);
        CHECK(1 == CountEntriesWithName(&trans, trans.root_id(), path_name));
        base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
            rand() % 10));
      } else {
        std::string unique_name =
            base::StringPrintf("%d.%d", thread_number_, entry_count++);
        path_name.assign(unique_name.begin(), unique_name.end());
        WriteTransaction trans(FROM_HERE, UNITTEST, dir_);
        MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), path_name);
        CHECK(e.good());
        base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
            rand() % 20));
        e.Put(IS_UNSYNCED, true);
        if (e.Put(ID, TestIdFactory::FromNumber(rand())) &&
            e.Get(ID).ServerKnows() && !e.Get(ID).IsRoot()) {
           e.Put(BASE_VERSION, 1);
        }
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(StressTransactionsDelegate);
};

TEST(SyncableDirectory, StressTransactions) {
  MessageLoop message_loop;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FakeEncryptor encryptor;
  TestUnrecoverableErrorHandler handler;
  NullDirectoryChangeDelegate delegate;
  std::string dirname = "stress";
  Directory dir(new InMemoryDirectoryBackingStore(dirname),
                &handler,
                NULL,
                NULL,
                NULL);
  dir.Open(dirname, &delegate, NullTransactionObserver());

  const int kThreadCount = 7;
  base::PlatformThreadHandle threads[kThreadCount];
  scoped_ptr<StressTransactionsDelegate> thread_delegates[kThreadCount];

  for (int i = 0; i < kThreadCount; ++i) {
    thread_delegates[i].reset(new StressTransactionsDelegate(&dir, i));
    ASSERT_TRUE(base::PlatformThread::Create(
        0, thread_delegates[i].get(), &threads[i]));
  }

  for (int i = 0; i < kThreadCount; ++i) {
    base::PlatformThread::Join(threads[i]);
  }

  dir.Close();
}

class SyncableClientTagTest : public SyncableDirectoryTest {
 public:
  static const int kBaseVersion = 1;
  const char* test_name_;
  const char* test_tag_;

  SyncableClientTagTest() : test_name_("test_name"), test_tag_("dietcoke") {}

  bool CreateWithDefaultTag(Id id, bool deleted) {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry me(&wtrans, CREATE, PREFERENCES,
                    wtrans.root_id(), test_name_);
    CHECK(me.good());
    me.Put(ID, id);
    if (id.ServerKnows()) {
      me.Put(BASE_VERSION, kBaseVersion);
    }
    me.Put(IS_UNSYNCED, true);
    me.Put(IS_DEL, deleted);
    me.Put(IS_DIR, false);
    return me.Put(UNIQUE_CLIENT_TAG, test_tag_);
  }

  // Verify an entry exists with the default tag.
  void VerifyTag(Id id, bool deleted) {
    // Should still be present and valid in the client tag index.
    ReadTransaction trans(FROM_HERE, dir_.get());
    Entry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    CHECK(me.good());
    EXPECT_EQ(me.Get(ID), id);
    EXPECT_EQ(me.Get(UNIQUE_CLIENT_TAG), test_tag_);
    EXPECT_EQ(me.Get(IS_DEL), deleted);

    // We only sync deleted items that the server knew about.
    if (me.Get(ID).ServerKnows() || !me.Get(IS_DEL)) {
      EXPECT_EQ(me.Get(IS_UNSYNCED), true);
    }
  }

 protected:
  TestIdFactory factory_;
};

TEST_F(SyncableClientTagTest, TestClientTagClear) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_TRUE(me.good());
    me.Put(UNIQUE_CLIENT_TAG, "");
  }
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    Entry by_tag(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_FALSE(by_tag.good());

    Entry by_id(&trans, GET_BY_ID, server_id);
    EXPECT_TRUE(by_id.good());
    EXPECT_TRUE(by_id.Get(UNIQUE_CLIENT_TAG).empty());
  }
}

TEST_F(SyncableClientTagTest, TestClientTagIndexServerId) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  VerifyTag(server_id, false);
}

TEST_F(SyncableClientTagTest, TestClientTagIndexClientId) {
  Id client_id = factory_.NewLocalId();
  EXPECT_TRUE(CreateWithDefaultTag(client_id, false));
  VerifyTag(client_id, false);
}

TEST_F(SyncableClientTagTest, TestDeletedClientTagIndexClientId) {
  Id client_id = factory_.NewLocalId();
  EXPECT_TRUE(CreateWithDefaultTag(client_id, true));
  VerifyTag(client_id, true);
}

TEST_F(SyncableClientTagTest, TestDeletedClientTagIndexServerId) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, true));
  VerifyTag(server_id, true);
}

TEST_F(SyncableClientTagTest, TestClientTagIndexDuplicateServer) {
  EXPECT_TRUE(CreateWithDefaultTag(factory_.NewServerId(), true));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewServerId(), true));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewServerId(), false));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewLocalId(), false));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewLocalId(), true));
}

}  // namespace
}  // namespace syncable
}  // namespace syncer
