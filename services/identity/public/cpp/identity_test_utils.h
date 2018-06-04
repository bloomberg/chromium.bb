// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_UTILS_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_UTILS_H_

#include <string>

#include "build/build_config.h"

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

class IdentityManager;

// Sets the primary account for the given email address, generating a GAIA ID
// that corresponds uniquely to that email address. On non-ChromeOS, results in
// the firing of the IdentityManager and SigninManager callbacks for signin
// success. Blocks until the primary account is set. Returns the account ID
// of the newly-set account.
// NOTE: See disclaimer at top of file re: direct usage.
std::string SetPrimaryAccount(SigninManagerBase* signin_manager,
                              IdentityManager* identity_manager,
                              const std::string& email);

// Sets a refresh token that corresponds uniquely to |account_id|. Blocks until
// the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                               IdentityManager* identity_manager,
                               const std::string& account_id);

// Makes the primary account available for the given email address, generating a
// GAIA ID and refresh token that correspond uniquely to that email address. On
// non-ChromeOS, results in the firing of the IdentityManager and SigninManager
// callbacks for signin success. Blocks until the primary account is available.
// Returns the account ID of the newly-available account.
// NOTE: See disclaimer at top of file re: direct usage.
std::string MakePrimaryAccountAvailable(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager,
    const std::string& email);

// Clears the primary account. On non-ChromeOS, results in the firing of the
// IdentityManager and SigninManager callbacks for signout. Blocks until the
// primary account is cleared.
// Note that this function requires FakeSigninManager, as it internally invokes
// functionality of the fake. If a use case emerges for invoking this
// functionality with a production SigninManager, contact blundell@chromium.org.
// NOTE: See disclaimer at top of file re: direct usage.
void ClearPrimaryAccount(SigninManagerForTest* signin_manager,
                         IdentityManager* identity_manager);

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_UTILS_H_
