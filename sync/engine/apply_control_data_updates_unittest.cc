// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "sync/engine/apply_control_data_updates.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_util.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/nigori_util.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/fake_sync_encryption_handler.h"
#include "sync/util/cryptographer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using syncable::MutableEntry;
using syncable::UNITTEST;
using syncable::Id;

class ApplyControlDataUpdatesTest : public ::testing::Test {
 public:
 protected:
  ApplyControlDataUpdatesTest() {}
  virtual ~ApplyControlDataUpdatesTest() {}

  virtual void SetUp() {
    dir_maker_.SetUp();
    entry_factory_.reset(new TestEntryFactory(directory()));
  }

  virtual void TearDown() {
    dir_maker_.TearDown();
  }

  syncable::Directory* directory() {
    return dir_maker_.directory();
  }

  TestIdFactory id_factory_;
  scoped_ptr<TestEntryFactory> entry_factory_;
 private:
  base::MessageLoop loop_;  // Needed for directory init.
  TestDirectorySetterUpper dir_maker_;

  DISALLOW_COPY_AND_ASSIGN(ApplyControlDataUpdatesTest);
};

// Verify that applying a nigori node sets initial sync ended properly,
// updates the set of encrypted types, and updates the cryptographer.
TEST_F(ApplyControlDataUpdatesTest, NigoriUpdate) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types;
  encrypted_types.PutAll(SyncEncryptionHandler::SensitiveTypes());

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
  encrypted_types.PutAll(SyncEncryptionHandler::SensitiveTypes());
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
                                       base::StringPrintf("Item %" PRIuS "", i),
                                       false, BOOKMARKS, NULL);
  }
  // Next five items are children of the root.
  for (; i < 2*batch_s; ++i) {
    entry_factory_->CreateUnsyncedItem(
        id_factory_.NewLocalId(), id_factory_.root(),
        base::StringPrintf("Item %" PRIuS "", i), false,
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
    MutableEntry entry(&trans, syncable::GET_TYPE_ROOT, NIGORI);
    ASSERT_TRUE(entry.good());
    entry.PutServerVersion(entry_factory_->GetNextRevision());
    entry.PutIsUnappliedUpdate(true);
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

// Create some local unsynced and unencrypted changes. Receive a new nigori
// node enabling their encryption but also introducing pending keys. Ensure
// we apply the update properly without encrypting the unsynced changes or
// breaking.
TEST_F(ApplyControlDataUpdatesTest, CannotEncryptUnsyncedChanges) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types;
  encrypted_types.PutAll(SyncEncryptionHandler::SensitiveTypes());
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
                                       base::StringPrintf("Item %" PRIuS "", i),
                                       false, BOOKMARKS, NULL);
  }
  // Next five items are children of the root.
  for (; i < 2*batch_s; ++i) {
    entry_factory_->CreateUnsyncedItem(
        id_factory_.NewLocalId(), id_factory_.root(),
        base::StringPrintf("Item %" PRIuS "", i), false,
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

// Verify we handle a nigori node conflict by merging encryption keys and
// types, but preserve the custom passphrase state of the server.
// Initial sync ended should be set.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictPendingKeysServerEncryptEverythingCustom) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams other_params = {"localhost", "dummy", "foobar"};
  KeyParams local_params = {"localhost", "dummy", "local"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up a temporary cryptographer to generate new keys with.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  other_cryptographer.AddKey(other_params);

  // Create server specifics with pending keys, new encrypted types,
  // and a custom passphrase (unmigrated).
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  other_cryptographer.GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(true);
  server_nigori->set_keybag_is_frozen(true);
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Initialize the local cryptographer with the local keys.
  cryptographer->AddKey(local_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with the local encryption keys and default encrypted
  // types.
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(false);
  local_nigori->set_keybag_is_frozen(true);
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->is_initialized());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  EXPECT_TRUE(other_cryptographer.CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().encrypt_everything());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
  }
}

// Verify we handle a nigori node conflict by merging encryption keys and
// types, but preserve the custom passphrase state of the server.
// Initial sync ended should be set.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictPendingKeysLocalEncryptEverythingCustom) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams other_params = {"localhost", "dummy", "foobar"};
  KeyParams local_params = {"localhost", "dummy", "local"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up a temporary cryptographer to generate new keys with.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  other_cryptographer.AddKey(other_params);

  // Create server specifics with pending keys, new encrypted types,
  // and a custom passphrase (unmigrated).
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  other_cryptographer.GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(false);
  server_nigori->set_keybag_is_frozen(false);
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Initialize the local cryptographer with the local keys.
  cryptographer->AddKey(local_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with the local encryption keys and default encrypted
  // types.
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(true);
  local_nigori->set_keybag_is_frozen(true);
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->is_initialized());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  EXPECT_TRUE(other_cryptographer.CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_FALSE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().encrypt_everything());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
  }
}

// If the conflicting nigori has a subset of the local keys, the conflict
// resolution should preserve the full local keys. Initial sync ended should be
// set.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictOldKeys) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams old_params = {"localhost", "dummy", "old"};
  KeyParams new_params = {"localhost", "dummy", "new"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up the cryptographer with old keys
  cryptographer->AddKey(old_params);

  // Create server specifics with old keys and new encrypted types.
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  cryptographer->GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(true);
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Add the new keys to the cryptogrpaher
  cryptographer->AddKey(new_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with the superset of keys.
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(false);
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_TRUE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_FALSE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().encrypt_everything());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
  }
}

// If both nigoris are migrated, but we also set a custom passphrase locally,
// the local nigori should be preserved.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictBothMigratedLocalCustom) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams old_params = {"localhost", "dummy", "old"};
  KeyParams new_params = {"localhost", "dummy", "new"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up the cryptographer with new keys
  Cryptographer other_cryptographer(cryptographer->encryptor());
  other_cryptographer.AddKey(old_params);

  // Create server specifics with a migrated keystore passphrase type.
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  other_cryptographer.GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(false);
  server_nigori->set_keybag_is_frozen(true);
  server_nigori->set_passphrase_type(
      sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  server_nigori->mutable_keystore_decryptor_token();
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Add the new keys to the cryptographer.
  cryptographer->AddKey(old_params);
  cryptographer->AddKey(new_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with a migrated custom passphrase type
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(true);
  local_nigori->set_keybag_is_frozen(true);
  local_nigori->set_passphrase_type(
      sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_TRUE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().encrypt_everything());
  EXPECT_EQ(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE,
            entry_factory_->GetLocalSpecificsForItem(nigori_handle).
                nigori().passphrase_type());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
  }
}

// If both nigoris are migrated, but a custom passphrase with a new key was
// set remotely, the remote nigori should be preserved.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictBothMigratedServerCustom) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams old_params = {"localhost", "dummy", "old"};
  KeyParams new_params = {"localhost", "dummy", "new"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up the cryptographer with both new keys and old keys.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  other_cryptographer.AddKey(old_params);
  other_cryptographer.AddKey(new_params);

  // Create server specifics with a migrated custom passphrase type.
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  other_cryptographer.GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(true);
  server_nigori->set_keybag_is_frozen(true);
  server_nigori->set_passphrase_type(
      sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Add the old keys to the cryptographer.
  cryptographer->AddKey(old_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with a migrated keystore passphrase type
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(false);
  local_nigori->set_keybag_is_frozen(true);
  local_nigori->set_passphrase_type(
      sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  server_nigori->mutable_keystore_decryptor_token();
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_TRUE(cryptographer->is_initialized());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  EXPECT_TRUE(other_cryptographer.CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().encrypt_everything());
  EXPECT_EQ(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE,
            entry_factory_->GetLocalSpecificsForItem(nigori_handle).
                nigori().passphrase_type());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
  }
}

// If the local nigori is migrated but the server is not, preserve the local
// nigori.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictLocalMigrated) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams old_params = {"localhost", "dummy", "old"};
  KeyParams new_params = {"localhost", "dummy", "new"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up the cryptographer with both new keys and old keys.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  other_cryptographer.AddKey(old_params);

  // Create server specifics with an unmigrated implicit passphrase type.
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  other_cryptographer.GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(true);
  server_nigori->set_keybag_is_frozen(false);
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Add the old keys to the cryptographer.
  cryptographer->AddKey(old_params);
  cryptographer->AddKey(new_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with a migrated custom passphrase type
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(true);
  local_nigori->set_keybag_is_frozen(true);
  local_nigori->set_passphrase_type(
      sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_TRUE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().encrypt_everything());
  EXPECT_EQ(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE,
            entry_factory_->GetLocalSpecificsForItem(nigori_handle).
                nigori().passphrase_type());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)
        .Equals(ModelTypeSet::All()));
  }
}

// If the server nigori is migrated but the local is not, preserve the server
// nigori.
TEST_F(ApplyControlDataUpdatesTest,
       NigoriConflictServerMigrated) {
  Cryptographer* cryptographer;
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  KeyParams old_params = {"localhost", "dummy", "old"};
  KeyParams new_params = {"localhost", "dummy", "new"};
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    EXPECT_TRUE(encrypted_types.Equals(
            directory()->GetNigoriHandler()->GetEncryptedTypes(&trans)));
  }

  // Set up the cryptographer with both new keys and old keys.
  Cryptographer other_cryptographer(cryptographer->encryptor());
  other_cryptographer.AddKey(old_params);

  // Create server specifics with an migrated keystore passphrase type.
  sync_pb::EntitySpecifics server_specifics;
  sync_pb::NigoriSpecifics* server_nigori = server_specifics.mutable_nigori();
  other_cryptographer.GetKeys(server_nigori->mutable_encryption_keybag());
  server_nigori->set_encrypt_everything(false);
  server_nigori->set_keybag_is_frozen(true);
  server_nigori->set_passphrase_type(
      sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  server_nigori->mutable_keystore_decryptor_token();
  int64 nigori_handle =
      entry_factory_->CreateUnappliedNewItem(kNigoriTag,
                                             server_specifics,
                                             true);

  // Add the old keys to the cryptographer.
  cryptographer->AddKey(old_params);
  cryptographer->AddKey(new_params);
  EXPECT_TRUE(cryptographer->is_ready());

  // Set up a local nigori with a migrated custom passphrase type
  sync_pb::EntitySpecifics local_specifics;
  sync_pb::NigoriSpecifics* local_nigori = local_specifics.mutable_nigori();
  cryptographer->GetKeys(local_nigori->mutable_encryption_keybag());
  local_nigori->set_encrypt_everything(false);
  local_nigori->set_keybag_is_frozen(false);
  ASSERT_TRUE(entry_factory_->SetLocalSpecificsForItem(
          nigori_handle, local_specifics));
  // Apply the update locally so that UpdateFromEncryptedTypes knows what state
  // to use.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
    directory()->GetNigoriHandler()->ApplyNigoriUpdate(
        *local_nigori,
        &trans);
  }

  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_TRUE(entry_factory_->GetIsUnappliedForItem(nigori_handle));
  ApplyControlDataUpdates(directory());
  EXPECT_TRUE(entry_factory_->GetIsUnsyncedForItem(nigori_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(nigori_handle));

  EXPECT_TRUE(cryptographer->is_ready());
  // Note: we didn't overwrite the encryption keybag with the local keys. The
  // sync encryption handler will do that when it detects that the new
  // keybag is out of date (and update the keystore bootstrap if necessary).
  EXPECT_FALSE(cryptographer->CanDecryptUsingDefaultKey(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_TRUE(cryptographer->CanDecrypt(
          entry_factory_->GetLocalSpecificsForItem(nigori_handle).
              nigori().encryption_keybag()));
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().keybag_is_frozen());
  EXPECT_TRUE(entry_factory_->GetLocalSpecificsForItem(nigori_handle).
      nigori().has_keystore_decryptor_token());
  EXPECT_EQ(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE,
            entry_factory_->GetLocalSpecificsForItem(nigori_handle).
                nigori().passphrase_type());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
  }
}

// Check that we can apply a simple control datatype node successfully.
TEST_F(ApplyControlDataUpdatesTest, ControlApply) {
  std::string experiment_id = "experiment";
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_experiments()->mutable_keystore_encryption()->
      set_enabled(true);
  int64 experiment_handle = entry_factory_->CreateUnappliedNewItem(
      experiment_id, specifics, false);
  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(experiment_handle));
  EXPECT_TRUE(
      entry_factory_->GetLocalSpecificsForItem(experiment_handle).
          experiments().keystore_encryption().enabled());
}

// Verify that we apply top level folders before their children.
TEST_F(ApplyControlDataUpdatesTest, ControlApplyParentBeforeChild) {
  std::string parent_id = "parent";
  std::string experiment_id = "experiment";
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_experiments()->mutable_keystore_encryption()->
      set_enabled(true);
  int64 experiment_handle = entry_factory_->CreateUnappliedNewItemWithParent(
      experiment_id, specifics, parent_id);
  int64 parent_handle = entry_factory_->CreateUnappliedNewItem(
      parent_id, specifics, true);
  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(parent_handle));
  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(experiment_handle));
  EXPECT_TRUE(
      entry_factory_->GetLocalSpecificsForItem(experiment_handle).
          experiments().keystore_encryption().enabled());
}

// Verify that we handle control datatype conflicts by preserving the server
// data.
TEST_F(ApplyControlDataUpdatesTest, ControlConflict) {
  std::string experiment_id = "experiment";
  sync_pb::EntitySpecifics local_specifics, server_specifics;
  server_specifics.mutable_experiments()->mutable_keystore_encryption()->
      set_enabled(true);
  local_specifics.mutable_experiments()->mutable_keystore_encryption()->
      set_enabled(false);
  int64 experiment_handle = entry_factory_->CreateSyncedItem(
      experiment_id, EXPERIMENTS, false);
  entry_factory_->SetServerSpecificsForItem(experiment_handle,
                                            server_specifics);
  entry_factory_->SetLocalSpecificsForItem(experiment_handle,
                                           local_specifics);
  ApplyControlDataUpdates(directory());

  EXPECT_FALSE(entry_factory_->GetIsUnappliedForItem(experiment_handle));
  EXPECT_TRUE(
      entry_factory_->GetLocalSpecificsForItem(experiment_handle).
          experiments().keystore_encryption().enabled());
}

}  // namespace syncer
