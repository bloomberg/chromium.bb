// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_ENVIRONMENT_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_ENVIRONMENT_H_

#include "base/optional.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"

namespace identity {

class IdentityTestEnvironmentInternal;

// Class that creates an IdentityManager for use in testing contexts and
// provides facilities for driving that IdentityManager. The IdentityManager
// instance is brought up in an environment where the primary account is
// not available; call MakePrimaryAccountAvailable() as needed.
class IdentityTestEnvironment : public IdentityManager::DiagnosticsObserver {
 public:
  IdentityTestEnvironment(
      bool use_fake_url_loader_for_gaia_cookie_manager = false);
  ~IdentityTestEnvironment() override;

  // The IdentityManager instance created and owned by this instance.
  IdentityManager* identity_manager();

  // Sets the primary account for the given email address, generating a GAIA ID
  // that corresponds uniquely to that email address. On non-ChromeOS, results
  // in the firing of the IdentityManager and SigninManager callbacks for signin
  // success. Blocks until the primary account is set. Returns the AccountInfo
  // of the newly-set account.
  AccountInfo SetPrimaryAccount(const std::string& email);

  // Sets a refresh token for the primary account (which must already be set).
  // Blocks until the refresh token is set.
  void SetRefreshTokenForPrimaryAccount();

  // Sets a special invalid refresh token for the primary account (which must
  // already be set). Blocks until the refresh token is set.
  void SetInvalidRefreshTokenForPrimaryAccount();

  // Removes any refresh token for the primary account, if present. Blocks until
  // the refresh token is removed.
  void RemoveRefreshTokenForPrimaryAccount();

  // Makes the primary account available for the given email address, generating
  // a GAIA ID and refresh token that correspond uniquely to that email address.
  // On non-ChromeOS platforms, this will also result in the firing of the
  // IdentityManager and SigninManager callbacks for signin success. On all
  // platforms, this method blocks until the primary account is available.
  // Returns the AccountInfo of the newly-available account.
  AccountInfo MakePrimaryAccountAvailable(const std::string& email);

  // Clears the primary account if present, with |policy| used to determine
  // whether to keep or remove all accounts. On non-ChromeOS, results in the
  // firing of the IdentityManager and SigninManager callbacks for signout.
  // Blocks until the primary account is cleared.
  void ClearPrimaryAccount(
      ClearPrimaryAccountPolicy policy = ClearPrimaryAccountPolicy::DEFAULT);

  // Makes an account available for the given email address, generating a GAIA
  // ID and refresh token that correspond uniquely to that email address. Blocks
  // until the account is available. Returns the AccountInfo of the
  // newly-available account.
  AccountInfo MakeAccountAvailable(const std::string& email);

  // Sets a refresh token for the given account (which must already be
  // available). Blocks until the refresh token is set. NOTE: See disclaimer at
  // top of file re: direct usage.
  void SetRefreshTokenForAccount(const std::string& account_id);

  // Sets a special invalid refresh token for the given account (which must
  // already be available). Blocks until the refresh token is set. NOTE: See
  // disclaimer at top of file re: direct usage.
  void SetInvalidRefreshTokenForAccount(const std::string& account_id);

  // Removes any refresh token that is present for the given account. Blocks
  // until the refresh token is removed.
  // NOTE: See disclaimer at top of file re: direct usage.
  void RemoveRefreshTokenForAccount(const std::string& account_id);

  // Puts the given accounts into the Gaia cookie, replacing any previous
  // accounts. Blocks until the accounts have been set.
  void SetCookieAccounts(const std::vector<CookieParams>& cookie_accounts);

  // When this is set, access token requests will be automatically granted with
  // an access token value of "access_token".
  void SetAutomaticIssueOfAccessTokens(bool grant);

  // Issues |token| in response to any access token request that either has (a)
  // already occurred and has not been matched by a previous call to this or
  // other WaitFor... method, or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow IdentityTestEnvironment to be
  // agnostic with respect to whether access token requests are handled
  // synchronously or asynchronously in the production code.
  // NOTE: This version is suitable for use in the common context where access
  // token requests are only being made for one account. If you need to
  // disambiguate requests coming for different accounts, see the version below.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      const std::string& token,
      const base::Time& expiration);

  // Issues |token| in response to an access token request for |account_id| that
  // either already occurred and has not been matched by a previous call to this
  // or other WaitFor... method , or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow
  // IdentityTestEnvironment to be agnostic with respect to whether access token
  // requests are handled synchronously or asynchronously in the production
  // code.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      const std::string& account_id,
      const std::string& token,
      const base::Time& expiration);

  // Issues |error| in response to any access token request that either has (a)
  // already occurred and has not been matched by a previous call to this or
  // other WaitFor... method, or (b) will occur in the future via  In the latter
  // case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow IdentityTestEnvironment to be
  // agnostic with respect to whether access token requests are handled
  // synchronously or asynchronously in the production code.
  // NOTE: This version is suitable for use in the common context where access
  // token requests are only being made for one account. If you need to
  // disambiguate requests coming for different accounts, see the version below.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      const GoogleServiceAuthError& error);

  // Issues |error| in response to an access token request for |account_id| that
  // either has (a) already occurred and has not been matched by a previous call
  // to this or other WaitFor... method, or (b) will occur in the future. In the
  // latter case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow
  // IdentityTestEnvironment to be agnostic with respect to whether access token
  // requests are handled synchronously or asynchronously in the production
  // code.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  // Sets a callback that will be invoked on the next incoming access token
  // request. Note that this can not be combined with the
  // WaitForAccessTokenRequestIfNecessaryAndRespondWith* methods - you must
  // either wait for the callback to get called, or explicitly reset it by
  // passing in a null callback, before the Wait* methods can be used again.
  void SetCallbackForNextAccessTokenRequest(base::OnceClosure callback);

 private:
  struct AccessTokenRequestState {
    AccessTokenRequestState();
    ~AccessTokenRequestState();
    AccessTokenRequestState(AccessTokenRequestState&& other);
    AccessTokenRequestState& operator=(AccessTokenRequestState&& other);

    enum {
      kPending,
      kAvailable,
    } state;
    base::Optional<std::string> account_id;
    base::OnceClosure on_available;
  };

  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override;

  // Handles the notification that an access token request was received for
  // |account_id|. Invokes |on_access_token_request_callback_| if the latter
  // is non-null *and* either |*pending_access_token_requester_| equals
  // |account_id| or |pending_access_token_requester_| is empty.
  void HandleOnAccessTokenRequested(std::string account_id);

  // If a token request for |account_id| (or any account if nullopt) has already
  // been made and not matched by a different call, returns immediately.
  // Otherwise and runs a nested runloop until a matching access token request
  // is observed.
  void WaitForAccessTokenRequestIfNecessary(
      base::Optional<std::string> account_id);

  std::unique_ptr<IdentityTestEnvironmentInternal> internals_;
  base::OnceClosure on_access_token_requested_callback_;
  std::vector<AccessTokenRequestState> requesters_;

  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironment);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_ENVIRONMENT_H_
