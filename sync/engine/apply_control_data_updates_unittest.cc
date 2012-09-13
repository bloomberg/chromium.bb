// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "sync/engine/apply_control_data_updates.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_util.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/nigori_util.h"
#include "sync/syncable/read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/fake_sync_encryption_handler.h"
#include "sync/util/cryptographer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using syncable::MutableEntry;
using syncable::UNITTEST;
using syncable::Id;

class ApplyControlDataUpdatesTest : public SyncerCommandTest {
 public:
 protected:
  ApplyControlDataUpdatesTest() {}
  virtual ~ApplyControlDataUpdatesTest() {}

  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();
    workers()->push_back(make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    (*mutable_routing_info())[BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[PASSWORDS] = GROUP_PASSWORD;
    (*mutable_routing_info())[NIGORI] = GROUP_PASSIVE;
    SyncerCommandTest::SetUp();
    entry_factory_.reset(new TestEntryFactory(directory()));

    syncable::ReadTransaction trans(FROM_HERE, directory());
  }

  TestIdFactory id_factory_;
  scoped_ptr<TestEntryFactory> entry_factory_;
 private:
  DISALLOW_COPY_AND_ASSIGN(ApplyControlDataUpdatesTest);
};

TEST_F(ApplyControlDataUpdatesTest, NigoriUpdate) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types;
  encrypted_types.Put(PASSWORDS);

  // We start with initial_sync_ended == false.  This is wrong, since would have
  // no nigori node if that were the case.  Howerver, it makes it easier to
  // verify that ApplyControlDataUpdates sets initial_sync_ended correctly.
  EXPECT_FALSE(directory()->initial_sync_ended_types().Has(NIGORI));

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(encrypted_types));
  }

  // Nigori node updates should update the Cryptographer.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  KeyParams params = {"localhost", "dummy", "foobar"};
  other_cryptographer.AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori = specifics.mutable_nigori();
  other_cryptographer.GetKeys(nigori->mutable_encryption_keybag());
  nigori->set_encrypt_everything(true);
  entry_factory_->CreateUnappliedNewItem(
      ModelTypeToRootTag(NIGORI), specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());

  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
    EXPECT_TRUE(directory()->initial_sync_ended_types().Has(NIGORI));
  }
}

TEST_F(ApplyControlDataUpdatesTest, NigoriUpdateForDisabledTypes) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types;
  encrypted_types.Put(PASSWORDS);

  // We start with initial_sync_ended == false.  This is wrong, since would have
  // no nigori node if that were the case.  Howerver, it makes it easier to
  // verify that ApplyControlDataUpdates sets initial_sync_ended correctly.
  EXPECT_FALSE(directory()->initial_sync_ended_types().Has(NIGORI));

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(encrypted_types));
  }

  // Nigori node updates should update the Cryptographer.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  KeyParams params = {"localhost", "dummy", "foobar"};
  other_cryptographer.AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori = specifics.mutable_nigori();
  other_cryptographer.GetKeys(nigori->mutable_encryption_keybag());
  nigori->set_encrypt_everything(true);
  entry_factory_->CreateUnappliedNewItem(
      ModelTypeToRootTag(NIGORI), specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());

  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
    EXPECT_TRUE(directory()->initial_sync_ended_types().Has(NIGORI));
  }
}

// Create some local unsynced and unencrypted data. Apply a nigori update that
// turns on encryption for the unsynced data. Ensure we properly encrypt the
// data as part of the nigori update. Apply another nigori update with no
// changes. Ensure we ignore already-encrypted unsynced data and that nothing
// breaks.
TEST_F(ApplyControlDataUpdatesTest, EncryptUnsyncedChanges) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types;
  encrypted_types.Put(PASSWORDS);
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(encrypted_types));

    // With default encrypted_types, this should be true.
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_TRUE(handles.empty());
  }

  // Create unsynced bookmarks without encryption.
  // First item is a folder
  Id folder_id = id_factory_.NewLocalId();
  entry_factory_->CreateUnsyncedItem(folder_id, id_factory_.root(), "folder",
                                     true, BOOKMARKS, NULL);
  // Next five items are children of the folder
  size_t i;
  size_t batch_s = 5;
  for (i = 0; i < batch_s; ++i) {
    entry_factory_->CreateUnsyncedItem(id_factory_.NewLocalId(), folder_id,
                                       base::StringPrintf("Item %"PRIuS"", i),
                                       false, BOOKMARKS, NULL);
  }
  // Next five items are children of the root.
  for (; i < 2*batch_s; ++i) {
    entry_factory_->CreateUnsyncedItem(
        id_factory_.NewLocalId(), id_factory_.root(),
        base::StringPrintf("Item %"PRIuS"", i), false,
        BOOKMARKS, NULL);
  }

  KeyParams params = {"localhost", "dummy", "foobar"};
  cryptographer->AddKey(params);
  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori = specifics.mutable_nigori();
  cryptographer->GetKeys(nigori->mutable_encryption_keybag());
  nigori->set_encrypt_everything(true);
  encrypted_types.Put(BOOKMARKS);
  entry_factory_->CreateUnappliedNewItem(
      ModelTypeToRootTag(NIGORI), specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());
  EXPECT_TRUE(cryptographer->is_ready());

  {
    // Ensure we have unsynced nodes that aren't properly encrypted.
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_FALSE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }

  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(cryptographer->has_pending_keys());
  EXPECT_TRUE(cryptographer->is_ready());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // If ProcessUnsyncedChangesForEncryption worked, all our unsynced changes
    // should be encrypted now.
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }

  // Simulate another nigori update that doesn't change anything.
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::GET_BY_SERVER_TAG,
                       ModelTypeToRootTag(NIGORI));
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::SERVER_VERSION, entry_factory_->GetNextRevision());
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  }

  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(cryptographer->has_pending_keys());
  EXPECT_TRUE(cryptographer->is_ready());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // All our changes should still be encrypted.
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }
}

TEST_F(ApplyControlDataUpdatesTest, CannotEncryptUnsyncedChanges) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types;
  encrypted_types.Put(PASSWORDS);
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(encrypted_types));

    // With default encrypted_types, this should be true.
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_TRUE(handles.empty());
  }

  // Create unsynced bookmarks without encryption.
  // First item is a folder
  Id folder_id = id_factory_.NewLocalId();
  entry_factory_->CreateUnsyncedItem(
      folder_id, id_factory_.root(), "folder", true,
      BOOKMARKS, NULL);
  // Next five items are children of the folder
  size_t i;
  size_t batch_s = 5;
  for (i = 0; i < batch_s; ++i) {
    entry_factory_->CreateUnsyncedItem(id_factory_.NewLocalId(), folder_id,
                                       base::StringPrintf("Item %"PRIuS"", i),
                                       false, BOOKMARKS, NULL);
  }
  // Next five items are children of the root.
  for (; i < 2*batch_s; ++i) {
    entry_factory_->CreateUnsyncedItem(
        id_factory_.NewLocalId(), id_factory_.root(),
        base::StringPrintf("Item %"PRIuS"", i), false,
        BOOKMARKS, NULL);
  }

  // We encrypt with new keys, triggering the local cryptographer to be unready
  // and unable to decrypt data (once updated).
  Cryptographer other_cryptographer(cryptographer->encryptor());
  KeyParams params = {"localhost", "dummy", "foobar"};
  other_cryptographer.AddKey(params);
  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori = specifics.mutable_nigori();
  other_cryptographer.GetKeys(nigori->mutable_encryption_keybag());
  nigori->set_encrypt_everything(true);
  encrypted_types.Put(BOOKMARKS);
  entry_factory_->CreateUnappliedNewItem(
      ModelTypeToRootTag(NIGORI), specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());

  {
    // Ensure we have unsynced nodes that aren't properly encrypted.
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_FALSE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));
    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }

  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // Since we have pending keys, we would have failed to encrypt, but the
    // cryptographer should be updated.
    EXPECT_FALSE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
    EXPECT_FALSE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->has_pending_keys());

    Syncer::UnsyncedMetaHandles handles;
    GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }
}

}  // namespace syncer
