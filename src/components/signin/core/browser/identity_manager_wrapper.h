// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_MANAGER_WRAPPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_MANAGER_WRAPPER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "services/identity/public/cpp/identity_manager.h"

class ProfileOAuth2TokenService;
class SigninManagerBase;
class GaiaCookieManagerService;
class AccountFetcherService;
class SigninManagerBase;
class AccountsMutator;

namespace identity {
class PrimaryAccountMutator;
class AccountsCookieMutator;
class DiagnosticsProvider;
}  // namespace identity

// Subclass that wraps IdentityManager in a KeyedService.
// TODO(https://crbug.com/952788): This class can be deleted if
// IdentityManager is updated to inherit from KeyedService directly.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  IdentityManagerWrapper(
      AccountTrackerService* account_tracker_service,
      std::unique_ptr<ProfileOAuth2TokenService> token_service,
      std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
      std::unique_ptr<SigninManagerBase> signin_manager,
      std::unique_ptr<AccountFetcherService> account_fetcher_service,
      std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<identity::AccountsMutator> accounts_mutator,
      std::unique_ptr<identity::AccountsCookieMutator> accounts_cookie_mutator,
      std::unique_ptr<identity::DiagnosticsProvider> diagnostics_provider);

  // KeyedService overrides.
  void Shutdown() override;
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_MANAGER_WRAPPER_H_
