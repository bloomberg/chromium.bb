// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_builder.h"

#include <string>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"

#if defined(OS_ANDROID)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#include "components/signin/public/webdata/token_web_data.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_chromeos.h"
#endif

#if defined(OS_IOS)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#endif

namespace {

#if defined(OS_ANDROID)
// TODO(crbug.com/986435) Provide AccountManagerFacade as a parameter once
// IdentityServicesProvider owns its instance management.
std::unique_ptr<ProfileOAuth2TokenServiceDelegateAndroid>
CreateAndroidOAuthDelegate(AccountTrackerService* account_tracker_service) {
  return std::make_unique<ProfileOAuth2TokenServiceDelegateAndroid>(
      account_tracker_service);
}
#elif defined(OS_IOS)
std::unique_ptr<ProfileOAuth2TokenServiceIOSDelegate> CreateIOSOAuthDelegate(
    SigninClient* signin_client,
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
    AccountTrackerService* account_tracker_service) {
  return std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
      signin_client, std::move(device_accounts_provider),
      account_tracker_service);
}
#elif BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
std::unique_ptr<ProfileOAuth2TokenServiceDelegate> CreateCrOsOAuthDelegate(
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker,
    account_manager::AccountManagerFacade* account_manager_facade,
    bool is_regular_profile) {
  return std::make_unique<signin::ProfileOAuth2TokenServiceDelegateChromeOS>(
      account_tracker_service, network_connection_tracker,
      account_manager_facade, is_regular_profile);
}
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)

std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
CreateMutableProfileOAuthDelegate(
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    bool delete_signin_cookies_on_exit,
    scoped_refptr<TokenWebData> token_web_data,
    SigninClient* signin_client,
#if defined(OS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    network::NetworkConnectionTracker* network_connection_tracker) {
  // When signin cookies are cleared on exit and Dice is enabled, all tokens
  // should also be cleared.
  bool revoke_all_tokens_on_load =
      (account_consistency == signin::AccountConsistencyMethod::kDice) &&
      delete_signin_cookies_on_exit;

  return std::make_unique<MutableProfileOAuth2TokenServiceDelegate>(
      signin_client, account_tracker_service, network_connection_tracker,
      token_web_data, account_consistency, revoke_all_tokens_on_load,
#if defined(OS_WIN)
      reauth_callback
#else
      MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback()
#endif  // defined(OS_WIN)
  );
}
#endif  // defined(OS_ANDROID)

std::unique_ptr<ProfileOAuth2TokenServiceDelegate>
CreateOAuth2TokenServiceDelegate(
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* signin_client,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    account_manager::AccountManagerFacade* account_manager_facade,
    bool is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool delete_signin_cookies_on_exit,
    scoped_refptr<TokenWebData> token_web_data,
#endif
#if defined(OS_IOS)
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
#endif
#if defined(OS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    network::NetworkConnectionTracker* network_connection_tracker) {
#if defined(OS_ANDROID)
  return CreateAndroidOAuthDelegate(account_tracker_service);
#elif defined(OS_IOS)
  return CreateIOSOAuthDelegate(signin_client,
                                std::move(device_accounts_provider),
                                account_tracker_service);
#elif BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return CreateCrOsOAuthDelegate(account_tracker_service,
                                 network_connection_tracker,
                                 account_manager_facade, is_regular_profile);
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Fall back to |MutableProfileOAuth2TokenServiceDelegate| on all platforms
  // other than Android, iOS, and Chrome OS (Ash and Lacros).
  return CreateMutableProfileOAuthDelegate(
      account_tracker_service, account_consistency,
      delete_signin_cookies_on_exit, token_web_data, signin_client,
#if defined(OS_WIN)
      reauth_callback,
#endif  // defined(OS_WIN)
      network_connection_tracker);
#else
  NOTREACHED();
  return nullptr;
#endif  // defined(OS_ANDROID)
}

}  // namespace

std::unique_ptr<ProfileOAuth2TokenService> BuildProfileOAuth2TokenService(
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker,
    signin::AccountConsistencyMethod account_consistency,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    account_manager::AccountManagerFacade* account_manager_facade,
    bool is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool delete_signin_cookies_on_exit,
    scoped_refptr<TokenWebData> token_web_data,
#endif
#if defined(OS_IOS)
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
#endif
#if defined(OS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    SigninClient* signin_client) {
// On ChromeOS the device ID is not managed by the token service.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Ensure the device ID is not empty. This is important for Dice, because the
  // device ID is needed on the network thread, but can only be generated on the
  // main thread.
  std::string device_id = signin::GetSigninScopedDeviceId(pref_service);
  DCHECK(!device_id.empty());
#endif

  return std::make_unique<ProfileOAuth2TokenService>(
      pref_service,
      CreateOAuth2TokenServiceDelegate(
          account_tracker_service, account_consistency, signin_client,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
          account_manager_facade, is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          delete_signin_cookies_on_exit, token_web_data,
#endif
#if defined(OS_IOS)
          std::move(device_accounts_provider),
#endif
#if defined(OS_WIN)
          reauth_callback,
#endif
          network_connection_tracker));
}
