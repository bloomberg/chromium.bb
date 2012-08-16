// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_encryption_handler_impl.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/tracked_objects.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/write_transaction.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/util/cryptographer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD2(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const sync_pb::EncryptedData&));  // NOLINT
  MOCK_METHOD0(OnPassphraseAccepted, void());  // NOLINT
  MOCK_METHOD1(OnBootstrapTokenUpdated, void(const std::string&));  // NOLINT
  MOCK_METHOD2(OnEncryptedTypesChanged,
               void(ModelTypeSet, bool));  // NOLINT
  MOCK_METHOD0(OnEncryptionComplete, void());  // NOLINT
  MOCK_METHOD1(OnCryptographerStateChanged, void(Cryptographer*));  // NOLINT
};

}  // namespace

class SyncEncryptionHandlerImplTest : public ::testing::Test {
 public:
  SyncEncryptionHandlerImplTest() : cryptographer_(NULL) {}
  virtual ~SyncEncryptionHandlerImplTest() {}

  virtual void SetUp() {
    test_user_share_.SetUp();
    SetUpEncryption();
    CreateRootForType(NIGORI);
  }

  virtual void TearDown() {
    test_user_share_.TearDown();
  }

 protected:
  void SetUpEncryption() {
    ReadTransaction trans(FROM_HERE, user_share());
    cryptographer_ = trans.GetCryptographer();
    encryption_handler_.reset(
        new SyncEncryptionHandlerImpl(user_share(),
                                      cryptographer_));
    cryptographer_->SetNigoriHandler(
        encryption_handler_.get());
    encryption_handler_->AddObserver(&observer_);
  }

  void CreateRootForType(ModelType model_type) {
    syncer::syncable::Directory* directory = user_share()->directory.get();

    std::string tag_name = ModelTypeToRootTag(model_type);

    syncable::WriteTransaction wtrans(FROM_HERE, syncable::UNITTEST, directory);
    syncable::MutableEntry node(&wtrans,
                                syncable::CREATE,
                                wtrans.root_id(),
                                tag_name);
    node.Put(syncable::UNIQUE_SERVER_TAG, tag_name);
    node.Put(syncable::IS_DIR, true);
    node.Put(syncable::SERVER_IS_DIR, false);
    node.Put(syncable::IS_UNSYNCED, false);
    node.Put(syncable::IS_UNAPPLIED_UPDATE, false);
    node.Put(syncable::SERVER_VERSION, 20);
    node.Put(syncable::BASE_VERSION, 20);
    node.Put(syncable::IS_DEL, false);
    node.Put(syncable::ID, ids_.MakeServer(tag_name));
    sync_pb::EntitySpecifics specifics;
    syncer::AddDefaultFieldValue(model_type, &specifics);
    node.Put(syncable::SPECIFICS, specifics);
  }

  void PumpLoop() {
    message_loop_.RunAllPending();
  }

  // Getters for tests.
  UserShare* user_share() { return test_user_share_.user_share(); }
  SyncEncryptionHandlerImpl* encryption_handler() {
      return encryption_handler_.get();
  }
  SyncEncryptionHandlerObserverMock* observer() { return &observer_; }
  Cryptographer* cryptographer() { return cryptographer_; }

 private:
  TestUserShare test_user_share_;
  scoped_ptr<SyncEncryptionHandlerImpl> encryption_handler_;
  StrictMock<SyncEncryptionHandlerObserverMock> observer_;
  Cryptographer* cryptographer_;
  TestIdFactory ids_;
  MessageLoop message_loop_;
};

// Verify that the encrypted types are being written to and read from the
// nigori node properly.
TEST_F(SyncEncryptionHandlerImplTest, NigoriEncryptionTypes) {
  sync_pb::NigoriSpecifics nigori;

  StrictMock<SyncEncryptionHandlerObserverMock> observer2;
  SyncEncryptionHandlerImpl handler2(user_share(),
                                     cryptographer());
  handler2.AddObserver(&observer2);

  // Just set the sensitive types (shouldn't trigger any notifications).
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  encryption_handler()->MergeEncryptedTypes(encrypted_types);
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateNigoriFromEncryptedTypes(
        &nigori,
        trans.GetWrappedTrans());
  }
  handler2.UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_TRUE(encrypted_types.Equals(
      encryption_handler()->GetEncryptedTypes()));
  EXPECT_TRUE(encrypted_types.Equals(
      handler2.GetEncryptedTypes()));

  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(&observer2);

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(ModelTypeSet::All()), false));
  EXPECT_CALL(observer2,
              OnEncryptedTypesChanged(
                  HasModelTypes(ModelTypeSet::All()), false));

  // Set all encrypted types
  encrypted_types = ModelTypeSet::All();
  encryption_handler()->MergeEncryptedTypes(encrypted_types);
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateNigoriFromEncryptedTypes(
        &nigori,
        trans.GetWrappedTrans());
  }
  handler2.UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_TRUE(encrypted_types.Equals(
      encryption_handler()->GetEncryptedTypes()));
  EXPECT_TRUE(encrypted_types.Equals(handler2.GetEncryptedTypes()));

  // Receiving an empty nigori should not reset any encrypted types or trigger
  // an observer notification.
  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(&observer2);
  nigori = sync_pb::NigoriSpecifics();
  encryption_handler()->UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_TRUE(encrypted_types.Equals(
      encryption_handler()->GetEncryptedTypes()));
}

// Verify the encryption handler processes the encrypt everything field
// properly.
TEST_F(SyncEncryptionHandlerImplTest, EncryptEverythingExplicit) {
  ModelTypeSet real_types = ModelTypeSet::All();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_everything(true);

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(ModelTypeSet::All()), true));

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  ModelTypeSet encrypted_types = encryption_handler()->GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == PASSWORDS || iter.Get() == NIGORI)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  encryption_handler()->UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    EXPECT_TRUE(encrypted_types.Has(iter.Get()));
  }

  // Receiving the nigori node again shouldn't trigger another notification.
  Mock::VerifyAndClearExpectations(observer());
  encryption_handler()->UpdateEncryptedTypesFromNigori(specifics);
}

// Verify the encryption handler can detect an implicit encrypt everything state
// (from clients that failed to write the encrypt everything field).
TEST_F(SyncEncryptionHandlerImplTest, EncryptEverythingImplicit) {
  ModelTypeSet real_types = ModelTypeSet::All();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_bookmarks(true);  // Non-passwords = encrypt everything

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(ModelTypeSet::All()), true));

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  ModelTypeSet encrypted_types = encryption_handler()->GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == PASSWORDS || iter.Get() == NIGORI)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  encryption_handler()->UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    EXPECT_TRUE(encrypted_types.Has(iter.Get()));
  }

  // Receiving a nigori node with encrypt everything explicitly set shouldn't
  // trigger another notification.
  Mock::VerifyAndClearExpectations(observer());
  specifics.set_encrypt_everything(true);
  encryption_handler()->UpdateEncryptedTypesFromNigori(specifics);
}

// Verify the encryption handler can deal with new versions treating new types
// as Sensitive, and that it does not consider this an implicit encrypt
// everything case.
TEST_F(SyncEncryptionHandlerImplTest, UnknownSensitiveTypes) {
  ModelTypeSet real_types = ModelTypeSet::All();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_everything(false);
  specifics.set_encrypt_bookmarks(true);

  ModelTypeSet expected_encrypted_types =
      SyncEncryptionHandler::SensitiveTypes();
  expected_encrypted_types.Put(BOOKMARKS);

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(expected_encrypted_types), false));

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  ModelTypeSet encrypted_types = encryption_handler()->GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == PASSWORDS || iter.Get() == NIGORI)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  encryption_handler()->UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == PASSWORDS ||
        iter.Get() == NIGORI ||
        iter.Get() == BOOKMARKS)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }
}

// Receive an old nigori with old encryption keys and encrypted types. We should
// not revert our default key or encrypted types, and should post a task to
// overwrite the existing nigori with the correct data.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveOldNigori) {
  KeyParams old_key = {"localhost", "dummy", "old"};
  KeyParams current_key = {"localhost", "dummy", "cur"};

  // Data for testing encryption/decryption.
  Cryptographer other_cryptographer(cryptographer()->encryptor());
  other_cryptographer.AddKey(old_key);
  sync_pb::EntitySpecifics other_encrypted_specifics;
  other_encrypted_specifics.mutable_bookmark()->set_title("title");
  other_cryptographer.Encrypt(
      other_encrypted_specifics,
      other_encrypted_specifics.mutable_encrypted());
  sync_pb::EntitySpecifics our_encrypted_specifics;
  our_encrypted_specifics.mutable_bookmark()->set_title("title2");
  ModelTypeSet encrypted_types = ModelTypeSet::All();

  // Set up the current encryption state (containing both keys and encrypt
  // everything).
  sync_pb::NigoriSpecifics current_nigori_specifics;
  cryptographer()->AddKey(old_key);
  cryptographer()->AddKey(current_key);
  cryptographer()->Encrypt(
      our_encrypted_specifics,
      our_encrypted_specifics.mutable_encrypted());
  cryptographer()->GetKeys(
      current_nigori_specifics.mutable_encrypted());
  current_nigori_specifics.set_encrypt_everything(true);

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_));
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
      HasModelTypes(ModelTypeSet::All()), true));
  {
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(
        current_nigori_specifics,
        trans.GetWrappedTrans());
  }
  Mock::VerifyAndClearExpectations(observer());

  // Now set up the old nigori specifics and apply it on top.
  // Has an old set of keys, and no encrypted types.
  sync_pb::NigoriSpecifics old_nigori;
  other_cryptographer.GetKeys(old_nigori.mutable_encrypted());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_));
  {
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(
        old_nigori,
        trans.GetWrappedTrans());
  }
  EXPECT_TRUE(cryptographer()->is_ready());
  EXPECT_FALSE(cryptographer()->has_pending_keys());

  // Encryption handler should have posted a task to overwrite the old
  // specifics.
  PumpLoop();

  {
    // The cryptographer should be able to decrypt both sets of keys and still
    // be encrypting with the newest, and the encrypted types should be the
    // most recent.
    // In addition, the nigori node should match the current encryption state.
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(ModelTypeToRootTag(NIGORI)),
              BaseNode::INIT_OK);
    const sync_pb::NigoriSpecifics& nigori = nigori_node.GetNigoriSpecifics();
    EXPECT_TRUE(cryptographer()->CanDecryptUsingDefaultKey(
        our_encrypted_specifics.encrypted()));
    EXPECT_TRUE(cryptographer()->CanDecrypt(
        other_encrypted_specifics.encrypted()));
    EXPECT_TRUE(cryptographer()->CanDecrypt(nigori.encrypted()));
    EXPECT_TRUE(nigori.encrypt_everything());
    EXPECT_TRUE(cryptographer()->CanDecryptUsingDefaultKey(nigori.encrypted()));
  }
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
}

}  // namespace syncer
