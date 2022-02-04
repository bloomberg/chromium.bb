// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_SYNC_TRUSTED_VAULT_KEYS_H_
#define ASH_COMPONENTS_LOGIN_AUTH_SYNC_TRUSTED_VAULT_KEYS_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace ash {

// Struct which holds keys about a user's encryption keys during signin flow.
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) SyncTrustedVaultKeys {
 public:
  SyncTrustedVaultKeys();
  SyncTrustedVaultKeys(const SyncTrustedVaultKeys&);
  SyncTrustedVaultKeys(SyncTrustedVaultKeys&&);
  SyncTrustedVaultKeys& operator=(const SyncTrustedVaultKeys&);
  SyncTrustedVaultKeys& operator=(SyncTrustedVaultKeys&&);
  ~SyncTrustedVaultKeys();

  // Initialize an instance of this class with data received from javascript.
  // The input data must be of type SyncTrustedVaultKeys as defined in
  // authenticator.js.
  static SyncTrustedVaultKeys FromJs(const base::DictionaryValue& js_object);

  const std::string& gaia_id() const;

  const std::vector<std::vector<uint8_t>>& encryption_keys() const;
  int last_encryption_key_version() const;

  struct TrustedRecoveryMethod {
    TrustedRecoveryMethod();
    TrustedRecoveryMethod(const TrustedRecoveryMethod&);
    TrustedRecoveryMethod& operator=(const TrustedRecoveryMethod&);
    ~TrustedRecoveryMethod();

    std::vector<uint8_t> public_key;
    int type_hint = 0;
  };
  const std::vector<TrustedRecoveryMethod>& trusted_recovery_methods() const;

 private:
  std::string gaia_id_;
  std::vector<std::vector<uint8_t>> encryption_keys_;
  int last_encryption_key_version_ = 0;
  std::vector<TrustedRecoveryMethod> trusted_recovery_methods_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::SyncTrustedVaultKeys;
}  // namespace chromeos

#endif  // ASH_COMPONENTS_LOGIN_AUTH_SYNC_TRUSTED_VAULT_KEYS_H_
