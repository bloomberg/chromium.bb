// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/sync_trusted_vault_keys.h"

#include "base/optional.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::NotNull;

MATCHER(HasEmptyValue, "") {
  return arg.encryption_keys().empty() &&
         arg.last_encryption_key_version() == 0 &&
         arg.trusted_public_keys().empty();
}

base::DictionaryValue MakeKeyValueWithoutVersion(
    const std::vector<uint8_t>& key_material) {
  base::DictionaryValue key_value;
  key_value.SetKey("keyMaterial", base::Value(key_material));
  return key_value;
}

base::DictionaryValue MakeKeyValue(const std::vector<uint8_t>& key_material,
                                   int version) {
  base::DictionaryValue key_value = MakeKeyValueWithoutVersion(key_material);
  key_value.SetIntKey("version", version);
  return key_value;
}

}  // namespace

TEST(SyncTrustedVaultKeysTest, DefaultConstructor) {
  EXPECT_THAT(SyncTrustedVaultKeys(), HasEmptyValue());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithEmptyDictionary) {
  EXPECT_THAT(SyncTrustedVaultKeys::FromJs(base::DictionaryValue()),
              HasEmptyValue());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithInvalidDictionary) {
  base::DictionaryValue value;
  value.SetStringKey("foo", "bar");
  EXPECT_THAT(SyncTrustedVaultKeys::FromJs(value), HasEmptyValue());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithEncryptionKeys) {
  const std::vector<uint8_t> kEncryptionKeyMaterial1 = {1, 2, 3, 4};
  const std::vector<uint8_t> kEncryptionKeyMaterial2 = {5, 6, 7, 8};
  const int kEncryptionKeyVersion1 = 17;
  const int kEncryptionKeyVersion2 = 15;

  std::vector<base::Value> key_values;
  key_values.push_back(
      MakeKeyValue(kEncryptionKeyMaterial1, kEncryptionKeyVersion1));
  key_values.push_back(
      MakeKeyValue(kEncryptionKeyMaterial2, kEncryptionKeyVersion2));

  base::DictionaryValue root_value;
  root_value.SetKey("encryptionKeys", base::Value(std::move(key_values)));

  const SyncTrustedVaultKeys actual_converted_keys =
      SyncTrustedVaultKeys::FromJs(root_value);
  EXPECT_THAT(actual_converted_keys.last_encryption_key_version(),
              Eq(kEncryptionKeyVersion2));
  EXPECT_THAT(actual_converted_keys.encryption_keys(),
              ElementsAre(kEncryptionKeyMaterial1, kEncryptionKeyMaterial2));
  EXPECT_THAT(actual_converted_keys.trusted_public_keys(), IsEmpty());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithEncryptionKeysWithMissingVersion) {
  const std::vector<uint8_t> kEncryptionKeyMaterial1 = {1, 2, 3, 4};
  const std::vector<uint8_t> kEncryptionKeyMaterial2 = {5, 6, 7, 8};
  const int kEncryptionKeyVersion1 = 17;

  std::vector<base::Value> key_values;
  key_values.push_back(
      MakeKeyValue(kEncryptionKeyMaterial1, kEncryptionKeyVersion1));
  key_values.push_back(MakeKeyValueWithoutVersion(kEncryptionKeyMaterial2));

  base::DictionaryValue root_value;
  root_value.SetKey("encryptionKeys", base::Value(std::move(key_values)));

  const SyncTrustedVaultKeys actual_converted_keys =
      SyncTrustedVaultKeys::FromJs(root_value);
  EXPECT_THAT(actual_converted_keys.last_encryption_key_version(),
              Eq(kEncryptionKeyVersion1));
  EXPECT_THAT(actual_converted_keys.encryption_keys(),
              ElementsAre(kEncryptionKeyMaterial1));
  EXPECT_THAT(actual_converted_keys.trusted_public_keys(), IsEmpty());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithTrustedPublicKeys) {
  const std::vector<uint8_t> kPublicKeyMaterial1 = {1, 2, 3, 4};
  const std::vector<uint8_t> kPublicKeyMaterial2 = {5, 6, 7, 8};

  std::vector<base::Value> key_values;
  key_values.push_back(MakeKeyValueWithoutVersion(kPublicKeyMaterial1));
  key_values.push_back(MakeKeyValueWithoutVersion(kPublicKeyMaterial2));

  base::DictionaryValue root_value;
  root_value.SetKey("trustedPublicKeys", base::Value(std::move(key_values)));

  const SyncTrustedVaultKeys actual_converted_keys =
      SyncTrustedVaultKeys::FromJs(root_value);
  EXPECT_THAT(actual_converted_keys.last_encryption_key_version(), Eq(0));
  EXPECT_THAT(actual_converted_keys.encryption_keys(), IsEmpty());
  EXPECT_THAT(actual_converted_keys.trusted_public_keys(),
              ElementsAre(kPublicKeyMaterial1, kPublicKeyMaterial2));
}

}  // namespace chromeos
