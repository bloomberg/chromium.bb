// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_performer.h"

#include "ash/components/cryptohome/cryptohome_util.h"
#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/cryptohome/userdataauth_util.h"
#include "ash/components/login/auth/cryptohome_key_constants.h"
#include "ash/components/login/auth/cryptohome_parameter_utils.h"
#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

AuthPerformer::AuthPerformer(base::raw_ptr<UserDataAuthClient> client)
    : client_(client) {}

AuthPerformer::~AuthPerformer() = default;

void AuthPerformer::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<AuthPerformer> AuthPerformer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthPerformer::StartAuthSession(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     StartSessionCallback callback) {
  client_->WaitForServiceToBeAvailable(base::BindOnce(
      &AuthPerformer::OnServiceRunning, weak_factory_.GetWeakPtr(),
      std::move(context), ephemeral, std::move(callback)));
}

void AuthPerformer::OnServiceRunning(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     StartSessionCallback callback,
                                     bool service_is_available) {
  if (!service_is_available) {
    // TODO(crbug.com/1262139): Maybe have this error surfaced to UI.
    LOG(FATAL) << "Cryptohome service could not start";
  }
  LOGIN_LOG(EVENT) << "Starting AuthSession";
  user_data_auth::StartAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());

  if (ephemeral) {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER);
  } else {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_NONE);
  }

  client_->StartAuthSession(
      request, base::BindOnce(&AuthPerformer::OnStartAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::AuthenticateUsingKnowledgeKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(context->GetChallengeResponseKeys().empty());
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthPerformer::HashKeyAndAuthenticate, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  // The login code might speculatively set the "gaia" label in the user
  // context, however at the cryptohome level the existing user key's label can
  // be either "gaia" or "legacy-N" - which is what we need to use when talking
  // to cryptohome. If in cryptohome, "gaia" is indeed the label, then at the
  // end of this operation, gaia would be returned. This case applies to only
  // "gaia" labels only because they are created at oobe.
  if (context->GetKey()->GetLabel() == kCryptohomeGaiaKeyLabel ||
      context->GetKey()->GetLabel().empty()) {
    const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
    const cryptohome::KeyDefinition* key_def =
        auth_factors.FindOnlinePasswordKey();
    if (!key_def) {
      LOGIN_LOG(ERROR) << "Could not find Password key";
      std::move(callback).Run(
          std::move(context),
          CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
    context->GetKey()->SetLabel(key_def->label);
  }

  LOGIN_LOG(EVENT) << "Authenticating using key "
                   << context->GetKey()->GetKeyType();

  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(*context), key);

  client_->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::HashKeyAndAuthenticate(std::unique_ptr<UserContext> context,
                                           AuthOperationCallback callback,
                                           const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateUsingChallengeResponseKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetChallengeResponseKeys().empty());
  LOGIN_LOG(EVENT) << "Authenticating using challenge-response";

  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  *request.mutable_authorization() = CreateAuthorizationRequestFromKeyDef(
      cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
          *context));

  client_->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::AuthenticateWithPassword(
    const std::string& key_label,
    const std::string& password,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!password.empty()) << "Caller should check for empty password";
  DCHECK(!key_label.empty()) << "Caller should provide correct label";
  DCHECK(!context->GetAuthSessionId().empty()) << "Auth session should exist";
  const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
  if (!auth_factors.HasPasswordKey(key_label)) {
    LOGIN_LOG(ERROR) << "User does not have password factor labeled "
                     << key_label;
    std::move(callback).Run(
        std::move(context),
        CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }

  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &AuthPerformer::HashPasswordAndAuthenticate, weak_factory_.GetWeakPtr(),
      password, key_label, std::move(context), std::move(callback)));
}

void AuthPerformer::HashPasswordAndAuthenticate(
    const std::string& key_label,
    const std::string& password,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  // Use Key until proper migration to AuthFactors API.
  chromeos::Key password_key(password);
  password_key.SetLabel(key_label);
  password_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  context->SetKey(password_key);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateWithPin(const std::string& pin,
                                        const std::string& pin_salt,
                                        std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  DCHECK(!pin.empty()) << "Caller should check for empty PIN";
  DCHECK(!pin_salt.empty()) << "Client code should provide correct salt";
  DCHECK(!context->GetAuthSessionId().empty()) << "Auth session should exist";
  const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
  const cryptohome::KeyDefinition* key_def = auth_factors.FindPinKey();
  if (!key_def) {
    LOGIN_LOG(ERROR) << "User does not have PIN as factor";
    std::move(callback).Run(
        std::move(context),
        CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  // Use Key until proper migration to AuthFactors API.
  Key key(pin);
  DCHECK_EQ(key_def->label, kCryptohomePinLabel);
  key.SetLabel(key_def->label);

  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, pin_salt);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateAsKiosk(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Authenticating as Kiosk";
  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyData* key_data = key->mutable_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);

  const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
  const cryptohome::KeyDefinition* key_def = auth_factors.FindKioskKey();
  if (!key_def) {
    LOGIN_LOG(ERROR) << "Could not find Kiosk key";
    std::move(callback).Run(
        std::move(context),
        CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  key_data->set_label(key_def->label);

  client_->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

/// ---- private callbacks ----

void AuthPerformer::OnStartAuthSession(
    std::unique_ptr<UserContext> context,
    StartSessionCallback callback,
    absl::optional<user_data_auth::StartAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Could not start authsession " << error;
    std::move(callback).Run(false, std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "AuthSession started, user "
                   << (reply->user_exists() ? "exists" : "does not exist");

  context->SetAuthSessionId(reply->auth_session_id());
  // Remember key metadata
  std::vector<cryptohome::KeyDefinition> key_definitions;
  for (const auto& [label, key_data] : reply->key_label_data()) {
    // Backfill kiosk key type
    // TODO(crbug.com/1310312): Find if there is any better way.
    cryptohome::KeyData data(key_data);
    if (!data.has_type()) {
      LOGIN_LOG(DEBUG) << "Backfilling Kiosk key type for key " << label;
      data.set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
    }
    // "legacy-0" keys exist as label in map, but might not exist as labels
    // in KeyData.
    if (!data.has_label() || data.label().empty()) {
      data.set_label(label);
    }
    key_definitions.push_back(KeyDataToKeyDefinition(data));
  }
  AuthFactorsData auth_factors_data(std::move(key_definitions));
  context->SetAuthFactorsData(std::move(auth_factors_data));

  std::move(callback).Run(reply->user_exists(), std::move(context),
                          absl::nullopt);
}

void AuthPerformer::OnAuthenticateAuthSession(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT) << "Failed to authenticate session, error code " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  DCHECK(reply->authenticated());
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
