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
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/test/values_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/syncable/directory_backing_store.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/syncable/directory_unittest.h"
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

namespace {

void PutDataAsBookmarkFavicon(WriteTransaction* wtrans,
                              MutableEntry* e,
                              const char* bytes,
                              size_t bytes_length) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_url("http://demo/");
  specifics.mutable_bookmark()->set_favicon(bytes, bytes_length);
  e->PutSpecifics(specifics);
}

void ExpectDataFromBookmarkFaviconEquals(BaseTransaction* trans,
                                         Entry* e,
                                         const char* bytes,
                                         size_t bytes_length) {
  ASSERT_TRUE(e->good());
  ASSERT_TRUE(e->GetSpecifics().has_bookmark());
  ASSERT_EQ("http://demo/", e->GetSpecifics().bookmark().url());
  ASSERT_EQ(std::string(bytes, bytes_length),
            e->GetSpecifics().bookmark().favicon());
}

}  // namespace

class SyncableGeneralTest : public testing::Test {
 public:
  static const char kIndexTestName[];
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ =
        temp_dir_.path().Append(FILE_PATH_LITERAL("SyncableTest.sqlite3"));
  }

  virtual void TearDown() {}

 protected:
  base::MessageLoop message_loop_;
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

  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  // Test simple read operations on an empty DB.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Directory::Metahandles child_handles;
    dir.GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_TRUE(child_handles.empty());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    written_metahandle = me.GetMetahandle();
  }

  // Test GetChildHandles* after something is now in the DB.
  // Also check that GET_BY_ID works.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Directory::Metahandles child_handles;
    dir.GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (Directory::Metahandles::iterator i = child_handles.begin();
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
    e.PutIsDel(true);

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

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_FALSE(dir.HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_TRUE(root.GetFirstChildId().IsRoot());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    written_metahandle = me.GetMetahandle();
  }

  // Test children ops after something is now in the DB.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry child(&rtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(child.good());

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_TRUE(dir.HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_EQ(e.GetId(), root.GetFirstChildId());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(me.good());
    me.PutIsDel(true);
  }

  // Test children ops after the children have been deleted.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_FALSE(dir.HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_TRUE(root.GetFirstChildId().IsRoot());
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
      me.PutId(id);
      me.PutBaseVersion(1);
      me.PutUniqueClientTag(tag);
      written_metahandle = me.GetMetahandle();
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
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetBaseVersion(), 1);
    EXPECT_EQ(me.GetUniqueClientTag(), tag);
    EXPECT_EQ(me.GetMetahandle(), written_metahandle);
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
      me.PutId(id);
      me.PutBaseVersion(1);
      me.PutUniqueClientTag(tag);
      me.PutIsDel(true);
      me.PutIsUnsynced(true);  // Or it might be purged.
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
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetUniqueClientTag(), tag);
    EXPECT_TRUE(me.GetIsDel());
    EXPECT_TRUE(me.GetIsUnsynced());
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

    scoped_ptr<base::DictionaryValue> value(e.ToValue(NULL));
    ExpectDictBooleanValue(false, *value, "good");
    EXPECT_EQ(1u, value->size());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "new");
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);

    scoped_ptr<base::DictionaryValue> value(me.ToValue(NULL));
    ExpectDictBooleanValue(true, *value, "good");
    EXPECT_TRUE(value->HasKey("kernel"));
    ExpectDictStringValue("Bookmarks", *value, "modelType");
    ExpectDictBooleanValue(true, *value, "existsOnClientBecauseNameIsNonEmpty");
    ExpectDictBooleanValue(false, *value, "isRoot");
  }

  dir.SaveChanges();
}

// Test that the bookmark tag generation algorithm remains unchanged.
TEST_F(SyncableGeneralTest, BookmarkTagTest) {
  InMemoryDirectoryBackingStore* store = new InMemoryDirectoryBackingStore("x");

  // The two inputs that form the bookmark tag are the directory's cache_guid
  // and its next_id value.  We don't need to take any action to ensure
  // consistent next_id values, but we do need to explicitly request that our
  // InMemoryDirectoryBackingStore always return the same cache_guid.
  store->request_consistent_cache_guid();

  Directory dir(store, &handler_, NULL, NULL, NULL);
  ASSERT_EQ(OPENED, dir.Open("x", &delegate_, NullTransactionObserver()));

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry bm(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "bm");
    bm.PutIsUnsynced(true);

    // If this assertion fails, that might indicate that the algorithm used to
    // generate bookmark tags has been modified.  This could have implications
    // for bookmark ordering.  Please make sure you know what you're doing if
    // you intend to make such a change.
    ASSERT_EQ("6wHRAb3kbnXV5GHrejp4/c1y5tw=", bm.GetUniqueBookmarkTag());
  }
}

// An OnDiskDirectoryBackingStore that can be set to always fail SaveChanges.
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
    SyncableDirectoryTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.path().Append(
        FILE_PATH_LITERAL("Test.sqlite3"));
    base::DeleteFile(file_path_, true);
    CreateDirectory();
  }

  virtual void TearDown() {
    // This also closes file handles.
    dir()->SaveChanges();
    dir().reset();
    base::DeleteFile(file_path_, true);
    SyncableDirectoryTest::TearDown();
  }

  // Creates a new directory.  Deletes the old directory, if it exists.
  void CreateDirectory() {
    test_directory_ = TestDirectory::Create(
        encryptor(), unrecoverable_error_handler(), kDirectoryName, file_path_);
    dir().reset(test_directory_);
    ASSERT_TRUE(dir().get());
    ASSERT_EQ(OPENED,
              dir()->Open(kDirectoryName,
                          directory_change_delegate(),
                          NullTransactionObserver()));
    ASSERT_TRUE(dir()->good());
  }

  void SaveAndReloadDir() {
    dir()->SaveChanges();
    CreateDirectory();
  }

  void StartFailingSaveChanges() {
    test_directory_->StartFailingSaveChanges();
  }

  TestDirectory *test_directory_;  // mirrors scoped_ptr<Directory> dir_
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
};

sync_pb::DataTypeProgressMarker BuildProgress(ModelType type) {
  sync_pb::DataTypeProgressMarker progress;
  progress.set_token("token");
  progress.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  return progress;
}

sync_pb::DataTypeContext BuildContext(ModelType type) {
  sync_pb::DataTypeContext context;
  context.set_context("context");
  context.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  return context;
}

TEST_F(OnDiskSyncableDirectoryTest, TestPurgeEntriesWithTypeIn) {
  sync_pb::EntitySpecifics bookmark_specs;
  sync_pb::EntitySpecifics autofill_specs;
  sync_pb::EntitySpecifics preference_specs;
  AddDefaultFieldValue(BOOKMARKS, &bookmark_specs);
  AddDefaultFieldValue(PREFERENCES, &preference_specs);
  AddDefaultFieldValue(AUTOFILL, &autofill_specs);

  ModelTypeSet types_to_purge(PREFERENCES, AUTOFILL);

  dir()->SetDownloadProgress(BOOKMARKS, BuildProgress(BOOKMARKS));
  dir()->SetDownloadProgress(PREFERENCES, BuildProgress(PREFERENCES));
  dir()->SetDownloadProgress(AUTOFILL, BuildProgress(AUTOFILL));

  TestIdFactory id_factory;
  // Create some items for each type.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->SetDataTypeContext(&trans, BOOKMARKS, BuildContext(BOOKMARKS));
    dir()->SetDataTypeContext(&trans, PREFERENCES, BuildContext(PREFERENCES));
    dir()->SetDataTypeContext(&trans, AUTOFILL, BuildContext(AUTOFILL));

    // Make it look like these types have completed initial sync.
    CreateTypeRoot(&trans, dir().get(), BOOKMARKS);
    CreateTypeRoot(&trans, dir().get(), PREFERENCES);
    CreateTypeRoot(&trans, dir().get(), AUTOFILL);

    // Add more nodes for this type.  Technically, they should be placed under
    // the proper type root nodes but the assertions in this test won't notice
    // if their parent isn't quite right.
    MutableEntry item1(&trans, CREATE, BOOKMARKS, trans.root_id(), "Item");
    ASSERT_TRUE(item1.good());
    item1.PutServerSpecifics(bookmark_specs);
    item1.PutIsUnsynced(true);

    MutableEntry item2(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item2.good());
    item2.PutServerSpecifics(bookmark_specs);
    item2.PutIsUnappliedUpdate(true);

    MutableEntry item3(&trans, CREATE, PREFERENCES,
                       trans.root_id(), "Item");
    ASSERT_TRUE(item3.good());
    item3.PutSpecifics(preference_specs);
    item3.PutServerSpecifics(preference_specs);
    item3.PutIsUnsynced(true);

    MutableEntry item4(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item4.good());
    item4.PutServerSpecifics(preference_specs);
    item4.PutIsUnappliedUpdate(true);

    MutableEntry item5(&trans, CREATE, AUTOFILL,
                       trans.root_id(), "Item");
    ASSERT_TRUE(item5.good());
    item5.PutSpecifics(autofill_specs);
    item5.PutServerSpecifics(autofill_specs);
    item5.PutIsUnsynced(true);

    MutableEntry item6(&trans, CREATE_NEW_UPDATE_ITEM,
      id_factory.NewServerId());
    ASSERT_TRUE(item6.good());
    item6.PutServerSpecifics(autofill_specs);
    item6.PutIsUnappliedUpdate(true);
  }

  dir()->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    MetahandleSet all_set;
    GetAllMetaHandles(&trans, &all_set);
    ASSERT_EQ(10U, all_set.size());
  }

  dir()->PurgeEntriesWithTypeIn(types_to_purge, ModelTypeSet(), ModelTypeSet());

  // We first query the in-memory data, and then reload the directory (without
  // saving) to verify that disk does not still have the data.
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, true);
  SaveAndReloadDir();
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, false);
}

TEST_F(OnDiskSyncableDirectoryTest, TestShareInfo) {
  dir()->set_store_birthday("Jan 31st");
  const char* const bag_of_chips_array = "\0bag of chips";
  const std::string bag_of_chips_string =
      std::string(bag_of_chips_array, sizeof(bag_of_chips_array));
  dir()->set_bag_of_chips(bag_of_chips_string);
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    EXPECT_EQ("Jan 31st", dir()->store_birthday());
    EXPECT_EQ(bag_of_chips_string, dir()->bag_of_chips());
  }
  dir()->set_store_birthday("April 10th");
  const char* const bag_of_chips2_array = "\0bag of chips2";
  const std::string bag_of_chips2_string =
      std::string(bag_of_chips2_array, sizeof(bag_of_chips2_array));
  dir()->set_bag_of_chips(bag_of_chips2_string);
  dir()->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    EXPECT_EQ("April 10th", dir()->store_birthday());
    EXPECT_EQ(bag_of_chips2_string, dir()->bag_of_chips());
  }
  const char* const bag_of_chips3_array = "\0bag of chips3";
  const std::string bag_of_chips3_string =
      std::string(bag_of_chips3_array, sizeof(bag_of_chips3_array));
  dir()->set_bag_of_chips(bag_of_chips3_string);
  // Restore the directory from disk.  Make sure that nothing's changed.
  SaveAndReloadDir();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    EXPECT_EQ("April 10th", dir()->store_birthday());
    EXPECT_EQ(bag_of_chips3_string, dir()->bag_of_chips());
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
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry create(
        &trans, CREATE, BOOKMARKS, trans.root_id(), create_name);
    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, update_id);
    create.PutIsUnsynced(true);
    update.PutIsUnappliedUpdate(true);
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_favicon("PNG");
    specifics.mutable_bookmark()->set_url("http://nowhere");
    create.PutSpecifics(specifics);
    update.PutSpecifics(specifics);
    create_pre_save = create.GetKernelCopy();
    update_pre_save = update.GetKernelCopy();
    create_id = create.GetId();
  }

  dir()->SaveChanges();
  dir().reset(
      new Directory(new OnDiskDirectoryBackingStore(kDirectoryName, file_path_),
                    unrecoverable_error_handler(),
                    NULL,
                    NULL,
                    NULL));

  ASSERT_TRUE(dir().get());
  ASSERT_EQ(OPENED,
            dir()->Open(kDirectoryName,
                        directory_change_delegate(),
                        NullTransactionObserver()));
  ASSERT_TRUE(dir()->good());

  {
    ReadTransaction trans(FROM_HERE, dir().get());
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
  for ( ; i < UNIQUE_POSITION_FIELDS_END; ++i) {
    EXPECT_TRUE(create_pre_save.ref((UniquePositionField)i).Equals(
        create_post_save.ref((UniquePositionField)i)))
        << "Position field #" << i << " changed during save/load";
    EXPECT_TRUE(update_pre_save.ref((UniquePositionField)i).Equals(
        update_post_save.ref((UniquePositionField)i)))
        << "Position field #" << i << " changed during save/load";
  }
}

TEST_F(OnDiskSyncableDirectoryTest, TestSaveChangesFailure) {
  int64 handle1 = 0;
  // Set up an item using a regular, saveable directory.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.GetMetahandle();
    e1.PutBaseVersion(1);
    e1.PutIsDir(true);
    e1.PutId(TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir()->SaveChanges());

  // Make sure the item is no longer dirty after saving,
  // and make a modification.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.GetNonUniqueName(), "aguilera");
    aguilera.PutNonUniqueName("overwritten");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir()->SaveChanges());

  // Now do some operations when SaveChanges() will fail.
  StartFailingSaveChanges();
  ASSERT_TRUE(dir()->good());

  int64 handle2 = 0;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.GetNonUniqueName(), "overwritten");
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_FALSE(IsInDirtyMetahandles(handle1));
    aguilera.PutNonUniqueName("christina");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));

    // New item.
    MutableEntry kids_on_block(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "kids");
    ASSERT_TRUE(kids_on_block.good());
    handle2 = kids_on_block.GetMetahandle();
    kids_on_block.PutBaseVersion(1);
    kids_on_block.PutIsDir(true);
    kids_on_block.PutId(TestIdFactory::FromNumber(102));
    EXPECT_TRUE(kids_on_block.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle2));
  }

  // We are using an unsaveable directory, so this can't succeed.  However,
  // the HandleSaveChangesFailure code path should have been triggered.
  ASSERT_FALSE(dir()->SaveChanges());

  // Make sure things were rolled back and the world is as it was before call.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
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
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.GetMetahandle();
    e1.PutBaseVersion(1);
    e1.PutIsDir(true);
    e1.PutId(TestIdFactory::FromNumber(101));
    sync_pb::EntitySpecifics bookmark_specs;
    AddDefaultFieldValue(BOOKMARKS, &bookmark_specs);
    e1.PutSpecifics(bookmark_specs);
    e1.PutServerSpecifics(bookmark_specs);
    e1.PutId(TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir()->SaveChanges());

  // Now do some operations while SaveChanges() is set to fail.
  StartFailingSaveChanges();
  ASSERT_TRUE(dir()->good());

  ModelTypeSet set(BOOKMARKS);
  dir()->PurgeEntriesWithTypeIn(set, ModelTypeSet(), ModelTypeSet());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
  ASSERT_FALSE(dir()->SaveChanges());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
}

class SyncableDirectoryManagement : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
  }
 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  NullDirectoryChangeDelegate delegate_;
};

TEST_F(SyncableDirectoryManagement, TestFileRelease) {
  base::FilePath path =
      temp_dir_.path().Append(Directory::kSyncDatabaseFilename);

  Directory dir(new OnDiskDirectoryBackingStore("ScopeTest", path),
                &handler_,
                NULL,
                NULL,
                NULL);
  DirOpenResult result =
      dir.Open("ScopeTest", &delegate_, NullTransactionObserver());
  ASSERT_EQ(result, OPENED);
  dir.Close();

  // Closing the directory should have released the backing database file.
  ASSERT_TRUE(base::DeleteFile(path, true));
}

class StressTransactionsDelegate : public base::PlatformThread::Delegate {
 public:
  StressTransactionsDelegate(Directory* dir, int thread_number)
      : dir_(dir), thread_number_(thread_number) {}

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
        e.PutIsUnsynced(true);
        if (e.PutId(TestIdFactory::FromNumber(rand())) &&
            e.GetId().ServerKnows() && !e.GetId().IsRoot()) {
           e.PutBaseVersion(1);
        }
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(StressTransactionsDelegate);
};

TEST(SyncableDirectory, StressTransactions) {
  base::MessageLoop message_loop;
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
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, PREFERENCES,
                    wtrans.root_id(), test_name_);
    CHECK(me.good());
    me.PutId(id);
    if (id.ServerKnows()) {
      me.PutBaseVersion(kBaseVersion);
    }
    me.PutIsUnsynced(true);
    me.PutIsDel(deleted);
    me.PutIsDir(false);
    return me.PutUniqueClientTag(test_tag_);
  }

  // Verify an entry exists with the default tag.
  void VerifyTag(Id id, bool deleted) {
    // Should still be present and valid in the client tag index.
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    CHECK(me.good());
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetUniqueClientTag(), test_tag_);
    EXPECT_EQ(me.GetIsDel(), deleted);

    // We only sync deleted items that the server knew about.
    if (me.GetId().ServerKnows() || !me.GetIsDel()) {
      EXPECT_EQ(me.GetIsUnsynced(), true);
    }
  }

 protected:
  TestIdFactory factory_;
};

TEST_F(SyncableClientTagTest, TestClientTagClear) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_TRUE(me.good());
    me.PutUniqueClientTag(std::string());
  }
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry by_tag(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_FALSE(by_tag.good());

    Entry by_id(&trans, GET_BY_ID, server_id);
    EXPECT_TRUE(by_id.good());
    EXPECT_TRUE(by_id.GetUniqueClientTag().empty());
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

}  // namespace syncable
}  // namespace syncer
