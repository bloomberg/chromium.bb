// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "content/public/browser/network_service_instance.h"

#if defined(OS_ANDROID)
#include "components/signin/core/browser/oauth2_token_service_delegate_android.h"
#else
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/oauth2_token_service_delegate.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/signin/signin_util_win.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
std::unique_ptr<chromeos::ChromeOSOAuth2TokenServiceDelegate>
CreateCrOsOAuthDelegate(Profile* profile) {
  chromeos::AccountManagerFactory* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  DCHECK(factory);
  chromeos::AccountManager* account_manager =
      factory->GetAccountManager(profile->GetPath().value());
  DCHECK(account_manager);

  return std::make_unique<chromeos::ChromeOSOAuth2TokenServiceDelegate>(
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile),
      account_manager);
}
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)

// Supervised users cannot revoke credentials.
bool CanRevokeCredentials(Profile* profile) {
#if defined(OS_CHROMEOS)
  // UserManager may not exist in unit_tests.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser()) {
    // Don't allow revoking credentials for Chrome OS supervised users.
    // See http://crbug.com/332032
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
#endif

  return true;
}

std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
CreateMutableProfileOAuthDelegate(Profile* profile) {
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  // When signin cookies are cleared on exit and Dice is enabled, all tokens
  // should also be cleared.
  bool revoke_all_tokens_on_load =
      (account_consistency == signin::AccountConsistencyMethod::kDice) &&
      signin::SettingsDeleteSigninCookiesOnExit(
          CookieSettingsFactory::GetForProfile(profile).get());

  return std::make_unique<MutableProfileOAuth2TokenServiceDelegate>(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile),
      content::GetNetworkConnectionTracker(),
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      account_consistency, revoke_all_tokens_on_load,
      CanRevokeCredentials(profile),
#if defined(OS_WIN)
      base::BindRepeating(&signin_util::ReauthWithCredentialProviderIfPossible,
                          base::Unretained(profile)));
#else
      MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback());
#endif  // defined(OS_WIN)
}
#endif  // !defined(OS_ANDROID)

std::unique_ptr<OAuth2TokenServiceDelegate> CreateOAuth2TokenServiceDelegate(
    Profile* profile) {
#if defined(OS_ANDROID)
  return std::make_unique<OAuth2TokenServiceDelegateAndroid>(
      AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile));

#else  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  if (chromeos::IsAccountManagerAvailable(profile)) {
    return CreateCrOsOAuthDelegate(profile);
  }
#endif  // defined(OS_CHROMEOS)

  // Fall back to |MutableProfileOAuth2TokenServiceDelegate|:
  // 1. On all platforms other than Android and Chrome OS.
  // 2. On Chrome OS, if |ChromeOSOAuth2TokenServiceDelegate| cannot be used
  // for this |profile|. See |chromeos::IsAccountManagerAvailable|.
  return CreateMutableProfileOAuthDelegate(profile);

#endif  // defined(OS_ANDROID)
}

}  // namespace

ProfileOAuth2TokenServiceFactory::ProfileOAuth2TokenServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ProfileOAuth2TokenService",
        BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
#endif
  DependsOn(AccountTrackerServiceFactory::GetInstance());
}

ProfileOAuth2TokenServiceFactory::~ProfileOAuth2TokenServiceFactory() {
}

ProfileOAuth2TokenService*
ProfileOAuth2TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileOAuth2TokenService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileOAuth2TokenServiceFactory*
    ProfileOAuth2TokenServiceFactory::GetInstance() {
  return base::Singleton<ProfileOAuth2TokenServiceFactory>::get();
}

void ProfileOAuth2TokenServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileOAuth2TokenService::RegisterProfilePrefs(registry);
#if !defined(OS_ANDROID)
  MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(registry);
#endif
}

KeyedService* ProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

// On ChromeOS the device ID is not managed by the token service.
#if !defined(OS_CHROMEOS)
  // Ensure the device ID is not empty. This is important for Dice, because the
  // device ID is needed on the network thread, but can only be generated on the
  // main thread.
  std::string device_id = signin::GetSigninScopedDeviceId(profile->GetPrefs());
  DCHECK(!device_id.empty());
#endif

  return new ProfileOAuth2TokenService(
      profile->GetPrefs(), CreateOAuth2TokenServiceDelegate(profile));
}
