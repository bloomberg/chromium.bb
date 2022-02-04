// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"

#include <utility>

#include "ash/components/login/auth/extended_authenticator.h"
#include "ash/components/login/auth/user_context.h"
#include "ash/components/proximity_auth/screenlock_bridge.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/browser/storage_partition.h"

namespace ash {

InSessionPasswordSyncManager::InSessionPasswordSyncManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      clock_(base::DefaultClock::GetInstance()),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)) {
  DCHECK(primary_user_);
  auto* session_manager = session_manager::SessionManager::Get();
  // Extra check as SessionManager may be not initialized in some unit
  // tests
  if (session_manager) {
    session_manager->AddObserver(this);
  }

  screenlock_bridge_ = proximity_auth::ScreenlockBridge::Get();
  DCHECK(screenlock_bridge_);
}

InSessionPasswordSyncManager::~InSessionPasswordSyncManager() {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

bool InSessionPasswordSyncManager::IsLockReauthEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSamlLockScreenReauthenticationEnabledOverrideForTesting)) {
    return true;
  }

  return primary_profile_->GetPrefs()->GetBoolean(
      prefs::kLockScreenReauthenticationEnabled);
}

void InSessionPasswordSyncManager::MaybeForceReauthOnLockScreen(
    ReauthenticationReason reauth_reason) {
  if (!IsLockReauthEnabled()) {
    // Reauth on lock is disabled by a policy.
    return;
  }
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kInvalidToken) {
    // Re-authentication already enforced, no other action is needed.
    return;
  }
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kPolicy &&
      reauth_reason == ReauthenticationReason::kInvalidToken) {
    // Re-authentication already enforced but need to reset it to trigger token
    // update. No other action is needed.
    lock_screen_reauth_reason_ = reauth_reason;
    return;
  }
  if (!primary_user_->force_online_signin()) {
    // force_online_signin flag is not set - do not update the screen.
    return;
  }
  if (screenlock_bridge_->IsLocked()) {
    // On the lock screen: need to update the UI.
    screenlock_bridge_->lock_handler()->SetAuthType(
        primary_user_->GetAccountId(),
        proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, std::u16string());
  }
  lock_screen_reauth_reason_ = reauth_reason;
}

void InSessionPasswordSyncManager::SetClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

void InSessionPasswordSyncManager::Shutdown() {}

void InSessionPasswordSyncManager::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    // We are unlocking the session, no further action required.
    return;
  }
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kNone) {
    // locking the session but no re-auth flag set - show standard UI.
    return;
  }

  // Request re-auth immediately after locking the screen.
  screenlock_bridge_->lock_handler()->SetAuthType(
      primary_user_->GetAccountId(),
      proximity_auth::mojom::AuthType::ONLINE_SIGN_IN, std::u16string());
}

void InSessionPasswordSyncManager::UpdateOnlineAuth() {
  PrefService* prefs = primary_profile_->GetPrefs();
  const base::Time now = clock_->Now();
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, now);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), false);
  user_manager::known_user::SetLastOnlineSignin(primary_user_->GetAccountId(),
                                                now);
}

void InSessionPasswordSyncManager::CreateTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenCreate();
}

void InSessionPasswordSyncManager::OnTokenCreated(const std::string& token) {
  password_sync_token_fetcher_.reset();
  PrefService* prefs = primary_profile_->GetPrefs();

  // Set token value in prefs for in-session operations and ephemeral users and
  // local settings for login screen sync.
  prefs->SetString(prefs::kSamlPasswordSyncToken, token);
  user_manager::known_user::SetPasswordSyncToken(primary_user_->GetAccountId(),
                                                 token);
  lock_screen_reauth_reason_ = ReauthenticationReason::kNone;
}

void InSessionPasswordSyncManager::FetchTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenGet();
}

void InSessionPasswordSyncManager::OnTokenFetched(const std::string& token) {
  password_sync_token_fetcher_.reset();
  if (!token.empty()) {
    // Set token fetched from the endpoint in prefs and local settings.
    PrefService* prefs = primary_profile_->GetPrefs();
    prefs->SetString(prefs::kSamlPasswordSyncToken, token);
    user_manager::known_user::SetPasswordSyncToken(
        primary_user_->GetAccountId(), token);
    lock_screen_reauth_reason_ = ReauthenticationReason::kNone;
  } else {
    // This is the first time a sync token is created for the user: we need to
    // initialize its value by calling the API and store it locally.
    CreateTokenAsync();
  }
}

void InSessionPasswordSyncManager::OnTokenVerified(bool is_valid) {
  // InSessionPasswordSyncManager does not verify the sync token.
}

void InSessionPasswordSyncManager::OnApiCallFailed(
    PasswordSyncTokenFetcher::ErrorType error_type) {
  // If error_type == kGetNoList || kGetNoToken the token API is not
  // initialized yet and we can fix it by creating a new token on lock
  // screen re-authentication.
  // All other API errors will be ignored since they are logged by
  // TokenFetcher and will be re-tried.
  password_sync_token_fetcher_.reset();
  if (error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoList ||
      error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoToken) {
    CreateTokenAsync();
  }
}

void InSessionPasswordSyncManager::CheckCredentials(
    const UserContext& user_context,
    PasswordChangedCallback callback) {
  user_context_ = user_context;
  password_changed_callback_ = std::move(callback);
  content::StoragePartition* signin_partition = login::GetSigninPartition();
  if (!signin_partition) {
    LOG(ERROR) << "The sign-in partition is not available yet";
    OnCookiesTransfered();
    return;
  }

  bool transfer_saml_auth_cookies_on_subsequent_login = false;
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(primary_profile_);
  if (user->IsAffiliated()) {
    CrosSettings::Get()->GetBoolean(
        kAccountsPrefTransferSAMLCookies,
        &transfer_saml_auth_cookies_on_subsequent_login);
  }

  ProfileAuthData::Transfer(
      signin_partition, primary_profile_->GetDefaultStoragePartition(),
      false /*transfer_auth_cookies_on_first_login*/,
      transfer_saml_auth_cookies_on_subsequent_login,
      base::BindOnce(&InSessionPasswordSyncManager::OnCookiesTransfered,
                     weak_factory_.GetWeakPtr()));
}

void InSessionPasswordSyncManager::OnCookiesTransfered() {
  if (!extended_authenticator_) {
    extended_authenticator_ = ExtendedAuthenticator::Create(this);
  }
  extended_authenticator_.get()->AuthenticateToCheck(user_context_,
                                                     base::OnceClosure());
}

void InSessionPasswordSyncManager::UpdateUserPassword(
    const std::string& old_password) {
  if (!authenticator_) {
    authenticator_ = new ChromeCryptohomeAuthenticator(this);
  }
  authenticator_->MigrateKey(user_context_, old_password);
}

// TODO(crbug.com/1163777): Add UMA histograms for lockscreen online
// re-authentication.
void InSessionPasswordSyncManager::OnAuthFailure(const AuthFailure& error) {
  password_changed_callback_.Run();
}

void InSessionPasswordSyncManager::OnAuthSuccess(
    const UserContext& user_context) {
  if (user_context.GetAccountId() != primary_user_->GetAccountId()) {
    // Tried to re-authenicate with non-primary user: the authentication was
    // successful but we are allowed to unlock only with valid credentials of
    // the user who locked the screen. In this case show customized version
    // of first re-auth flow dialog with an error message.
    // TODO(crbug.com/1090341)
    return;
  }

  UpdateOnlineAuth();
  if (lock_screen_reauth_reason_ == ReauthenticationReason::kInvalidToken) {
    FetchTokenAsync();
  } else {
    lock_screen_reauth_reason_ = ReauthenticationReason::kNone;
  }
  if (screenlock_bridge_->IsLocked()) {
    screenlock_bridge_->lock_handler()->Unlock(user_context.GetAccountId());
  }
  DismissDialog();
}

void InSessionPasswordSyncManager::CreateAndShowDialog() {
  if (!IsLockReauthEnabled())
    NOTREACHED();
  DCHECK(!lock_screen_start_reauth_dialog_);
  lock_screen_start_reauth_dialog_ =
      std::make_unique<LockScreenStartReauthDialog>();
  lock_screen_start_reauth_dialog_->Show();
}

void InSessionPasswordSyncManager::DismissDialog() {
  if (lock_screen_start_reauth_dialog_) {
    lock_screen_start_reauth_dialog_->Dismiss();
  }
}

void InSessionPasswordSyncManager::ResetDialog() {
  DCHECK(lock_screen_start_reauth_dialog_);
  lock_screen_start_reauth_dialog_.reset();
}

int InSessionPasswordSyncManager::GetDialogWidth() {
  if (!lock_screen_start_reauth_dialog_)
    return 0;
  return lock_screen_start_reauth_dialog_->GetDialogWidth();
}

bool InSessionPasswordSyncManager::IsReauthDialogLoadedForTesting(
    base::OnceClosure callback) {
  if (is_dialog_loaded_for_testing_)
    return true;
  DCHECK(!on_dialog_loaded_callback_for_testing_);
  on_dialog_loaded_callback_for_testing_ = std::move(callback);
  return false;
}

void InSessionPasswordSyncManager::OnReauthDialogReadyForTesting() {
  if (is_dialog_loaded_for_testing_)
    return;
  is_dialog_loaded_for_testing_ = true;
  if (on_dialog_loaded_callback_for_testing_) {
    std::move(on_dialog_loaded_callback_for_testing_).Run();
  }
}

}  // namespace ash
