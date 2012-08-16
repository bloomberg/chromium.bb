// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_sync_encryption_handler.h"

#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/syncable/nigori_util.h"
#include "sync/util/cryptographer.h"

namespace syncer {

FakeSyncEncryptionHandler::FakeSyncEncryptionHandler()
    : encrypted_types_(SensitiveTypes()),
      encrypt_everything_(false),
      explicit_passphrase_(false),
      cryptographer_(NULL) {
}
FakeSyncEncryptionHandler::~FakeSyncEncryptionHandler() {}

void FakeSyncEncryptionHandler::Init() {
  // Do nothing.
}

void FakeSyncEncryptionHandler::ApplyNigoriUpdate(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  if (nigori.encrypt_everything())
    EnableEncryptEverything();
  if (nigori.using_explicit_passphrase())
    explicit_passphrase_ = true;

  if (!cryptographer_)
    return;

  if (cryptographer_->CanDecrypt(nigori.encrypted()))
    cryptographer_->InstallKeys(nigori.encrypted());
  else
    cryptographer_->SetPendingKeys(nigori.encrypted());

  if (cryptographer_->has_pending_keys()) {
    DVLOG(1) << "OnPassPhraseRequired Sent";
    sync_pb::EncryptedData pending_keys = cryptographer_->GetPendingKeys();
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnPassphraseRequired(REASON_DECRYPTION,
                                           pending_keys));
  } else if (!cryptographer_->is_ready()) {
    DVLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
             << "ready";
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnPassphraseRequired(REASON_ENCRYPTION,
                                           sync_pb::EncryptedData()));
  }
}

void FakeSyncEncryptionHandler::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori,
    syncable::BaseTransaction* const trans) const {
  syncable::UpdateNigoriFromEncryptedTypes(encrypted_types_,
                                           encrypt_everything_,
                                           nigori);
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
    explicit_passphrase_ = true;
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
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnEncryptedTypesChanged(encrypted_types_, encrypt_everything_));
}

bool FakeSyncEncryptionHandler::EncryptEverythingEnabled() const {
  return encrypt_everything_;
}

ModelTypeSet FakeSyncEncryptionHandler::GetEncryptedTypes() const {
  return encrypted_types_;
}

bool FakeSyncEncryptionHandler::IsUsingExplicitPassphrase() const {
  return explicit_passphrase_;
}

}  // namespace syncer
