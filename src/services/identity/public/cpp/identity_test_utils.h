// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_UTILS_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_UTILS_H_

#include <string>

#include "build/build_config.h"
#include "components/signin/core/browser/account_info.h"

class AccountTrackerService;
class FakeGaiaCookieManagerService;
class FakeSigninManagerBase;
class FakeSigninManager;
class ProfileOAuth2TokenService;
class SigninManagerBase;

#if defined(OS_CHROMEOS)
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

// Test-related utilities that don't fit in either IdentityTestEnvironment or
// IdentityManager itself. NOTE: Using these utilities directly is discouraged,
// but sometimes necessary during conversion. Use IdentityTestEnvironment if
// possible. These utilities should be used directly only if the production code
// is using IdentityManager, but it is not yet feasible to convert the test code
// to use IdentityTestEnvironment. Any such usage should only be temporary,
// i.e., should be followed as quickly as possible by conversion of the test
// code to use IdentityTestEnvironment.
namespace identity {

// Controls whether to keep or remove accounts when clearing the primary
// account.
enum class ClearPrimaryAccountPolicy {
  // Use the default internal policy.
  DEFAULT,
  // Explicitly keep all accounts.
  KEEP_ALL_ACCOUNTS,
  // Explicitly remove all accounts.
  REMOVE_ALL_ACCOUNTS
};

struct CookieParams {
  std::string email;
  std::string gaia_id;
};

class IdentityManager;

// Sets the primary account (which must not already be set) to the given email
// address, generating a GAIA ID that corresponds uniquely to that email
// address. On non-ChromeOS, results in the firing of the IdentityManager and
// SigninManager callbacks for signin success. Blocks until the primary account
// is set. Returns the AccountInfo of the newly-set account.
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo SetPrimaryAccount(SigninManagerBase* signin_manager,
                              IdentityManager* identity_manager,
                              const std::string& email);

// Sets a refresh token for the primary account (which must already be set).
// Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetRefreshTokenForPrimaryAccount(ProfileOAuth2TokenService* token_service,
                                      IdentityManager* identity_manager);

// Sets a special invalid refresh token for the primary account (which must
// already be set). Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetInvalidRefreshTokenForPrimaryAccount(
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager);

// Removes any refresh token for the primary account, if present. Blocks until
// the refresh token is removed.
// NOTE: See disclaimer at top of file re: direct usage.
void RemoveRefreshTokenForPrimaryAccount(
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager);

// Makes the primary account (which must not already be set) available for the
// given email address, generating a GAIA ID and refresh token that correspond
// uniquely to that email address. On non-ChromeOS, results in the firing of the
// IdentityManager and SigninManager callbacks for signin success. Blocks until
// the primary account is available. Returns the AccountInfo of the
// newly-available account.
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakePrimaryAccountAvailable(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager,
    const std::string& email);

// Clears the primary account if present, with |policy| used to determine
// whether to keep or remove all accounts. On non-ChromeOS, results in the
// firing of the IdentityManager and SigninManager callbacks for signout. Blocks
// until the primary account is cleared.
// NOTE: See disclaimer at top of file re: direct usage.
void ClearPrimaryAccount(
    SigninManagerBase* signin_manager,
    IdentityManager* identity_manager,
    ClearPrimaryAccountPolicy policy = ClearPrimaryAccountPolicy::DEFAULT);

// Makes an account available for the given email address, generating a GAIA ID
// and refresh token that correspond uniquely to that email address. Blocks
// until the account is available. Returns the AccountInfo of the
// newly-available account.
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakeAccountAvailable(AccountTrackerService* account_tracker_service,
                                 ProfileOAuth2TokenService* token_service,
                                 IdentityManager* identity_manager,
                                 const std::string& email);

// Sets a refresh token for the given account (which must already be available).
// Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                               IdentityManager* identity_manager,
                               const std::string& account_id);

// Sets a special invalid refresh token for the given account (which must
// already be available). Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetInvalidRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                      IdentityManager* identity_manager,
                                      const std::string& account_id);

// Removes any refresh token that is present for the given account. Blocks until
// the refresh token is removed. Is a no-op if no refresh token is present for
// the given account.
// NOTE: See disclaimer at top of file re: direct usage.
void RemoveRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                  IdentityManager* identity_manager,
                                  const std::string& account_id);

// Puts the given accounts into the Gaia cookie, replacing any previous
// accounts. Blocks until the accounts have been set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetCookieAccounts(FakeGaiaCookieManagerService* cookie_manager,
                       IdentityManager* identity_manager,
                       const std::vector<CookieParams>& cookie_accounts);

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_UTILS_H_
