// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/directory_unittest.h"

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "sync/internal_api/public/base/attachment_id_proto.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "sync/test/test_directory_backing_store.h"

using base::ExpectDictBooleanValue;
using base::ExpectDictStringValue;

namespace syncer {

namespace syncable {

namespace {

bool IsLegalNewParent(const Entry& a, const Entry& b) {
  return IsLegalNewParent(a.trans(), a.GetId(), b.GetId());
}

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

const char SyncableDirectoryTest::kDirectoryName[] = "Foo";

SyncableDirectoryTest::SyncableDirectoryTest() {
}

SyncableDirectoryTest::~SyncableDirectoryTest() {
}

void SyncableDirectoryTest::SetUp() {
  ASSERT_TRUE(connection_.OpenInMemory());
  ASSERT_EQ(OPENED, ReopenDirectory());
}

void SyncableDirectoryTest::TearDown() {
  if (dir_)
    dir_->SaveChanges();
  dir_.reset();
}

DirOpenResult SyncableDirectoryTest::ReopenDirectory() {
  // Use a TestDirectoryBackingStore and sql::Connection so we can have test
  // data persist across Directory object lifetimes while getting the
  // performance benefits of not writing to disk.
  dir_.reset(
      new Directory(new TestDirectoryBackingStore(kDirectoryName, &connection_),
                    &handler_,
                    NULL,
                    NULL,
                    NULL));

  DirOpenResult open_result =
      dir_->Open(kDirectoryName, &delegate_, NullTransactionObserver());

  if (open_result != OPENED) {
    dir_.reset();
  }

  return open_result;
}

// Creates an empty entry and sets the ID field to a default one.
void SyncableDirectoryTest::CreateEntry(const ModelType& model_type,
                                        const std::string& entryname) {
  CreateEntry(model_type, entryname, TestIdFactory::FromNumber(-99));
}

// Creates an empty entry and sets the ID field to id.
void SyncableDirectoryTest::CreateEntry(const ModelType& model_type,
                                        const std::string& entryname,
                                        const int id) {
  CreateEntry(model_type, entryname, TestIdFactory::FromNumber(id));
}

void SyncableDirectoryTest::CreateEntry(const ModelType& model_type,
                                        const std::string& entryname,
                                        const Id& id) {
  CreateEntryWithAttachmentMetadata(
      model_type, entryname, id, sync_pb::AttachmentMetadata());
}

void SyncableDirectoryTest::CreateEntryWithAttachmentMetadata(
    const ModelType& model_type,
    const std::string& entryname,
    const Id& id,
    const sync_pb::AttachmentMetadata& attachment_metadata) {
  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry me(&wtrans, CREATE, model_type, wtrans.root_id(), entryname);
  ASSERT_TRUE(me.good());
  me.PutId(id);
  me.PutAttachmentMetadata(attachment_metadata);
  me.PutIsUnsynced(true);
}

void SyncableDirectoryTest::DeleteEntry(const Id& id) {
  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry entry(&trans, GET_BY_ID, id);
  ASSERT_TRUE(entry.good());
  entry.PutIsDel(true);
}

DirOpenResult SyncableDirectoryTest::SimulateSaveAndReloadDir() {
  if (!dir_->SaveChanges())
    return FAILED_IN_UNITTEST;

  return ReopenDirectory();
}

DirOpenResult SyncableDirectoryTest::SimulateCrashAndReloadDir() {
  return ReopenDirectory();
}

void SyncableDirectoryTest::GetAllMetaHandles(BaseTransaction* trans,
                                              MetahandleSet* result) {
  dir_->GetAllMetaHandles(trans, result);
}

void SyncableDirectoryTest::CheckPurgeEntriesWithTypeInSucceeded(
    ModelTypeSet types_to_purge,
    bool before_reload) {
  SCOPED_TRACE(testing::Message("Before reload: ") << before_reload);
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    MetahandleSet all_set;
    dir_->GetAllMetaHandles(&trans, &all_set);
    EXPECT_EQ(4U, all_set.size());
    if (before_reload)
      EXPECT_EQ(6U, dir_->kernel_->metahandles_to_purge.size());
    for (MetahandleSet::iterator iter = all_set.begin(); iter != all_set.end();
         ++iter) {
      Entry e(&trans, GET_BY_HANDLE, *iter);
      const ModelType local_type = e.GetModelType();
      const ModelType server_type = e.GetServerModelType();

      // Note the dance around incrementing |it|, since we sometimes erase().
      if ((IsRealDataType(local_type) && types_to_purge.Has(local_type)) ||
          (IsRealDataType(server_type) && types_to_purge.Has(server_type))) {
        FAIL() << "Illegal type should have been deleted.";
      }
    }
  }

  for (ModelTypeSet::Iterator it = types_to_purge.First(); it.Good();
       it.Inc()) {
    EXPECT_FALSE(dir_->InitialSyncEndedForType(it.Get()));
    sync_pb::DataTypeProgressMarker progress;
    dir_->GetDownloadProgress(it.Get(), &progress);
    EXPECT_EQ("", progress.token());

    ReadTransaction trans(FROM_HERE, dir_.get());
    sync_pb::DataTypeContext context;
    dir_->GetDataTypeContext(&trans, it.Get(), &context);
    EXPECT_TRUE(context.SerializeAsString().empty());
  }
  EXPECT_FALSE(types_to_purge.Has(BOOKMARKS));
  EXPECT_TRUE(dir_->InitialSyncEndedForType(BOOKMARKS));
}

bool SyncableDirectoryTest::IsInDirtyMetahandles(int64 metahandle) {
  return 1 == dir_->kernel_->dirty_metahandles.count(metahandle);
}

bool SyncableDirectoryTest::IsInMetahandlesToPurge(int64 metahandle) {
  return 1 == dir_->kernel_->metahandles_to_purge.count(metahandle);
}

scoped_ptr<Directory>& SyncableDirectoryTest::dir() {
  return dir_;
}

DirectoryChangeDelegate* SyncableDirectoryTest::directory_change_delegate() {
  return &delegate_;
}

Encryptor* SyncableDirectoryTest::encryptor() {
  return &encryptor_;
}

UnrecoverableErrorHandler*
SyncableDirectoryTest::unrecoverable_error_handler() {
  return &handler_;
}

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
    ASSERT_TRUE(name == e.GetNonUniqueName());
  ASSERT_TRUE(base_version == e.GetBaseVersion());
  ASSERT_TRUE(server_version == e.GetServerVersion());
  ASSERT_TRUE(is_del == e.GetIsDel());
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsMetahandlesToPurge) {
  const int metas_to_create = 50;
  MetahandleSet expected_purges;
  MetahandleSet all_handles;
  {
    dir()->SetDownloadProgress(BOOKMARKS, BuildProgress(BOOKMARKS));
    dir()->SetDownloadProgress(PREFERENCES, BuildProgress(PREFERENCES));
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (int i = 0; i < metas_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      e.PutIsUnsynced(true);
      sync_pb::EntitySpecifics specs;
      if (i % 2 == 0) {
        AddDefaultFieldValue(BOOKMARKS, &specs);
        expected_purges.insert(e.GetMetahandle());
        all_handles.insert(e.GetMetahandle());
      } else {
        AddDefaultFieldValue(PREFERENCES, &specs);
        all_handles.insert(e.GetMetahandle());
      }
      e.PutSpecifics(specs);
      e.PutServerSpecifics(specs);
    }
  }

  ModelTypeSet to_purge(BOOKMARKS);
  dir()->PurgeEntriesWithTypeIn(to_purge, ModelTypeSet(), ModelTypeSet());

  Directory::SaveChangesSnapshot snapshot1;
  base::AutoLock scoped_lock(dir()->kernel_->save_changes_mutex);
  dir()->TakeSnapshotForSaveChanges(&snapshot1);
  EXPECT_TRUE(expected_purges == snapshot1.metahandles_to_purge);

  to_purge.Clear();
  to_purge.Put(PREFERENCES);
  dir()->PurgeEntriesWithTypeIn(to_purge, ModelTypeSet(), ModelTypeSet());

  dir()->HandleSaveChangesFailure(snapshot1);

  Directory::SaveChangesSnapshot snapshot2;
  dir()->TakeSnapshotForSaveChanges(&snapshot2);
  EXPECT_TRUE(all_handles == snapshot2.metahandles_to_purge);
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsAllDirtyHandlesTest) {
  const int metahandles_to_create = 100;
  std::vector<int64> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.GetMetahandle());
      e.PutIsUnsynced(true);
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel_->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    ASSERT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
         i != snapshot.dirty_metas.end();
         ++i) {
      ASSERT_TRUE((*i)->is_dirty());
    }
    dir()->VacuumAfterSaveChanges(snapshot);
  }
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    std::vector<int64> new_dirty_metahandles;
    for (std::vector<int64>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end();
         ++i) {
      // Change existing entries to directories to dirty them.
      MutableEntry e1(&trans, GET_BY_HANDLE, *i);
      e1.PutIsDir(true);
      e1.PutIsUnsynced(true);
      // Add new entries
      MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), "bar");
      e2.PutIsUnsynced(true);
      new_dirty_metahandles.push_back(e2.GetMetahandle());
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
                                      new_dirty_metahandles.begin(),
                                      new_dirty_metahandles.end());
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel_->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
         i != snapshot.dirty_metas.end();
         ++i) {
      EXPECT_TRUE((*i)->is_dirty());
    }
    dir()->VacuumAfterSaveChanges(snapshot);
  }
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsOnlyDirtyHandlesTest) {
  const int metahandles_to_create = 100;

  // half of 2 * metahandles_to_create
  const unsigned int number_changed = 100u;
  std::vector<int64> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.GetMetahandle());
      e.PutIsUnsynced(true);
    }
  }
  dir()->SaveChanges();
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    std::vector<int64> new_dirty_metahandles;
    for (std::vector<int64>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end();
         ++i) {
      // Change existing entries to directories to dirty them.
      MutableEntry e1(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e1.good());
      e1.PutIsDir(true);
      e1.PutIsUnsynced(true);
      // Add new entries
      MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), "bar");
      e2.PutIsUnsynced(true);
      new_dirty_metahandles.push_back(e2.GetMetahandle());
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
                                      new_dirty_metahandles.begin(),
                                      new_dirty_metahandles.end());
  }
  dir()->SaveChanges();
  // Don't make any changes whatsoever and ensure nothing comes back.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    for (std::vector<int64>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end();
         ++i) {
      MutableEntry e(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e.good());
      // We aren't doing anything to dirty these entries.
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel_->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there are no dirty_metahandles.
    EXPECT_EQ(0u, snapshot.dirty_metas.size());
    dir()->VacuumAfterSaveChanges(snapshot);
  }
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    bool should_change = false;
    for (std::vector<int64>::const_iterator i =
             expected_dirty_metahandles.begin();
         i != expected_dirty_metahandles.end();
         ++i) {
      // Maybe change entries by flipping IS_DIR.
      MutableEntry e(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e.good());
      should_change = !should_change;
      if (should_change) {
        bool not_dir = !e.GetIsDir();
        e.PutIsDir(not_dir);
        e.PutIsUnsynced(true);
      }
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir()->kernel_->save_changes_mutex);
    dir()->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each changed metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(number_changed, snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
         i != snapshot.dirty_metas.end();
         ++i) {
      EXPECT_TRUE((*i)->is_dirty());
    }
    dir()->VacuumAfterSaveChanges(snapshot);
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
    CreateEntry(BOOKMARKS, "item1", id1);
    CreateEntry(BOOKMARKS, "item2", id2);
    {
      WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
      MutableEntry item1(&trans, GET_BY_ID, id1);
      ASSERT_TRUE(item1.good());
      handle1 = item1.GetMetahandle();
      item1.PutSpecifics(bookmark_specifics);
      item1.PutServerSpecifics(bookmark_specifics);
      MutableEntry item2(&trans, GET_BY_ID, id2);
      ASSERT_TRUE(item2.good());
      handle2 = item2.GetMetahandle();
      item2.PutSpecifics(bookmark_specifics);
      item2.PutServerSpecifics(bookmark_specifics);
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  {  // Test adding and saving delete journals.
    DeleteJournal* delete_journal = dir()->delete_journal();
    {
      WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
      EntryKernelSet journal_entries;
      delete_journal->GetDeleteJournals(&trans, BOOKMARKS, &journal_entries);
      ASSERT_EQ(0u, journal_entries.size());

      // Set SERVER_IS_DEL of the entries to true and they should be added to
      // delete journals.
      MutableEntry item1(&trans, GET_BY_ID, id1);
      ASSERT_TRUE(item1.good());
      item1.PutServerIsDel(true);
      MutableEntry item2(&trans, GET_BY_ID, id2);
      ASSERT_TRUE(item2.good());
      item2.PutServerIsDel(true);
      EntryKernel tmp;
      tmp.put(ID, id1);
      EXPECT_TRUE(delete_journal->delete_journals_.count(&tmp));
      tmp.put(ID, id2);
      EXPECT_TRUE(delete_journal->delete_journals_.count(&tmp));
    }

    // Save delete journals in database and verify memory clearing.
    ASSERT_TRUE(dir()->SaveChanges());
    {
      ReadTransaction trans(FROM_HERE, dir().get());
      EXPECT_EQ(0u, delete_journal->GetDeleteJournalSize(&trans));
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  {
    {
      // Test reading delete journals from database.
      WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
      DeleteJournal* delete_journal = dir()->delete_journal();
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
      WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
      DeleteJournal* delete_journal = dir()->delete_journal();
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
      item1.PutServerIsDel(false);
      EXPECT_TRUE(delete_journal->delete_journals_.empty());
      EXPECT_EQ(1u, delete_journal->delete_journals_to_purge_.size());
      EXPECT_TRUE(delete_journal->delete_journals_to_purge_.count(handle1));
    }
    ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());
  }

  {
    // Verify undeleted entry is gone from database.
    ReadTransaction trans(FROM_HERE, dir().get());
    DeleteJournal* delete_journal = dir()->delete_journal();
    ASSERT_EQ(0u, delete_journal->GetDeleteJournalSize(&trans));
  }
}

TEST_F(SyncableDirectoryTest, TestBasicLookupNonExistantID) {
  ReadTransaction rtrans(FROM_HERE, dir().get());
  Entry e(&rtrans, GET_BY_ID, TestIdFactory::FromNumber(-99));
  ASSERT_FALSE(e.good());
}

TEST_F(SyncableDirectoryTest, TestBasicLookupValidID) {
  CreateEntry(BOOKMARKS, "rtc");
  ReadTransaction rtrans(FROM_HERE, dir().get());
  Entry e(&rtrans, GET_BY_ID, TestIdFactory::FromNumber(-99));
  ASSERT_TRUE(e.good());
}

TEST_F(SyncableDirectoryTest, TestDelete) {
  std::string name = "peanut butter jelly time";
  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e1.good());
  e1.PutIsDel(true);
  MutableEntry e2(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e2.good());
  e2.PutIsDel(true);
  MutableEntry e3(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
  ASSERT_TRUE(e3.good());
  e3.PutIsDel(true);

  e1.PutIsDel(false);
  e2.PutIsDel(false);
  e3.PutIsDel(false);

  e1.PutIsDel(true);
  e2.PutIsDel(true);
  e3.PutIsDel(true);
}

TEST_F(SyncableDirectoryTest, TestGetUnsynced) {
  Directory::Metahandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.GetMetahandle();
    e1.PutBaseVersion(1);
    e1.PutIsDir(true);
    e1.PutId(TestIdFactory::FromNumber(101));

    MutableEntry e2(&trans, CREATE, BOOKMARKS, e1.GetId(), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.GetMetahandle();
    e2.PutBaseVersion(1);
    e2.PutId(TestIdFactory::FromNumber(102));
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.PutIsUnsynced(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.PutIsUnsynced(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(2 == handles.size());
    if (handle1 == handles[0]) {
      ASSERT_TRUE(handle2 == handles[1]);
    } else {
      ASSERT_TRUE(handle2 == handles[0]);
      ASSERT_TRUE(handle1 == handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    ASSERT_TRUE(e5.GetIsUnsynced());
    ASSERT_TRUE(e5.PutIsUnsynced(false));
    ASSERT_FALSE(e5.GetIsUnsynced());
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, TestGetUnappliedUpdates) {
  std::vector<int64> handles;
  int64 handle1, handle2;
  const FullModelTypeSet all_types = FullModelTypeSet::All();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, BOOKMARKS, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.GetMetahandle();
    e1.PutIsUnappliedUpdate(false);
    e1.PutBaseVersion(1);
    e1.PutId(TestIdFactory::FromNumber(101));
    e1.PutIsDir(true);

    MutableEntry e2(&trans, CREATE, BOOKMARKS, e1.GetId(), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.GetMetahandle();
    e2.PutIsUnappliedUpdate(false);
    e2.PutBaseVersion(1);
    e2.PutId(TestIdFactory::FromNumber(102));
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.PutIsUnappliedUpdate(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.PutIsUnappliedUpdate(true);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(2 == handles.size());
    if (handle1 == handles[0]) {
      ASSERT_TRUE(handle2 == handles[1]);
    } else {
      ASSERT_TRUE(handle2 == handles[0]);
      ASSERT_TRUE(handle1 == handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    e5.PutIsUnappliedUpdate(false);
  }
  dir()->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    dir()->GetUnappliedUpdateMetaHandles(&trans, all_types, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, DeleteBug_531383) {
  // Try to evoke a check failure...
  TestIdFactory id_factory;
  int64 grandchild_handle;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, id_factory.root(), "Bob");
    ASSERT_TRUE(parent.good());
    parent.PutIsDir(true);
    parent.PutId(id_factory.NewServerId());
    parent.PutBaseVersion(1);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "Bob");
    ASSERT_TRUE(child.good());
    child.PutIsDir(true);
    child.PutId(id_factory.NewServerId());
    child.PutBaseVersion(1);
    MutableEntry grandchild(&wtrans, CREATE, BOOKMARKS, child.GetId(), "Bob");
    ASSERT_TRUE(grandchild.good());
    grandchild.PutId(id_factory.NewServerId());
    grandchild.PutBaseVersion(1);
    grandchild.PutIsDel(true);
    MutableEntry twin(&wtrans, CREATE, BOOKMARKS, child.GetId(), "Bob");
    ASSERT_TRUE(twin.good());
    twin.PutIsDel(true);
    grandchild.PutIsDel(false);

    grandchild_handle = grandchild.GetMetahandle();
  }
  dir()->SaveChanges();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry grandchild(&wtrans, GET_BY_HANDLE, grandchild_handle);
    grandchild.PutIsDel(true);  // Used to CHECK fail here.
  }
}

TEST_F(SyncableDirectoryTest, TestIsLegalNewParent) {
  TestIdFactory id_factory;
  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
  Entry root(&wtrans, GET_BY_ID, id_factory.root());
  ASSERT_TRUE(root.good());
  MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root.GetId(), "Bob");
  ASSERT_TRUE(parent.good());
  parent.PutIsDir(true);
  parent.PutId(id_factory.NewServerId());
  parent.PutBaseVersion(1);
  MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "Bob");
  ASSERT_TRUE(child.good());
  child.PutIsDir(true);
  child.PutId(id_factory.NewServerId());
  child.PutBaseVersion(1);
  MutableEntry grandchild(&wtrans, CREATE, BOOKMARKS, child.GetId(), "Bob");
  ASSERT_TRUE(grandchild.good());
  grandchild.PutId(id_factory.NewServerId());
  grandchild.PutBaseVersion(1);

  MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, root.GetId(), "Pete");
  ASSERT_TRUE(parent2.good());
  parent2.PutIsDir(true);
  parent2.PutId(id_factory.NewServerId());
  parent2.PutBaseVersion(1);
  MutableEntry child2(&wtrans, CREATE, BOOKMARKS, parent2.GetId(), "Pete");
  ASSERT_TRUE(child2.good());
  child2.PutIsDir(true);
  child2.PutId(id_factory.NewServerId());
  child2.PutBaseVersion(1);
  MutableEntry grandchild2(&wtrans, CREATE, BOOKMARKS, child2.GetId(), "Pete");
  ASSERT_TRUE(grandchild2.good());
  grandchild2.PutId(id_factory.NewServerId());
  grandchild2.PutBaseVersion(1);
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
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "folder");
    ASSERT_TRUE(folder.good());
    folder.PutIsDir(true);
    EXPECT_TRUE(folder.PutIsUnsynced(true));
    folder_id = folder.GetId();

    MutableEntry entry(&trans, CREATE, BOOKMARKS, folder.GetId(), entry_name);
    ASSERT_TRUE(entry.good());
    entry_handle = entry.GetMetahandle();
    entry.PutIsUnsynced(true);
    entry_id = entry.GetId();
  }

  // Make sure we can find the entry in the folder.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), entry_name));
    EXPECT_EQ(1, CountEntriesWithName(&trans, folder_id, entry_name));

    Entry entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(entry_handle, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetNonUniqueName() == entry_name);
    EXPECT_TRUE(entry.GetParentId() == folder_id);
  }
}

TEST_F(SyncableDirectoryTest, TestParentIdIndexUpdate) {
  std::string child_name = "child";

  WriteTransaction wt(FROM_HERE, UNITTEST, dir().get());
  MutableEntry parent_folder(&wt, CREATE, BOOKMARKS, wt.root_id(), "folder1");
  parent_folder.PutIsUnsynced(true);
  parent_folder.PutIsDir(true);

  MutableEntry parent_folder2(&wt, CREATE, BOOKMARKS, wt.root_id(), "folder2");
  parent_folder2.PutIsUnsynced(true);
  parent_folder2.PutIsDir(true);

  MutableEntry child(&wt, CREATE, BOOKMARKS, parent_folder.GetId(), child_name);
  child.PutIsDir(true);
  child.PutIsUnsynced(true);

  ASSERT_TRUE(child.good());

  EXPECT_EQ(0, CountEntriesWithName(&wt, wt.root_id(), child_name));
  EXPECT_EQ(parent_folder.GetId(), child.GetParentId());
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder.GetId(), child_name));
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder2.GetId(), child_name));
  child.PutParentId(parent_folder2.GetId());
  EXPECT_EQ(parent_folder2.GetId(), child.GetParentId());
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder.GetId(), child_name));
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder2.GetId(), child_name));
}

TEST_F(SyncableDirectoryTest, TestNoReindexDeletedItems) {
  std::string folder_name = "folder";
  std::string new_name = "new_name";

  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), folder_name);
  ASSERT_TRUE(folder.good());
  folder.PutIsDir(true);
  folder.PutIsDel(true);

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));

  MutableEntry deleted(&trans, GET_BY_ID, folder.GetId());
  ASSERT_TRUE(deleted.good());
  deleted.PutParentId(trans.root_id());
  deleted.PutNonUniqueName(new_name);

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));
  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), new_name));
}

TEST_F(SyncableDirectoryTest, TestCaseChangeRename) {
  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
  MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "CaseChange");
  ASSERT_TRUE(folder.good());
  folder.PutParentId(trans.root_id());
  folder.PutNonUniqueName("CASECHANGE");
  folder.PutIsDel(true);
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

    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "Folder");
    ASSERT_TRUE(folder.good());
    folder.PutId(id_factory.NewServerId());
    folder.PutSpecifics(specifics);
    folder.PutBaseVersion(1);
    folder.PutIsDir(true);
    folder.PutIsDel(false);
    ASSERT_EQ(datatype, folder.GetModelType());

    MutableEntry item(&trans, CREATE, BOOKMARKS, trans.root_id(), "Item");
    ASSERT_TRUE(item.good());
    item.PutId(id_factory.NewServerId());
    item.PutSpecifics(specifics);
    item.PutBaseVersion(1);
    item.PutIsDir(false);
    item.PutIsDel(false);
    ASSERT_EQ(datatype, item.GetModelType());

    // It's critical that deletion records retain their datatype, so that
    // they can be dispatched to the appropriate change processor.
    MutableEntry deleted_item(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "Deleted Item");
    ASSERT_TRUE(item.good());
    deleted_item.PutId(id_factory.NewServerId());
    deleted_item.PutSpecifics(specifics);
    deleted_item.PutBaseVersion(1);
    deleted_item.PutIsDir(false);
    deleted_item.PutIsDel(true);
    ASSERT_EQ(datatype, deleted_item.GetModelType());

    MutableEntry server_folder(
        &trans, CREATE_NEW_UPDATE_ITEM, id_factory.NewServerId());
    ASSERT_TRUE(server_folder.good());
    server_folder.PutServerSpecifics(specifics);
    server_folder.PutBaseVersion(1);
    server_folder.PutServerIsDir(true);
    server_folder.PutServerIsDel(false);
    ASSERT_EQ(datatype, server_folder.GetServerModelType());

    MutableEntry server_item(
        &trans, CREATE_NEW_UPDATE_ITEM, id_factory.NewServerId());
    ASSERT_TRUE(server_item.good());
    server_item.PutServerSpecifics(specifics);
    server_item.PutBaseVersion(1);
    server_item.PutServerIsDir(false);
    server_item.PutServerIsDel(false);
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
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.GetId(), "child");
    child.PutIsUnsynced(true);

    orig_parent_id = parent.GetId();
    orig_child_id = child.GetId();
  }

  {
    // Simulate what happens after committing two items.  Their IDs will be
    // replaced with server IDs.  The child is renamed first, then the parent.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);
    MutableEntry child(&trans, GET_BY_ID, orig_child_id);

    ChangeEntryIDAndUpdateChildren(&trans, &child, id_factory.NewServerId());
    child.PutIsUnsynced(false);
    child.PutBaseVersion(1);
    child.PutServerVersion(1);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.PutIsUnsynced(false);
    parent.PutBaseVersion(1);
    parent.PutServerVersion(1);
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
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.GetId(), "child");
    child.PutIsUnsynced(true);

    orig_parent_id = parent.GetId();
    orig_child_id = child.GetId();
  }

  {
    // Delete the child.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry child(&trans, GET_BY_ID, orig_child_id);
    child.PutIsDel(true);
  }

  {
    // Simulate what happens after committing the parent.  Its ID will be
    // replaced with server a ID.
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, GET_BY_ID, orig_parent_id);

    ChangeEntryIDAndUpdateChildren(&trans, &parent, id_factory.NewServerId());
    parent.PutIsUnsynced(false);
    parent.PutBaseVersion(1);
    parent.PutServerVersion(1);
  }

  // Final check for validity.
  EXPECT_EQ(OPENED, SimulateSaveAndReloadDir());
}

// Ask the directory to generate a unique ID.  Close and re-open the database
// without saving, then ask for another unique ID.  Verify IDs are not reused.
// This scenario simulates a crash within the first few seconds of operation.
TEST_F(SyncableDirectoryTest, LocalIdReuseTest) {
  Id pre_crash_id = dir()->NextId();
  SimulateCrashAndReloadDir();
  Id post_crash_id = dir()->NextId();
  EXPECT_NE(pre_crash_id, post_crash_id);
}

// Ask the directory to generate a unique ID.  Save the directory.  Close and
// re-open the database without saving, then ask for another unique ID.  Verify
// IDs are not reused.  This scenario simulates a steady-state crash.
TEST_F(SyncableDirectoryTest, LocalIdReuseTestWithSave) {
  Id pre_crash_id = dir()->NextId();
  dir()->SaveChanges();
  SimulateCrashAndReloadDir();
  Id post_crash_id = dir()->NextId();
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
  dir()->SetInvariantCheckLevel(OFF);

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    // Create an uncommitted tombstone entry.
    MutableEntry server_knows(
        &trans, CREATE, BOOKMARKS, id_factory.root(), "server_knows");
    server_knows.PutId(server_knows_id);
    server_knows.PutIsUnsynced(true);
    server_knows.PutIsDel(true);
    server_knows.PutBaseVersion(5);
    server_knows.PutServerVersion(4);

    // Create a valid update entry.
    MutableEntry not_is_del(
        &trans, CREATE, BOOKMARKS, id_factory.root(), "not_is_del");
    not_is_del.PutId(not_is_del_id);
    not_is_del.PutIsDel(false);
    not_is_del.PutIsUnsynced(true);

    // Create a tombstone which should never be sent to the server because the
    // server never knew about the item's existence.
    //
    // New clients should never put entries into this state.  We work around
    // this by setting IS_DEL before setting IS_UNSYNCED, something which the
    // client should never do in practice.
    MutableEntry zombie(&trans, CREATE, BOOKMARKS, id_factory.root(), "zombie");
    zombie.PutId(zombie_id);
    zombie.PutIsDel(true);
    zombie.PutIsUnsynced(true);
  }

  ASSERT_EQ(OPENED, SimulateSaveAndReloadDir());

  {
    ReadTransaction trans(FROM_HERE, dir().get());

    // The directory loading routines should have cleaned things up, making it
    // safe to check invariants once again.
    dir()->FullyCheckTreeInvariants(&trans);

    Entry server_knows(&trans, GET_BY_ID, server_knows_id);
    EXPECT_TRUE(server_knows.good());

    Entry not_is_del(&trans, GET_BY_ID, not_is_del_id);
    EXPECT_TRUE(not_is_del.good());

    Entry zombie(&trans, GET_BY_ID, zombie_id);
    EXPECT_FALSE(zombie.good());
  }
}

TEST_F(SyncableDirectoryTest, PositionWithNullSurvivesSaveAndReload) {
  TestIdFactory id_factory;
  Id null_child_id;
  const char null_cstr[] = "\0null\0test";
  std::string null_str(null_cstr, arraysize(null_cstr) - 1);
  // Pad up to the minimum length with 0x7f characters, then add a string that
  // contains a few NULLs to the end.  This is slightly wrong, since the suffix
  // part of a UniquePosition shouldn't contain NULLs, but it's good enough for
  // this test.
  std::string suffix =
      std::string(UniquePosition::kSuffixLength - null_str.length(), '\x7f') +
      null_str;
  UniquePosition null_pos = UniquePosition::FromInt64(10, suffix);

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    MutableEntry child(&trans, CREATE, BOOKMARKS, parent.GetId(), "child");
    child.PutIsUnsynced(true);
    child.PutUniquePosition(null_pos);
    child.PutServerUniquePosition(null_pos);

    null_child_id = child.GetId();
  }

  EXPECT_EQ(OPENED, SimulateSaveAndReloadDir());

  {
    ReadTransaction trans(FROM_HERE, dir().get());

    Entry null_ordinal_child(&trans, GET_BY_ID, null_child_id);
    EXPECT_TRUE(null_pos.Equals(null_ordinal_child.GetUniquePosition()));
    EXPECT_TRUE(null_pos.Equals(null_ordinal_child.GetServerUniquePosition()));
  }
}

// Any item with BOOKMARKS in their local specifics should have a valid local
// unique position.  If there is an item in the loaded DB that does not match
// this criteria, we consider the whole DB to be corrupt.
TEST_F(SyncableDirectoryTest, BadPositionCountsAsCorruption) {
  TestIdFactory id_factory;

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    MutableEntry parent(&trans, CREATE, BOOKMARKS, id_factory.root(), "parent");
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);

    // The code is littered with DCHECKs that try to stop us from doing what
    // we're about to do.  Our work-around is to create a bookmark based on
    // a server update, then update its local specifics without updating its
    // local unique position.

    MutableEntry child(
        &trans, CREATE_NEW_UPDATE_ITEM, id_factory.MakeServer("child"));
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(BOOKMARKS, &specifics);
    child.PutIsUnappliedUpdate(true);
    child.PutSpecifics(specifics);

    EXPECT_TRUE(child.ShouldMaintainPosition());
    EXPECT_TRUE(!child.GetUniquePosition().IsValid());
  }

  EXPECT_EQ(FAILED_DATABASE_CORRUPT, SimulateSaveAndReloadDir());
}

TEST_F(SyncableDirectoryTest, General) {
  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  // Test simple read operations on an empty DB.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Directory::Metahandles child_handles;
    dir()->GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_TRUE(child_handles.empty());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    written_metahandle = me.GetMetahandle();
  }

  // Test GetChildHandles* after something is now in the DB.
  // Also check that GET_BY_ID works.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Directory::Metahandles child_handles;
    dir()->GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (Directory::Metahandles::iterator i = child_handles.begin();
         i != child_handles.end(); ++i) {
      EXPECT_EQ(*i, written_metahandle);
    }
  }

  // Test writing data to an entity. Also check that GET_BY_HANDLE works.
  static const char s[] = "Hello World.";
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    PutDataAsBookmarkFavicon(&trans, &e, s, sizeof(s));
  }

  // Test reading back the contents that we just wrote.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    ExpectDataFromBookmarkFaviconEquals(&trans, &e, s, sizeof(s));
  }

  // Verify it exists in the folder.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    EXPECT_EQ(1, CountEntriesWithName(&rtrans, rtrans.root_id(), name));
  }

  // Now delete it.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    e.PutIsDel(true);

    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), name));
  }

  dir()->SaveChanges();
}

TEST_F(SyncableDirectoryTest, ChildrenOps) {
  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_FALSE(dir()->HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_TRUE(root.GetFirstChildId().IsRoot());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    written_metahandle = me.GetMetahandle();
  }

  // Test children ops after something is now in the DB.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry child(&rtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(child.good());

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_TRUE(dir()->HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_EQ(e.GetId(), root.GetFirstChildId());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(me.good());
    me.PutIsDel(true);
  }

  // Test children ops after the children have been deleted.
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry root(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(root.good());
    EXPECT_FALSE(dir()->HasChildren(&rtrans, rtrans.root_id()));
    EXPECT_TRUE(root.GetFirstChildId().IsRoot());
  }

  dir()->SaveChanges();
}

TEST_F(SyncableDirectoryTest, ClientIndexRebuildsProperly) {
  int64 written_metahandle;
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string name = "cheesepuffs";
  std::string tag = "dietcoke";

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    me.PutUniqueClientTag(tag);
    written_metahandle = me.GetMetahandle();
  }
  dir()->SaveChanges();

  // Close and reopen, causing index regeneration.
  ReopenDirectory();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetBaseVersion(), 1);
    EXPECT_EQ(me.GetUniqueClientTag(), tag);
    EXPECT_EQ(me.GetMetahandle(), written_metahandle);
  }
}

TEST_F(SyncableDirectoryTest, ClientIndexRebuildsDeletedProperly) {
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string tag = "dietcoke";

  // Test creating a deleted, unsynced, server meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry me(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "deleted");
    ASSERT_TRUE(me.good());
    me.PutId(id);
    me.PutBaseVersion(1);
    me.PutUniqueClientTag(tag);
    me.PutIsDel(true);
    me.PutIsUnsynced(true);  // Or it might be purged.
  }
  dir()->SaveChanges();

  // Close and reopen, causing index regeneration.
  ReopenDirectory();
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    // Should still be present and valid in the client tag index.
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.GetId(), id);
    EXPECT_EQ(me.GetUniqueClientTag(), tag);
    EXPECT_TRUE(me.GetIsDel());
    EXPECT_TRUE(me.GetIsUnsynced());
  }
}

TEST_F(SyncableDirectoryTest, ToValue) {
  const Id id = TestIdFactory::FromNumber(99);
  {
    ReadTransaction rtrans(FROM_HERE, dir().get());
    Entry e(&rtrans, GET_BY_ID, id);
    EXPECT_FALSE(e.good());  // Hasn't been written yet.

    scoped_ptr<base::DictionaryValue> value(e.ToValue(NULL));
    ExpectDictBooleanValue(false, *value, "good");
    EXPECT_EQ(1u, value->size());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir().get());
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

  dir()->SaveChanges();
}

// Test that the bookmark tag generation algorithm remains unchanged.
TEST_F(SyncableDirectoryTest, BookmarkTagTest) {
  // This test needs its own InMemoryDirectoryBackingStore because it needs to
  // call request_consistent_cache_guid().
  InMemoryDirectoryBackingStore* store = new InMemoryDirectoryBackingStore("x");

  // The two inputs that form the bookmark tag are the directory's cache_guid
  // and its next_id value.  We don't need to take any action to ensure
  // consistent next_id values, but we do need to explicitly request that our
  // InMemoryDirectoryBackingStore always return the same cache_guid.
  store->request_consistent_cache_guid();

  Directory dir(store, unrecoverable_error_handler(), NULL, NULL, NULL);
  ASSERT_EQ(
      OPENED,
      dir.Open("x", directory_change_delegate(), NullTransactionObserver()));

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

// A thread that creates a bunch of directory entries.
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
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(rand() % 10));
      } else {
        std::string unique_name =
            base::StringPrintf("%d.%d", thread_number_, entry_count++);
        path_name.assign(unique_name.begin(), unique_name.end());
        WriteTransaction trans(FROM_HERE, UNITTEST, dir_);
        MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), path_name);
        CHECK(e.good());
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(rand() % 20));
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

// Stress test Directory by accessing it from several threads concurrently.
TEST_F(SyncableDirectoryTest, StressTransactions) {
  const int kThreadCount = 7;
  base::PlatformThreadHandle threads[kThreadCount];
  scoped_ptr<StressTransactionsDelegate> thread_delegates[kThreadCount];

  for (int i = 0; i < kThreadCount; ++i) {
    thread_delegates[i].reset(new StressTransactionsDelegate(dir().get(), i));
    ASSERT_TRUE(base::PlatformThread::Create(
        0, thread_delegates[i].get(), &threads[i]));
  }

  for (int i = 0; i < kThreadCount; ++i) {
    base::PlatformThread::Join(threads[i]);
  }
}

// Verify that Directory is notifed when a MutableEntry's AttachmentMetadata
// changes.
TEST_F(SyncableDirectoryTest, MutableEntry_PutAttachmentMetadata) {
  sync_pb::AttachmentMetadata attachment_metadata;
  sync_pb::AttachmentMetadataRecord* record = attachment_metadata.add_record();
  sync_pb::AttachmentIdProto attachment_id_proto =
      syncer::CreateAttachmentIdProto();
  *record->mutable_id() = attachment_id_proto;
  ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

    // Create an entry with attachment metadata and see that the attachment id
    // is not linked.
    MutableEntry entry(
        &trans, CREATE, PREFERENCES, trans.root_id(), "some entry");
    entry.PutId(TestIdFactory::FromNumber(-1));
    entry.PutIsUnsynced(true);

    Directory::Metahandles metahandles;
    ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
    dir()->GetMetahandlesByAttachmentId(
        &trans, attachment_id_proto, &metahandles);
    ASSERT_TRUE(metahandles.empty());

    // Now add the attachment metadata and see that Directory believes it is
    // linked.
    entry.PutAttachmentMetadata(attachment_metadata);
    ASSERT_TRUE(dir()->IsAttachmentLinked(attachment_id_proto));
    dir()->GetMetahandlesByAttachmentId(
        &trans, attachment_id_proto, &metahandles);
    ASSERT_FALSE(metahandles.empty());
    ASSERT_EQ(metahandles[0], entry.GetMetahandle());

    // Clear out the attachment metadata and see that it's no longer linked.
    sync_pb::AttachmentMetadata empty_attachment_metadata;
    entry.PutAttachmentMetadata(empty_attachment_metadata);
    ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
    dir()->GetMetahandlesByAttachmentId(
        &trans, attachment_id_proto, &metahandles);
    ASSERT_TRUE(metahandles.empty());
  }
  ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
}

// Verify that UpdateAttachmentId updates attachment_id and is_on_server flag.
TEST_F(SyncableDirectoryTest, MutableEntry_UpdateAttachmentId) {
  sync_pb::AttachmentMetadata attachment_metadata;
  sync_pb::AttachmentMetadataRecord* r1 = attachment_metadata.add_record();
  sync_pb::AttachmentMetadataRecord* r2 = attachment_metadata.add_record();
  *r1->mutable_id() = syncer::CreateAttachmentIdProto();
  *r2->mutable_id() = syncer::CreateAttachmentIdProto();
  sync_pb::AttachmentIdProto attachment_id_proto = r1->id();

  WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());

  MutableEntry entry(
      &trans, CREATE, PREFERENCES, trans.root_id(), "some entry");
  entry.PutId(TestIdFactory::FromNumber(-1));
  entry.PutAttachmentMetadata(attachment_metadata);

  const sync_pb::AttachmentMetadata& entry_metadata =
      entry.GetAttachmentMetadata();
  ASSERT_EQ(2, entry_metadata.record_size());
  ASSERT_FALSE(entry_metadata.record(0).is_on_server());
  ASSERT_FALSE(entry_metadata.record(1).is_on_server());
  ASSERT_FALSE(entry.GetIsUnsynced());

  entry.MarkAttachmentAsOnServer(attachment_id_proto);

  ASSERT_TRUE(entry_metadata.record(0).is_on_server());
  ASSERT_FALSE(entry_metadata.record(1).is_on_server());
  ASSERT_TRUE(entry.GetIsUnsynced());
}

// Verify that deleted entries with attachments will retain the attachments.
TEST_F(SyncableDirectoryTest, Directory_DeleteDoesNotUnlinkAttachments) {
  sync_pb::AttachmentMetadata attachment_metadata;
  sync_pb::AttachmentMetadataRecord* record = attachment_metadata.add_record();
  sync_pb::AttachmentIdProto attachment_id_proto =
      syncer::CreateAttachmentIdProto();
  *record->mutable_id() = attachment_id_proto;
  ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
  const Id id = TestIdFactory::FromNumber(-1);

  // Create an entry with attachment metadata and see that the attachment id
  // is linked.
  CreateEntryWithAttachmentMetadata(
      PREFERENCES, "some entry", id, attachment_metadata);
  ASSERT_TRUE(dir()->IsAttachmentLinked(attachment_id_proto));

  // Delete the entry and see that it's still linked because the entry hasn't
  // yet been purged.
  DeleteEntry(id);
  ASSERT_TRUE(dir()->IsAttachmentLinked(attachment_id_proto));

  // Reload the Directory, purging the deleted entry, and see that the
  // attachment is no longer linked.
  SimulateSaveAndReloadDir();
  ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
}

// Verify that a given attachment can be referenced by multiple entries and that
// any one of the references is sufficient to ensure it remains linked.
TEST_F(SyncableDirectoryTest, Directory_LastReferenceUnlinksAttachments) {
  // Create one attachment.
  sync_pb::AttachmentMetadata attachment_metadata;
  sync_pb::AttachmentMetadataRecord* record = attachment_metadata.add_record();
  sync_pb::AttachmentIdProto attachment_id_proto =
      syncer::CreateAttachmentIdProto();
  *record->mutable_id() = attachment_id_proto;

  // Create two entries, each referencing the attachment.
  const Id id1 = TestIdFactory::FromNumber(-1);
  const Id id2 = TestIdFactory::FromNumber(-2);
  CreateEntryWithAttachmentMetadata(
      PREFERENCES, "some entry", id1, attachment_metadata);
  CreateEntryWithAttachmentMetadata(
      PREFERENCES, "some other entry", id2, attachment_metadata);

  // See that the attachment is considered linked.
  ASSERT_TRUE(dir()->IsAttachmentLinked(attachment_id_proto));

  // Delete the first entry, reload the Directory, see that the attachment is
  // still linked.
  DeleteEntry(id1);
  SimulateSaveAndReloadDir();
  ASSERT_TRUE(dir()->IsAttachmentLinked(attachment_id_proto));

  // Delete the second entry, reload the Directory, see that the attachment is
  // no loner linked.
  DeleteEntry(id2);
  SimulateSaveAndReloadDir();
  ASSERT_FALSE(dir()->IsAttachmentLinked(attachment_id_proto));
}

TEST_F(SyncableDirectoryTest, Directory_GetAttachmentIdsToUpload) {
  // Create one attachment, referenced by two entries.
  AttachmentId attachment_id = AttachmentId::Create();
  sync_pb::AttachmentIdProto attachment_id_proto = attachment_id.GetProto();
  sync_pb::AttachmentMetadata attachment_metadata;
  sync_pb::AttachmentMetadataRecord* record = attachment_metadata.add_record();
  *record->mutable_id() = attachment_id_proto;
  const Id id1 = TestIdFactory::FromNumber(-1);
  const Id id2 = TestIdFactory::FromNumber(-2);
  CreateEntryWithAttachmentMetadata(
      PREFERENCES, "some entry", id1, attachment_metadata);
  CreateEntryWithAttachmentMetadata(
      PREFERENCES, "some other entry", id2, attachment_metadata);

  // See that Directory reports that this attachment is not on the server.
  AttachmentIdSet id_set;
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    dir()->GetAttachmentIdsToUpload(&trans, PREFERENCES, &id_set);
  }
  ASSERT_EQ(1U, id_set.size());
  ASSERT_EQ(attachment_id, *id_set.begin());

  // Call again, but this time with a ModelType for which there are no entries.
  // See that Directory correctly reports that there are none.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    dir()->GetAttachmentIdsToUpload(&trans, PASSWORDS, &id_set);
  }
  ASSERT_TRUE(id_set.empty());

  // Now, mark the attachment as "on the server" via entry_1.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir().get());
    MutableEntry entry_1(&trans, GET_BY_ID, id1);
    entry_1.MarkAttachmentAsOnServer(attachment_id_proto);
  }

  // See that Directory no longer reports that this attachment is not on the
  // server.
  {
    ReadTransaction trans(FROM_HERE, dir().get());
    dir()->GetAttachmentIdsToUpload(&trans, PREFERENCES, &id_set);
  }
  ASSERT_TRUE(id_set.empty());
}

}  // namespace syncable

}  // namespace syncer
