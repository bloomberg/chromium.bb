// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/sync_trusted_vault_keys.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// Keys in base::Value dictionaries.
const char kEncryptionKeysDictKey[] = "encryptionKeys";
const char kGaiaIdDictKey[] = "obfuscatedGaiaId";
const char kKeyMaterialDictKey[] = "keyMaterial";
const char kMethodTypeHintDictKey[] = "type";
const char kPublicKeyDictKey[] = "publicKey";
const char kTrustedRecoveryMethodsDictKey[] = "trustedRecoveryMethods";
const char kVersionDictKey[] = "version";

struct KeyMaterialAndVersion {
  std::vector<uint8_t> key_material;
  int version = 0;
};

absl::optional<KeyMaterialAndVersion> ParseSingleEncryptionKey(
    const base::Value& js_object) {
  const base::Value::BlobStorage* key_material =
      js_object.FindBlobKey(kKeyMaterialDictKey);
  if (key_material == nullptr) {
    return absl::nullopt;
  }

  return KeyMaterialAndVersion{
      *key_material, js_object.FindIntKey(kVersionDictKey).value_or(0)};
}

absl::optional<SyncTrustedVaultKeys::TrustedRecoveryMethod>
ParseSingleTrustedRecoveryMethod(const base::Value& js_object) {
  const base::Value::BlobStorage* public_key =
      js_object.FindBlobKey(kPublicKeyDictKey);
  if (public_key == nullptr) {
    return absl::nullopt;
  }

  SyncTrustedVaultKeys::TrustedRecoveryMethod method;
  method.public_key = *public_key;
  method.type_hint = js_object.FindIntKey(kMethodTypeHintDictKey).value_or(0);
  return method;
}

template <typename T>
std::vector<T> ParseList(
    const base::Value* list,
    const base::RepeatingCallback<absl::optional<T>(const base::Value&)>&
        entry_parser) {
  if (list == nullptr || !list->is_list()) {
    return {};
  }

  std::vector<T> parsed_list;
  for (const base::Value& list_entry : list->GetListDeprecated()) {
    absl::optional<T> parsed_entry = entry_parser.Run(list_entry);
    if (parsed_entry.has_value()) {
      parsed_list.push_back(std::move(*parsed_entry));
    }
  }

  return parsed_list;
}

}  // namespace

SyncTrustedVaultKeys::TrustedRecoveryMethod::TrustedRecoveryMethod() = default;

SyncTrustedVaultKeys::TrustedRecoveryMethod::TrustedRecoveryMethod(
    const TrustedRecoveryMethod&) = default;

SyncTrustedVaultKeys::TrustedRecoveryMethod&
SyncTrustedVaultKeys::TrustedRecoveryMethod::operator=(
    const TrustedRecoveryMethod&) = default;

SyncTrustedVaultKeys::TrustedRecoveryMethod::~TrustedRecoveryMethod() = default;

SyncTrustedVaultKeys::SyncTrustedVaultKeys() = default;

SyncTrustedVaultKeys::SyncTrustedVaultKeys(const SyncTrustedVaultKeys&) =
    default;

SyncTrustedVaultKeys::SyncTrustedVaultKeys(SyncTrustedVaultKeys&&) = default;

SyncTrustedVaultKeys& SyncTrustedVaultKeys::operator=(
    const SyncTrustedVaultKeys&) = default;

SyncTrustedVaultKeys& SyncTrustedVaultKeys::operator=(SyncTrustedVaultKeys&&) =
    default;

SyncTrustedVaultKeys::~SyncTrustedVaultKeys() = default;

// static
SyncTrustedVaultKeys SyncTrustedVaultKeys::FromJs(
    const base::DictionaryValue& js_object) {
  SyncTrustedVaultKeys result;
  const std::string* gaia_id = js_object.FindStringKey(kGaiaIdDictKey);
  if (gaia_id) {
    result.gaia_id_ = *gaia_id;
  }

  const std::vector<KeyMaterialAndVersion> encryption_keys =
      ParseList(js_object.FindListKey(kEncryptionKeysDictKey),
                base::BindRepeating(&ParseSingleEncryptionKey));

  for (const KeyMaterialAndVersion& key : encryption_keys) {
    if (key.version != 0) {
      result.encryption_keys_.push_back(key.key_material);
      result.last_encryption_key_version_ = key.version;
    }
  }

  result.trusted_recovery_methods_ =
      ParseList(js_object.FindListKey(kTrustedRecoveryMethodsDictKey),
                base::BindRepeating(&ParseSingleTrustedRecoveryMethod));

  return result;
}

const std::string& SyncTrustedVaultKeys::gaia_id() const {
  return gaia_id_;
}

const std::vector<std::vector<uint8_t>>& SyncTrustedVaultKeys::encryption_keys()
    const {
  return encryption_keys_;
}

int SyncTrustedVaultKeys::last_encryption_key_version() const {
  return last_encryption_key_version_;
}

const std::vector<SyncTrustedVaultKeys::TrustedRecoveryMethod>&
SyncTrustedVaultKeys::trusted_recovery_methods() const {
  return trusted_recovery_methods_;
}

}  // namespace ash
