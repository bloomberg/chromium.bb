// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_encryption_handler.h"

#include "components/sync/base/passphrase_enums.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/syncable/nigori_util.h"

namespace syncer {

FakeSyncEncryptionHandler::FakeSyncEncryptionHandler()
    : encrypted_types_(SensitiveTypes()),
      encrypt_everything_(false),
      passphrase_type_(PassphraseType::IMPLICIT_PASSPHRASE),
      cryptographer_(&encryptor_) {}
FakeSyncEncryptionHandler::~FakeSyncEncryptionHandler() {}

void FakeSyncEncryptionHandler::Init() {
  // Set up a basic cryptographer.
  KeyParams keystore_params = {KeyDerivationParams::CreateForPbkdf2(),
                               "keystore_key"};
  cryptographer_.AddKey(keystore_params);
}

void FakeSyncEncryptionHandler::ApplyNigoriUpdate(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  if (nigori.encrypt_everything())
    EnableEncryptEverything();
  if (nigori.keybag_is_frozen())
    passphrase_type_ = PassphraseType::CUSTOM_PASSPHRASE;

  // TODO(zea): consider adding fake support for migration.
  if (cryptographer_.CanDecrypt(nigori.encryption_keybag()))
    cryptographer_.InstallKeys(nigori.encryption_keybag());
  else if (nigori.has_encryption_keybag())
    cryptographer_.SetPendingKeys(nigori.encryption_keybag());

  if (cryptographer_.has_pending_keys()) {
    DVLOG(1) << "OnPassPhraseRequired Sent";
    sync_pb::EncryptedData pending_keys = cryptographer_.GetPendingKeys();
    for (auto& observer : observers_)
      observer.OnPassphraseRequired(REASON_DECRYPTION,
                                    KeyDerivationParams::CreateForPbkdf2(),
                                    pending_keys);
  } else if (!cryptographer_.is_ready()) {
    DVLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
             << "ready";
    for (auto& observer : observers_) {
      observer.OnPassphraseRequired(REASON_ENCRYPTION,
                                    KeyDerivationParams::CreateForPbkdf2(),
                                    sync_pb::EncryptedData());
    }
  }
}

void FakeSyncEncryptionHandler::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori,
    syncable::BaseTransaction* const trans) const {
  syncable::UpdateNigoriFromEncryptedTypes(encrypted_types_,
                                           encrypt_everything_, nigori);
}

bool FakeSyncEncryptionHandler::NeedKeystoreKey(
    syncable::BaseTransaction* const trans) const {
  return keystore_key_.empty();
}

bool FakeSyncEncryptionHandler::SetKeystoreKeys(
    const google::protobuf::RepeatedPtrField<google::protobuf::string>& keys,
    syncable::BaseTransaction* const trans) {
  if (keys.size() == 0)
    return false;
  std::string new_key = keys.Get(keys.size() - 1);
  if (new_key.empty())
    return false;
  keystore_key_ = new_key;

  DVLOG(1) << "Keystore bootstrap token updated.";
  for (auto& observer : observers_)
    observer.OnBootstrapTokenUpdated(keystore_key_, KEYSTORE_BOOTSTRAP_TOKEN);

  return true;
}

ModelTypeSet FakeSyncEncryptionHandler::GetEncryptedTypes(
    syncable::BaseTransaction* const trans) const {
  return encrypted_types_;
}

void FakeSyncEncryptionHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncEncryptionHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeSyncEncryptionHandler::SetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {
  if (is_explicit)
    passphrase_type_ = PassphraseType::CUSTOM_PASSPHRASE;
}

void FakeSyncEncryptionHandler::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::EnableEncryptEverything() {
  if (encrypt_everything_)
    return;
  encrypt_everything_ = true;
  encrypted_types_ = ModelTypeSet::All();
  for (auto& observer : observers_)
    observer.OnEncryptedTypesChanged(encrypted_types_, encrypt_everything_);
}

bool FakeSyncEncryptionHandler::IsEncryptEverythingEnabled() const {
  return encrypt_everything_;
}

PassphraseType FakeSyncEncryptionHandler::GetPassphraseType(
    syncable::BaseTransaction* const trans) const {
  return passphrase_type_;
}

}  // namespace syncer
