// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager_builder.h"

#include <string>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"
#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"
#include "components/signin/internal/identity_manager/primary_account_policy_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_builder.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"

#if !defined(OS_ANDROID)
#include "components/signin/public/webdata/token_web_data.h"
#endif

#if defined(OS_IOS)
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/signin/internal/identity_manager/primary_account_policy_manager_impl.h"
#endif

namespace signin {

namespace {

std::unique_ptr<AccountTrackerService> BuildAccountTrackerService(
    PrefService* pref_service,
    base::FilePath profile_path) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(pref_service, profile_path);
  return account_tracker_service;
}

std::unique_ptr<PrimaryAccountManager> BuildPrimaryAccountManager(
    SigninClient* client,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    PrefService* local_state) {
  std::unique_ptr<PrimaryAccountManager> primary_account_manager;
  std::unique_ptr<PrimaryAccountPolicyManager> policy_manager;
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_IOS)
  policy_manager = std::make_unique<PrimaryAccountPolicyManagerImpl>(client);
#endif
  primary_account_manager = std::make_unique<PrimaryAccountManager>(
      client, token_service, account_tracker_service,
      std::move(policy_manager));
  primary_account_manager->Initialize(local_state);
  return primary_account_manager;
}

std::unique_ptr<AccountsMutator> BuildAccountsMutator(
    PrefService* prefs,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  return std::make_unique<AccountsMutatorImpl>(
      token_service, account_tracker_service, primary_account_manager, prefs);
#else
  return nullptr;
#endif
}

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(signin_client, token_service,
                                      account_tracker_service,
                                      std::move(image_decoder));
  return account_fetcher_service;
}

}  // anonymous namespace

IdentityManagerBuildParams::IdentityManagerBuildParams() = default;

IdentityManagerBuildParams::~IdentityManagerBuildParams() = default;

IdentityManager::InitParameters BuildIdentityManagerInitParameters(
    IdentityManagerBuildParams* params) {
  std::unique_ptr<AccountTrackerService> account_tracker_service =
      BuildAccountTrackerService(params->pref_service, params->profile_path);

  std::unique_ptr<ProfileOAuth2TokenService> token_service =
      BuildProfileOAuth2TokenService(
          params->pref_service, account_tracker_service.get(),
          params->network_connection_tracker, params->account_consistency,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
          params->account_manager_facade, params->is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          params->delete_signin_cookies_on_exit, params->token_web_data,
#endif
#if defined(OS_IOS)
          std::move(params->device_accounts_provider),
#endif
#if defined(OS_WIN)
          params->reauth_callback,
#endif
          params->signin_client);

  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      token_service.get(), params->signin_client);

  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      BuildPrimaryAccountManager(params->signin_client,
                                 account_tracker_service.get(),
                                 token_service.get(), params->local_state);

  IdentityManager::InitParameters init_params;

  init_params.primary_account_mutator =
      std::make_unique<PrimaryAccountMutatorImpl>(
          account_tracker_service.get(), token_service.get(),
          primary_account_manager.get(), params->pref_service,
          params->account_consistency);

  init_params.accounts_mutator =
      BuildAccountsMutator(params->pref_service, account_tracker_service.get(),
                           token_service.get(), primary_account_manager.get());

  init_params.accounts_cookie_mutator =
      std::make_unique<AccountsCookieMutatorImpl>(
          params->signin_client, token_service.get(),
          gaia_cookie_manager_service.get(), account_tracker_service.get());

  init_params.diagnostics_provider = std::make_unique<DiagnosticsProviderImpl>(
      token_service.get(), gaia_cookie_manager_service.get());

  init_params.account_fetcher_service = BuildAccountFetcherService(
      params->signin_client, token_service.get(), account_tracker_service.get(),
      std::move(params->image_decoder));

#if defined(OS_IOS) || defined(OS_ANDROID)
  init_params.device_accounts_synchronizer =
      std::make_unique<DeviceAccountsSynchronizerImpl>(
          token_service->GetDelegate());
#endif

  init_params.account_tracker_service = std::move(account_tracker_service);
  init_params.gaia_cookie_manager_service =
      std::move(gaia_cookie_manager_service);
  init_params.primary_account_manager = std::move(primary_account_manager);
  init_params.token_service = std::move(token_service);
  init_params.account_consistency = params->account_consistency;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  init_params.signin_client = params->signin_client;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  init_params.account_manager_facade = params->account_manager_facade;
#endif

  return init_params;
}

std::unique_ptr<IdentityManager> BuildIdentityManager(
    IdentityManagerBuildParams* params) {
  return std::make_unique<IdentityManager>(
      BuildIdentityManagerInitParameters(params));
}

}  // namespace signin
