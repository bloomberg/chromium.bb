// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/version_info/channel.h"
#include "url/origin.h"

namespace password_manager {

bool PasswordManagerClient::IsSavingAndFillingEnabled(const GURL& url) const {
  return true;
}

bool PasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  return true;
}

bool PasswordManagerClient::IsFillingFallbackEnabled(const GURL& url) const {
  return true;
}

void PasswordManagerClient::ShowTouchToFill(PasswordManagerDriver* driver) {}

void PasswordManagerClient::OnPasswordSelected(const std::u16string& text) {}

scoped_refptr<device_reauth::BiometricAuthenticator>
PasswordManagerClient::GetBiometricAuthenticator() {
  return nullptr;
}

void PasswordManagerClient::GeneratePassword(
    autofill::password_generation::PasswordGenerationType type) {}

void PasswordManagerClient::UpdateCredentialCache(
    const url::Origin& origin,
    const std::vector<const PasswordForm*>& best_matches,
    bool is_blocklisted) {}

void PasswordManagerClient::PasswordWasAutofilled(
    const std::vector<const PasswordForm*>& best_matches,
    const url::Origin& origin,
    const std::vector<const PasswordForm*>* federated_matches) {}

void PasswordManagerClient::AutofillHttpAuth(
    const PasswordForm& preferred_match,
    const PasswordFormManagerForUI* form_manager) {}

void PasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username) {}

void PasswordManagerClient::TriggerReauthForPrimaryAccount(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
  std::move(reauth_callback).Run(ReauthSucceeded(false));
}

void PasswordManagerClient::TriggerSignIn(signin_metrics::AccessPoint) {}

SyncState PasswordManagerClient::GetPasswordSyncState() const {
  return SyncState::kNotSyncing;
}

bool PasswordManagerClient::WasLastNavigationHTTPError() const {
  return false;
}

net::CertStatus PasswordManagerClient::GetMainFrameCertStatus() const {
  return 0;
}

void PasswordManagerClient::PromptUserToEnableAutosignin() {}

bool PasswordManagerClient::IsIncognito() const {
  return false;
}

profile_metrics::BrowserProfileType PasswordManagerClient::GetProfileType()
    const {
  // This is an abstract interface and thus never instantiated directly,
  // therefore it is safe to always return |kRegular| here.
  return profile_metrics::BrowserProfileType::kRegular;
}

const PasswordManager* PasswordManagerClient::GetPasswordManager() const {
  return nullptr;
}

PasswordManager* PasswordManagerClient::GetPasswordManager() {
  return const_cast<PasswordManager*>(
      static_cast<const PasswordManagerClient*>(this)->GetPasswordManager());
}

const PasswordFeatureManager* PasswordManagerClient::GetPasswordFeatureManager()
    const {
  return nullptr;
}

PasswordFeatureManager* PasswordManagerClient::GetPasswordFeatureManager() {
  return const_cast<PasswordFeatureManager*>(
      static_cast<const PasswordManagerClient*>(this)
          ->GetPasswordFeatureManager());
}

HttpAuthManager* PasswordManagerClient::GetHttpAuthManager() {
  return nullptr;
}

autofill::AutofillDownloadManager*
PasswordManagerClient::GetAutofillDownloadManager() {
  return nullptr;
}

bool PasswordManagerClient::IsCommittedMainFrameSecure() const {
  return false;
}

const autofill::LogManager* PasswordManagerClient::GetLogManager() const {
  return nullptr;
}

void PasswordManagerClient::AnnotateNavigationEntry(bool has_password_field) {}

autofill::LanguageCode PasswordManagerClient::GetPageLanguage() const {
  return autofill::LanguageCode();
}

PasswordRequirementsService*
PasswordManagerClient::GetPasswordRequirementsService() {
  // Not impemented but that is a valid state as per interface definition.
  // Therefore, don't call NOTIMPLEMENTED() here.
  return nullptr;
}

favicon::FaviconService* PasswordManagerClient::GetFaviconService() {
  return nullptr;
}

network::mojom::NetworkContext* PasswordManagerClient::GetNetworkContext()
    const {
  return nullptr;
}

WebAuthnCredentialsDelegate*
PasswordManagerClient::GetWebAuthnCredentialsDelegate() {
  return nullptr;
}

version_info::Channel PasswordManagerClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

}  // namespace password_manager
