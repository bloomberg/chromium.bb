// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_parameters.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/notreached.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

using chromeos::ChallengeResponseKey;

namespace cryptohome {
namespace {

const std::string GetCryptohomeId(const AccountId& account_id) {
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE: {
      return account_id.GetUserEmail();
    }
    case AccountType::ACTIVE_DIRECTORY: {
      // Always use the account id key, authpolicyd relies on it!
      return account_id.GetAccountIdKey();
    }
    case AccountType::UNKNOWN: {
      return account_id.GetUserEmail();
    }
  }

  NOTREACHED();
  return account_id.GetUserEmail();
}

AccountId LookupUserByCryptohomeId(const std::string& cryptohome_id) {
  const std::vector<AccountId> known_account_ids =
      user_manager::known_user::GetKnownAccountIds();

  // A LOT of tests start with --login_user <user>, and not registering this
  // user before. So we might have "known_user" entry without gaia_id.
  for (const AccountId& known_id : known_account_ids) {
    if (known_id.HasAccountIdKey() &&
        known_id.GetAccountIdKey() == cryptohome_id) {
      return known_id;
    }
  }

  for (const AccountId& known_id : known_account_ids) {
    if (known_id.GetUserEmail() == cryptohome_id) {
      return known_id;
    }
  }

  return user_manager::known_user::GetAccountId(
      cryptohome_id, std::string() /* id */, AccountType::UNKNOWN);
}

}  //  anonymous namespace

Identification::Identification() = default;

Identification::Identification(const AccountId& account_id)
    : id_(GetCryptohomeId(account_id)) {}

Identification::Identification(const std::string& id) : id_(id) {}

Identification Identification::FromString(const std::string& id) {
  return Identification(id);
}

bool Identification::operator==(const Identification& other) const {
  return id_ == other.id_;
}

bool Identification::operator<(const Identification& right) const {
  return id_ < right.id_;
}

AccountId Identification::GetAccountId() const {
  return LookupUserByCryptohomeId(id_);
}

AccountIdentifier CreateAccountIdentifierFromAccountId(const AccountId& id) {
  AccountIdentifier out;
  out.set_account_id(GetCryptohomeId(id));
  return out;
}

AccountIdentifier CreateAccountIdentifierFromIdentification(
    const Identification& id) {
  AccountIdentifier out;
  out.set_account_id(id.id());
  return out;
}

AccountId GetAccountIdFromAccountIdentifier(
    const AccountIdentifier& account_identifier) {
  return LookupUserByCryptohomeId(account_identifier.account_id());
}

KeyDefinition::ProviderData::ProviderData() = default;

KeyDefinition::ProviderData::ProviderData(const std::string& name)
    : name(name) {
}

KeyDefinition::ProviderData::ProviderData(const ProviderData& other)
    : name(other.name) {
  if (other.number)
    number = std::make_unique<int64_t>(*other.number);
  if (other.bytes)
    bytes = std::make_unique<std::string>(*other.bytes);
}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          int64_t number)
    : name(name), number(new int64_t(number)) {}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          const std::string& bytes)
    : name(name),
      bytes(new std::string(bytes)) {
}

void KeyDefinition::ProviderData::operator=(const ProviderData& other) {
  name = other.name;
  number.reset(other.number ? new int64_t(*other.number) : NULL);
  bytes.reset(other.bytes ? new std::string(*other.bytes) : NULL);
}

KeyDefinition::ProviderData::~ProviderData() = default;

bool KeyDefinition::ProviderData::operator==(const ProviderData& other) const {
  const bool has_number = number != nullptr;
  const bool other_has_number = other.number != nullptr;
  const bool has_bytes = bytes != nullptr;
  const bool other_has_bytes = other.bytes != nullptr;
  return name == other.name &&
         has_number == other_has_number &&
         has_bytes == other_has_bytes &&
         (!has_number || (*number == *other.number)) &&
         (!has_bytes || (*bytes == *other.bytes));
}

bool KeyDefinition::Policy::operator==(const Policy& other) const {
  return low_entropy_credential == other.low_entropy_credential &&
         auth_locked == other.auth_locked;
}

bool KeyDefinition::Policy::operator!=(const Policy& other) const {
  return !(*this == other);
}

KeyDefinition KeyDefinition::CreateForPassword(
    const std::string& secret,
    const std::string& label,
    int /*AuthKeyPrivileges*/ privileges) {
  KeyDefinition key_def;
  key_def.type = TYPE_PASSWORD;
  key_def.label = label;
  key_def.privileges = privileges;
  key_def.secret = secret;
  return key_def;
}

KeyDefinition KeyDefinition::CreateForChallengeResponse(
    const std::vector<ChallengeResponseKey>& challenge_response_keys,
    const std::string& label,
    int /*AuthKeyPrivileges*/ privileges) {
  KeyDefinition key_def;
  key_def.type = TYPE_CHALLENGE_RESPONSE;
  key_def.label = label;
  key_def.privileges = privileges;
  key_def.challenge_response_keys = challenge_response_keys;
  return key_def;
}

KeyDefinition::KeyDefinition() = default;

KeyDefinition::KeyDefinition(const KeyDefinition& other) = default;

KeyDefinition::~KeyDefinition() = default;

bool KeyDefinition::operator==(const KeyDefinition& other) const {
  if (type != other.type || label != other.label ||
      privileges != other.privileges || policy != other.policy ||
      revision != other.revision ||
      challenge_response_keys != other.challenge_response_keys ||
      provider_data.size() != other.provider_data.size()) {
    return false;
  }

  for (size_t i = 0; i < provider_data.size(); ++i) {
    if (!(provider_data[i] == other.provider_data[i]))
      return false;
  }
  return true;
}

Authorization::Authorization(const std::string& key, const std::string& label)
    : key(key),
      label(label) {
}

Authorization::Authorization(const KeyDefinition& key_def)
    : key(key_def.secret),
      label(key_def.label) {
}

bool Authorization::operator==(const Authorization& other) const {
  return key == other.key && label == other.label;
}

}  // namespace cryptohome
