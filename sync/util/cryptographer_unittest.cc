// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/cryptographer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/test/fake_encryptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;
using syncable::ModelTypeSet;

class MockObserver : public Cryptographer::Observer {
 public:
  MOCK_METHOD2(OnEncryptedTypesChanged,
               void(syncable::ModelTypeSet, bool));
};

}  // namespace

class SyncCryptographerTest : public ::testing::Test {
 protected:
  SyncCryptographerTest() : cryptographer_(&encryptor_) {}

  FakeEncryptor encryptor_;
  Cryptographer cryptographer_;
};

TEST_F(SyncCryptographerTest, EmptyCantDecrypt) {
  EXPECT_FALSE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer_.CanDecrypt(encrypted));
}

TEST_F(SyncCryptographerTest, EmptyCantEncrypt) {
  EXPECT_FALSE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted;
  sync_pb::PasswordSpecificsData original;
  EXPECT_FALSE(cryptographer_.Encrypt(original, &encrypted));
}

TEST_F(SyncCryptographerTest, MissingCantDecrypt) {
  KeyParams params = {"localhost", "dummy", "dummy"};
  cryptographer_.AddKey(params);
  EXPECT_TRUE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer_.CanDecrypt(encrypted));
}

TEST_F(SyncCryptographerTest, CanEncryptAndDecrypt) {
  KeyParams params = {"localhost", "dummy", "dummy"};
  EXPECT_TRUE(cryptographer_.AddKey(params));
  EXPECT_TRUE(cryptographer_.is_ready());

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  sync_pb::EncryptedData encrypted;
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted));

  sync_pb::PasswordSpecificsData decrypted;
  EXPECT_TRUE(cryptographer_.Decrypt(encrypted, &decrypted));

  EXPECT_EQ(original.SerializeAsString(), decrypted.SerializeAsString());
}

TEST_F(SyncCryptographerTest, EncryptOnlyIfDifferent) {
  KeyParams params = {"localhost", "dummy", "dummy"};
  EXPECT_TRUE(cryptographer_.AddKey(params));
  EXPECT_TRUE(cryptographer_.is_ready());

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  sync_pb::EncryptedData encrypted;
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted));

  sync_pb::EncryptedData encrypted2, encrypted3;
  encrypted2.CopyFrom(encrypted);
  encrypted3.CopyFrom(encrypted);
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted2));

  // Now encrypt with a new default key. Should overwrite the old data.
  KeyParams params_new = {"localhost", "dummy", "dummy2"};
  cryptographer_.AddKey(params_new);
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted3));

  sync_pb::PasswordSpecificsData decrypted;
  EXPECT_TRUE(cryptographer_.Decrypt(encrypted2, &decrypted));
  // encrypted2 should match encrypted, encrypted3 should not (due to salting).
  EXPECT_EQ(encrypted.SerializeAsString(), encrypted2.SerializeAsString());
  EXPECT_NE(encrypted.SerializeAsString(), encrypted3.SerializeAsString());
  EXPECT_EQ(original.SerializeAsString(), decrypted.SerializeAsString());
}

TEST_F(SyncCryptographerTest, AddKeySetsDefault) {
  KeyParams params1 = {"localhost", "dummy", "dummy1"};
  EXPECT_TRUE(cryptographer_.AddKey(params1));
  EXPECT_TRUE(cryptographer_.is_ready());

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  sync_pb::EncryptedData encrypted1;
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted1));
  sync_pb::EncryptedData encrypted2;
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted2));

  KeyParams params2 = {"localhost", "dummy", "dummy2"};
  EXPECT_TRUE(cryptographer_.AddKey(params2));
  EXPECT_TRUE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted3;
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted3));
  sync_pb::EncryptedData encrypted4;
  EXPECT_TRUE(cryptographer_.Encrypt(original, &encrypted4));

  EXPECT_EQ(encrypted1.key_name(), encrypted2.key_name());
  EXPECT_NE(encrypted1.key_name(), encrypted3.key_name());
  EXPECT_EQ(encrypted3.key_name(), encrypted4.key_name());
}

// Crashes, Bug 55178.
#if defined(OS_WIN)
#define MAYBE_EncryptExportDecrypt DISABLED_EncryptExportDecrypt
#else
#define MAYBE_EncryptExportDecrypt EncryptExportDecrypt
#endif
TEST_F(SyncCryptographerTest, MAYBE_EncryptExportDecrypt) {
  sync_pb::EncryptedData nigori;
  sync_pb::EncryptedData encrypted;

  sync_pb::PasswordSpecificsData original;
  original.set_origin("http://example.com");
  original.set_username_value("azure");
  original.set_password_value("hunter2");

  {
    Cryptographer cryptographer(&encryptor_);

    KeyParams params = {"localhost", "dummy", "dummy"};
    cryptographer.AddKey(params);
    EXPECT_TRUE(cryptographer.is_ready());

    EXPECT_TRUE(cryptographer.Encrypt(original, &encrypted));
    EXPECT_TRUE(cryptographer.GetKeys(&nigori));
  }

  {
    Cryptographer cryptographer(&encryptor_);
    EXPECT_FALSE(cryptographer.CanDecrypt(nigori));

    cryptographer.SetPendingKeys(nigori);
    EXPECT_FALSE(cryptographer.is_ready());
    EXPECT_TRUE(cryptographer.has_pending_keys());

    KeyParams params = {"localhost", "dummy", "dummy"};
    EXPECT_TRUE(cryptographer.DecryptPendingKeys(params));
    EXPECT_TRUE(cryptographer.is_ready());
    EXPECT_FALSE(cryptographer.has_pending_keys());

    sync_pb::PasswordSpecificsData decrypted;
    EXPECT_TRUE(cryptographer.Decrypt(encrypted, &decrypted));
    EXPECT_EQ(original.SerializeAsString(), decrypted.SerializeAsString());
  }
}

// Crashes, Bug 55178.
#if defined(OS_WIN)
#define MAYBE_PackUnpack DISABLED_PackUnpack
#else
#define MAYBE_PackUnpack PackUnpack
#endif
TEST_F(SyncCryptographerTest, MAYBE_PackUnpack) {
  Nigori nigori;
  ASSERT_TRUE(nigori.InitByDerivation("example.com", "username", "password"));
  std::string expected_user, expected_encryption, expected_mac;
  ASSERT_TRUE(nigori.ExportKeys(&expected_user, &expected_encryption,
                                &expected_mac));

  std::string token;
  EXPECT_TRUE(cryptographer_.PackBootstrapToken(&nigori, &token));
  EXPECT_TRUE(IsStringUTF8(token));

  scoped_ptr<Nigori> unpacked(cryptographer_.UnpackBootstrapToken(token));
  EXPECT_NE(static_cast<Nigori*>(NULL), unpacked.get());

  std::string user_key, encryption_key, mac_key;
  ASSERT_TRUE(unpacked->ExportKeys(&user_key, &encryption_key, &mac_key));

  EXPECT_EQ(expected_user, user_key);
  EXPECT_EQ(expected_encryption, encryption_key);
  EXPECT_EQ(expected_mac, mac_key);
}

TEST_F(SyncCryptographerTest, NigoriEncryptionTypes) {
  Cryptographer cryptographer2(&encryptor_);
  sync_pb::NigoriSpecifics nigori;

  StrictMock<MockObserver> observer;
  cryptographer_.AddObserver(&observer);
  StrictMock<MockObserver> observer2;
  cryptographer2.AddObserver(&observer2);

  // Just set the sensitive types (shouldn't trigger any
  // notifications).
  ModelTypeSet encrypted_types(Cryptographer::SensitiveTypes());
  cryptographer_.MergeEncryptedTypesForTest(encrypted_types);
  cryptographer_.UpdateNigoriFromEncryptedTypes(&nigori);
  cryptographer2.UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_TRUE(encrypted_types.Equals(cryptographer_.GetEncryptedTypes()));
  EXPECT_TRUE(encrypted_types.Equals(cryptographer2.GetEncryptedTypes()));

  Mock::VerifyAndClearExpectations(&observer);
  Mock::VerifyAndClearExpectations(&observer2);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(
                  HasModelTypes(syncable::ModelTypeSet::All()),
                  false));
  EXPECT_CALL(observer2,
              OnEncryptedTypesChanged(
                  HasModelTypes(syncable::ModelTypeSet::All()),
                  false));

  // Set all encrypted types
  encrypted_types = syncable::ModelTypeSet::All();
  cryptographer_.MergeEncryptedTypesForTest(encrypted_types);
  cryptographer_.UpdateNigoriFromEncryptedTypes(&nigori);
  cryptographer2.UpdateEncryptedTypesFromNigori(nigori);
  EXPECT_TRUE(encrypted_types.Equals(cryptographer_.GetEncryptedTypes()));
  EXPECT_TRUE(encrypted_types.Equals(cryptographer2.GetEncryptedTypes()));

   // Receiving an empty nigori should not reset any encrypted types or trigger
   // an observer notification.
   Mock::VerifyAndClearExpectations(&observer);
   nigori = sync_pb::NigoriSpecifics();
   cryptographer_.UpdateEncryptedTypesFromNigori(nigori);
   EXPECT_TRUE(encrypted_types.Equals(cryptographer_.GetEncryptedTypes()));
}

TEST_F(SyncCryptographerTest, EncryptEverythingExplicit) {
  ModelTypeSet real_types = syncable::ModelTypeSet::All();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_everything(true);

  StrictMock<MockObserver> observer;
  cryptographer_.AddObserver(&observer);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(
                  HasModelTypes(syncable::ModelTypeSet::All()), true));

  EXPECT_FALSE(cryptographer_.encrypt_everything());
  ModelTypeSet encrypted_types = cryptographer_.GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == syncable::PASSWORDS || iter.Get() == syncable::NIGORI)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  cryptographer_.UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_TRUE(cryptographer_.encrypt_everything());
  encrypted_types = cryptographer_.GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    EXPECT_TRUE(encrypted_types.Has(iter.Get()));
  }

  // Shouldn't trigger another notification.
  specifics.set_encrypt_everything(true);

  cryptographer_.RemoveObserver(&observer);
}

TEST_F(SyncCryptographerTest, EncryptEverythingImplicit) {
  ModelTypeSet real_types = syncable::ModelTypeSet::All();
  sync_pb::NigoriSpecifics specifics;
  specifics.set_encrypt_bookmarks(true);  // Non-passwords = encrypt everything

  StrictMock<MockObserver> observer;
  cryptographer_.AddObserver(&observer);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(
                  HasModelTypes(syncable::ModelTypeSet::All()), true));

  EXPECT_FALSE(cryptographer_.encrypt_everything());
  ModelTypeSet encrypted_types = cryptographer_.GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == syncable::PASSWORDS || iter.Get() == syncable::NIGORI)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  cryptographer_.UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_TRUE(cryptographer_.encrypt_everything());
  encrypted_types = cryptographer_.GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    EXPECT_TRUE(encrypted_types.Has(iter.Get()));
  }

  // Shouldn't trigger another notification.
  specifics.set_encrypt_everything(true);

  cryptographer_.RemoveObserver(&observer);
}

TEST_F(SyncCryptographerTest, UnknownSensitiveTypes) {
  ModelTypeSet real_types = syncable::ModelTypeSet::All();
  sync_pb::NigoriSpecifics specifics;
  // Explicitly setting encrypt everything should override logic for implicit
  // encrypt everything.
  specifics.set_encrypt_everything(false);
  specifics.set_encrypt_bookmarks(true);

  StrictMock<MockObserver> observer;
  cryptographer_.AddObserver(&observer);

  syncable::ModelTypeSet expected_encrypted_types =
      Cryptographer::SensitiveTypes();
  expected_encrypted_types.Put(syncable::BOOKMARKS);

  EXPECT_CALL(observer,
              OnEncryptedTypesChanged(
                  HasModelTypes(expected_encrypted_types), false));

  EXPECT_FALSE(cryptographer_.encrypt_everything());
  ModelTypeSet encrypted_types = cryptographer_.GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == syncable::PASSWORDS || iter.Get() == syncable::NIGORI)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  cryptographer_.UpdateEncryptedTypesFromNigori(specifics);

  EXPECT_FALSE(cryptographer_.encrypt_everything());
  encrypted_types = cryptographer_.GetEncryptedTypes();
  for (ModelTypeSet::Iterator iter = real_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == syncable::PASSWORDS ||
        iter.Get() == syncable::NIGORI ||
        iter.Get() == syncable::BOOKMARKS)
      EXPECT_TRUE(encrypted_types.Has(iter.Get()));
    else
      EXPECT_FALSE(encrypted_types.Has(iter.Get()));
  }

  cryptographer_.RemoveObserver(&observer);
}

}  // namespace syncer
