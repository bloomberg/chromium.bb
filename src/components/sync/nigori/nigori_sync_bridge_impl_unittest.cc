// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/time.h"
#include "components/sync/model/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::NotNull;

const char kNigoriKeyName[] = "nigori-key";

MATCHER(NotNullTime, "") {
  return !arg.is_null();
}

MATCHER_P(HasDefaultKeyDerivedFrom, key_params, "") {
  const Cryptographer& cryptographer = arg;
  std::unique_ptr<Nigori> expected_default_nigori = Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string expected_default_key_name;
  EXPECT_TRUE(expected_default_nigori->Permute(
      Nigori::Type::Password, kNigoriKeyName, &expected_default_key_name));
  return cryptographer.GetDefaultNigoriKeyName() == expected_default_key_name;
}

MATCHER(HasKeystoreNigori, "") {
  const std::unique_ptr<EntityData>& entity_data = arg;
  if (!entity_data || !entity_data->specifics.has_nigori()) {
    return false;
  }
  const sync_pb::NigoriSpecifics& specifics = entity_data->specifics.nigori();
  if (specifics.passphrase_type() !=
      sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    return false;
  }
  return !specifics.encryption_keybag().blob().empty() &&
         !specifics.keystore_decryptor_token().blob().empty() &&
         specifics.keybag_is_frozen() &&
         specifics.has_keystore_migration_time();
}

MATCHER_P(CanDecryptWith, key_params, "") {
  const Cryptographer& cryptographer = arg;
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string nigori_name;
  EXPECT_TRUE(
      nigori->Permute(Nigori::Type::Password, kNigoriKeyName, &nigori_name));
  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori_name);
  EXPECT_TRUE(nigori->Encrypt(unencrypted, encrypted.mutable_blob()));

  if (!cryptographer.CanDecrypt(encrypted)) {
    return false;
  }
  std::string decrypted;
  if (!cryptographer.DecryptToString(encrypted, &decrypted)) {
    return false;
  }
  return decrypted == unencrypted;
}

MATCHER_P(EncryptedDataEq, expected, "") {
  const sync_pb::EncryptedData& given = arg;
  return given.key_name() == expected.key_name() &&
         given.blob() == expected.blob();
}

KeyParams Pbkdf2KeyParams(std::string key) {
  return {KeyDerivationParams::CreateForPbkdf2(), std::move(key)};
}

KeyParams KeystoreKeyParams(const std::string& key) {
  // Due to mis-encode of keystore keys to base64 we have to always encode such
  // keys to provide backward compatibility.
  std::string encoded_key;
  base::Base64Encode(key, &encoded_key);
  return Pbkdf2KeyParams(std::move(encoded_key));
}

class MockNigoriLocalChangeProcessor : public NigoriLocalChangeProcessor {
 public:
  MockNigoriLocalChangeProcessor() = default;
  ~MockNigoriLocalChangeProcessor() = default;

  MOCK_METHOD2(ModelReadyToSync, void(NigoriSyncBridge*, NigoriMetadataBatch));
  MOCK_METHOD1(Put, void(std::unique_ptr<EntityData>));
  MOCK_METHOD0(GetMetadata, NigoriMetadataBatch());
  MOCK_METHOD1(ReportError, void(const ModelError&));
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<ModelTypeControllerDelegate>());
};

class MockObserver : public SyncEncryptionHandler::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() = default;

  MOCK_METHOD3(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const KeyDerivationParams&,
                    const sync_pb::EncryptedData&));
  MOCK_METHOD0(OnPassphraseAccepted, void());
  MOCK_METHOD2(OnBootstrapTokenUpdated,
               void(const std::string&, BootstrapTokenType type));
  MOCK_METHOD2(OnEncryptedTypesChanged, void(ModelTypeSet, bool));
  MOCK_METHOD0(OnEncryptionComplete, void());
  MOCK_METHOD1(OnCryptographerStateChanged, void(Cryptographer*));
  MOCK_METHOD2(OnPassphraseTypeChanged, void(PassphraseType, base::Time));
  MOCK_METHOD1(OnLocalSetPassphraseEncryption,
               void(const SyncEncryptionHandler::NigoriState&));
};

class NigoriSyncBridgeImplTest : public testing::Test {
 protected:
  NigoriSyncBridgeImplTest() {
    auto processor =
        std::make_unique<testing::NiceMock<MockNigoriLocalChangeProcessor>>();
    processor_ = processor.get();
    bridge_ = std::make_unique<NigoriSyncBridgeImpl>(std::move(processor),
                                                     &encryptor_);
    bridge_->AddObserver(&observer_);
  }

  ~NigoriSyncBridgeImplTest() override { bridge_->RemoveObserver(&observer_); }

  NigoriSyncBridgeImpl* bridge() { return bridge_.get(); }
  MockNigoriLocalChangeProcessor* processor() { return processor_; }
  MockObserver* observer() { return &observer_; }

  // Builds NigoriSpecifics with following fields:
  // 1. encryption_keybag contains all keys derived from |keybag_keys_params|
  // and encrypted with a key derived from |keybag_decryptor_params|.
  // keystore_decryptor_token is always saved in encryption_keybag, even if it
  // is not derived from any params in |keybag_keys_params|.
  // 2. keystore_decryptor_token contains the key derived from
  // |keybag_decryptor_params| and encrypted with a key derived from
  // |keystore_key_params|.
  // 3. passphrase_type is KEYSTORE_PASSHPRASE.
  // 4. Other fields are default.
  sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
      const std::vector<KeyParams>& keybag_keys_params,
      const KeyParams& keystore_decryptor_params,
      const KeyParams& keystore_key_params) {
    sync_pb::NigoriSpecifics specifics;

    Cryptographer cryptographer(&encryptor_);
    cryptographer.AddKey(keystore_decryptor_params);
    for (const KeyParams& key_params : keybag_keys_params) {
      cryptographer.AddNonDefaultKey(key_params);
    }
    EXPECT_TRUE(cryptographer.GetKeys(specifics.mutable_encryption_keybag()));

    std::string serialized_keystore_decryptor =
        cryptographer.GetDefaultNigoriKeyData();
    Cryptographer keystore_cryptographer(&encryptor_);
    keystore_cryptographer.AddKey(keystore_key_params);
    EXPECT_TRUE(keystore_cryptographer.EncryptString(
        serialized_keystore_decryptor,
        specifics.mutable_keystore_decryptor_token()));

    specifics.set_passphrase_type(
        sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    return specifics;
  }

  // Builds NigoriSpecifics with following fields:
  // 1. encryption_keybag contains keys derived from |passphrase_key_params|
  // and |*old_key_params| (if |old_key_params| isn't nullopt). Encrypted with
  // key derived from |passphrase_key_params|.
  // 2. custom_passphrase_time is current time.
  // 3. passphrase_type is CUSTOM_PASSPHRASE.
  // 4. encrypt_everything is true.
  // 5. Other fields are default.
  sync_pb::NigoriSpecifics BuildCustomPassphraseNigoriSpecifics(
      const KeyParams& passphrase_key_params,
      const base::Optional<KeyParams>& old_key_params = base::nullopt) {
    sync_pb::NigoriSpecifics specifics;

    Cryptographer cryptographer(&encryptor_);
    cryptographer.AddKey(passphrase_key_params);
    if (old_key_params) {
      cryptographer.AddNonDefaultKey(*old_key_params);
    }
    EXPECT_TRUE(cryptographer.GetKeys(specifics.mutable_encryption_keybag()));

    specifics.set_custom_passphrase_time(TimeToProtoTime(base::Time::Now()));
    specifics.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    specifics.set_encrypt_everything(true);

    return specifics;
  }

 private:
  // Don't change the order. |encryptor_| should outlive |bridge_|.
  FakeEncryptor encryptor_;
  std::unique_ptr<NigoriSyncBridgeImpl> bridge_;
  // Ownership transferred to |bridge_|.
  testing::NiceMock<MockNigoriLocalChangeProcessor>* processor_;
  testing::NiceMock<MockObserver> observer_;
};

// Simplest case of keystore Nigori: we have only one keystore key and no old
// keys. This keystore key is encrypted in both encryption_keybag and
// keystore_decryptor_token. Client receives such Nigori if initialization of
// Nigori node was done after keystore was introduced and no key rotations
// happened.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptKeysFromKeystoreNigoriAndNotifyObservers) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(NotNull()));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
}

// Tests that client can properly process remote updates with rotated keystore
// nigori. Cryptographer should be able to decrypt any data encrypted with any
// keystore key and use current keystore key as default key.
TEST_F(NigoriSyncBridgeImplTest, ShouldAcceptKeysFromRotatedKeystoreNigori) {
  const std::string kRawOldKey = "raw_old_keystore_key";
  const KeyParams kOldKeyParams = KeystoreKeyParams(kRawOldKey);
  const std::string kRawCurrentKey = "raw_keystore_key";
  const KeyParams kCurrentKeyParams = KeystoreKeyParams(kRawCurrentKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kOldKeyParams, kCurrentKeyParams},
      /*keystore_decryptor_params=*/kCurrentKeyParams,
      /*keystore_key_params=*/kCurrentKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawOldKey, kRawCurrentKey}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kOldKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(kCurrentKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kCurrentKeyParams));
}

// In the backward compatible mode keystore Nigori's keystore_decryptor_token
// isn't a kestore key, however keystore_decryptor_token itself should be
// encrypted with the keystore key.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptKeysFromBackwardCompatibleKeystoreNigori) {
  const KeyParams kGaiaKeyParams = Pbkdf2KeyParams("gaia_key");
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kGaiaKeyParams, kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kGaiaKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kGaiaKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kGaiaKeyParams));
}

// Tests that we can successfully use old keys from encryption_keybag in
// backward compatible mode.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldAcceptOldKeysFromBackwardCompatibleKeystoreNigori) {
  // |kOldKeyParams| is needed to ensure we was able to decrypt
  // encryption_keybag - there is no way to add key derived from
  // |kOldKeyParams| to cryptographer without decrypting encryption_keybag.
  const KeyParams kOldKeyParams = Pbkdf2KeyParams("old_key");
  const KeyParams kCurrentKeyParams = Pbkdf2KeyParams("current_key");
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);
  const std::vector<KeyParams> kAllKeyParams = {
      kOldKeyParams, kCurrentKeyParams, kKeystoreKeyParams};
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/kAllKeyParams,
      /*keystore_decryptor_params=*/kCurrentKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  for (const KeyParams& key_params : kAllKeyParams) {
    EXPECT_THAT(cryptographer, CanDecryptWith(key_params));
  }
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kCurrentKeyParams));
}

// Tests that we build keystore Nigori, put it to processor, initialize the
// cryptographer and expose a valid entity through GetData(), when the default
// Nigori is received.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldPutAndMakeCryptographerReadyOnDefaultNigori) {
  const std::string kRawKeystoreKey = "raw_keystore_key";
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(kRawKeystoreKey);

  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  EXPECT_TRUE(bridge()->SetKeystoreKeys({kRawKeystoreKey}));

  // We don't verify entire NigoriSpecifics here, because it requires too
  // complex matcher (NigoriSpecifics is not determenistic).
  EXPECT_CALL(*processor(), Put(HasKeystoreNigori()));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(default_entity_data)),
              Eq(base::nullopt));
  EXPECT_THAT(bridge()->GetData(), HasKeystoreNigori());

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kKeystoreKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kKeystoreKeyParams));
  // TODO(crbug.com/922900): verify that passphrase type is equal to
  // KeystorePassphrase once passphrase type support is implemented.
}

// Tests that we can perform initial sync with custom passphrase Nigori.
// We should notify observers about encryption state changes and cryptographer
// shouldn't be ready (by having pending keys) until user provides the
// passphrase.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldNotifyWhenSyncedWithCustomPassphraseNigori) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(Pbkdf2KeyParams("passphrase"));

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(NotNull()));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::CUSTOM_PASSPHRASE,
                                      NotNullTime()));
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->GetCryptographerForTesting().has_pending_keys());
}

// Tests that we can process remote update with custom passphrase Nigori, while
// we already have keystore Nigori locally.
// We should notify observers about encryption state changes and cryptographer
// shouldn't be ready (by having pending keys) until user provides the
// passphrase.
TEST_F(NigoriSyncBridgeImplTest, ShouldTransitToCustomPassphrase) {
  EntityData default_entity_data;
  *default_entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));
  // Note: passing default Nigori to MergeSyncData() leads to instantiation of
  // keystore Nigori.
  ASSERT_THAT(bridge()->MergeSyncData(std::move(default_entity_data)),
              Eq(base::nullopt));

  EntityData new_entity_data;
  *new_entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(Pbkdf2KeyParams("passphrase"));

  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
                               /*encrypted_types=*/EncryptableUserTypes(),
                               /*encrypt_everything=*/true));
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(NotNull()));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(PassphraseType::CUSTOM_PASSPHRASE,
                                      NotNullTime()));
  EXPECT_THAT(bridge()->ApplySyncChanges(std::move(new_entity_data)),
              Eq(base::nullopt));
  EXPECT_TRUE(bridge()->GetCryptographerForTesting().has_pending_keys());
}

// Tests that we don't try to overwrite default passphrase type and report
// ModelError unless we received default Nigori node (which is determined by
// the size of encryption_keybag). It's a requirement because receiving default
// passphrase type might mean that some newer client switched to the new
// passphrase type.
TEST_F(NigoriSyncBridgeImplTest, ShouldFailOnUnknownPassprase) {
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      sync_pb::NigoriSpecifics::default_instance();
  entity_data.specifics.mutable_nigori()->mutable_encryption_keybag()->set_blob(
      "data");
  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  EXPECT_CALL(*processor(), Put(_)).Times(0);
  EXPECT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Ne(base::nullopt));
}

// Tests decryption logic for explicit passphrase. In order to check that we're
// able to decrypt the data encrypted with old key (i.e. keystore keys or old
// GAIA passphrase) we add one extra key to the encryption keybag.
TEST_F(NigoriSyncBridgeImplTest,
       ShouldDecryptWithCustomPassphraseAndUpdateDefaultKey) {
  const KeyParams kOldKeyParams = Pbkdf2KeyParams("old_key");
  const KeyParams kPassphraseKeyParams = Pbkdf2KeyParams("passphrase");
  EntityData entity_data;
  *entity_data.specifics.mutable_nigori() =
      BuildCustomPassphraseNigoriSpecifics(kPassphraseKeyParams, kOldKeyParams);

  ASSERT_TRUE(bridge()->SetKeystoreKeys({"keystore_key"}));

  EXPECT_CALL(
      *observer(),
      OnPassphraseRequired(
          /*reason=*/REASON_DECRYPTION,
          /*key_derivation_params=*/KeyDerivationParams::CreateForPbkdf2(),
          /*pending_keys=*/
          EncryptedDataEq(entity_data.specifics.nigori().encryption_keybag())));
  ASSERT_THAT(bridge()->MergeSyncData(std::move(entity_data)),
              Eq(base::nullopt));

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(NotNull()));
  bridge()->SetDecryptionPassphrase(kPassphraseKeyParams.password);

  const Cryptographer& cryptographer = bridge()->GetCryptographerForTesting();
  EXPECT_THAT(cryptographer, CanDecryptWith(kOldKeyParams));
  EXPECT_THAT(cryptographer, CanDecryptWith(kPassphraseKeyParams));
  EXPECT_THAT(cryptographer, HasDefaultKeyDerivedFrom(kPassphraseKeyParams));
}

}  // namespace

}  // namespace syncer
