// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/cryptographer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/test/fake_encryptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;

}  // namespace

class CryptographerTest : public ::testing::Test {
 protected:
  CryptographerTest() : cryptographer_(&encryptor_) {}

  FakeEncryptor encryptor_;
  Cryptographer cryptographer_;
};

TEST_F(CryptographerTest, EmptyCantDecrypt) {
  EXPECT_FALSE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer_.CanDecrypt(encrypted));
}

TEST_F(CryptographerTest, EmptyCantEncrypt) {
  EXPECT_FALSE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted;
  sync_pb::PasswordSpecificsData original;
  EXPECT_FALSE(cryptographer_.Encrypt(original, &encrypted));
}

TEST_F(CryptographerTest, MissingCantDecrypt) {
  KeyParams params = {"localhost", "dummy", "dummy"};
  cryptographer_.AddKey(params);
  EXPECT_TRUE(cryptographer_.is_ready());

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name("foo");
  encrypted.set_blob("bar");

  EXPECT_FALSE(cryptographer_.CanDecrypt(encrypted));
}

TEST_F(CryptographerTest, CanEncryptAndDecrypt) {
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

TEST_F(CryptographerTest, EncryptOnlyIfDifferent) {
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

TEST_F(CryptographerTest, AddKeySetsDefault) {
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
TEST_F(CryptographerTest, MAYBE_EncryptExportDecrypt) {
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

TEST_F(CryptographerTest, Bootstrap) {
  KeyParams params = {"localhost", "dummy", "dummy"};
  cryptographer_.AddKey(params);

  std::string token;
  EXPECT_TRUE(cryptographer_.GetBootstrapToken(&token));
  EXPECT_TRUE(base::IsStringUTF8(token));

  Cryptographer other_cryptographer(&encryptor_);
  other_cryptographer.Bootstrap(token);
  EXPECT_TRUE(other_cryptographer.is_ready());

  const char secret[] = "secret";
  sync_pb::EncryptedData encrypted;
  EXPECT_TRUE(other_cryptographer.EncryptString(secret, &encrypted));
  EXPECT_TRUE(cryptographer_.CanDecryptUsingDefaultKey(encrypted));
}

}  // namespace syncer
