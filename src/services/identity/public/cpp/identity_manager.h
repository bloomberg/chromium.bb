// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/ubertoken_fetcher.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"
#include "services/identity/public/cpp/accounts_in_cookie_jar_info.h"
#include "services/identity/public/cpp/scope_set.h"

#if !defined(OS_CHROMEOS)
#include "components/signin/core/browser/signin_manager.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

// Necessary to declare this class as a friend.
namespace arc {
class ArcTermsOfServiceDefaultNegotiatorTest;
}

// Necessary to declare these classes as friends.
namespace chromeos {
class ChromeSessionManager;
class UserSessionManager;
}  // namespace chromeos

namespace network {
class SharedURLLoaderFactory;
class TestURLLoaderFactory;
}  // namespace network

// Necessary to declare these classes as friends.
class ArcSupportHostTest;
class IdentityManagerFactory;
class PrefRegistrySimple;

namespace identity {

class AccountsMutator;
class AccountsCookieMutator;
class DiagnosticsProvider;
class PrimaryAccountMutator;
enum class ClearPrimaryAccountPolicy;
struct CookieParams;

// Gives access to information about the user's Google identities. See
// ./README.md for detailed documentation.
class IdentityManager : public SigninManagerBase::Observer,
                        public OAuth2TokenService::DiagnosticsObserver,
                        public OAuth2TokenService::Observer,
                        public GaiaCookieManagerService::Observer,
                        public AccountTrackerService::Observer {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called when an account becomes the user's primary account.
    // This method is not called during a reauth.
    virtual void OnPrimaryAccountSet(
        const CoreAccountInfo& primary_account_info) {}

    // Called when when the user moves from having a primary account to no
    // longer having a primary account.
    virtual void OnPrimaryAccountCleared(
        const CoreAccountInfo& previous_primary_account_info) {}

    // Called when the user attempts but fails to set their primary
    // account. |error| gives the reason for the failure.
    virtual void OnPrimaryAccountSigninFailed(
        const GoogleServiceAuthError& error) {}

    // Called when a new refresh token is associated with |account_info|.
    // NOTE: On a signin event, the ordering of this callback wrt the
    // OnPrimaryAccountSet() callback is undefined. If you as a client are
    // interested in both callbacks, PrimaryAccountAccessTokenFetcher will
    // likely meet your needs. Otherwise, if this lack of ordering is
    // problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info) {}

    // Called when the refresh token previously associated with |account_id|
    // has been removed. At the time that this callback is invoked, there is
    // no longer guaranteed to be any AccountInfo associated with
    // |account_id|.
    // NOTE: It is not guaranteed that a call to
    // OnRefreshTokenUpdatedForAccount() has previously occurred for this
    // account due to corner cases.
    // TODO(https://crbug.com/884731): Eliminate these corner cases.
    // NOTE: On a signout event, the ordering of this callback wrt the
    // OnPrimaryAccountCleared() callback is undefined.If this lack of ordering
    // is problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenRemovedForAccount(
        const std::string& account_id) {}

    // Called when the error state of the refresh token for |account_id| has
    // changed. Note: It is always called after
    // |OnRefreshTokenUpdatedForAccount| when the refresh token is updated. It
    // is not called when the refresh token is removed.
    virtual void OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error) {}

    // Called after refresh tokens are loaded.
    // CAVEAT: On ChromeOS, this callback is not invoked during
    // startup in all cases. See https://crbug.com/749535, which
    // details the cases where it's not invoked.
    virtual void OnRefreshTokensLoaded() {}

    // Called whenever the list of Gaia accounts in the cookie jar has changed.
    // |accounts_in_cookie_jar_info.accounts| is ordered by the order of the
    // accounts in the cookie.
    //
    // This observer method is also called when fetching the list of accounts
    // in Gaia cookies fails after a number of internal retries. In this case:
    // * |error| hold the last error to fetch the list of accounts;
    // * |accounts_in_cookie_jar_info.accounts_are_fresh| is set to false as
    //   the accounts information is considered stale;
    // * |accounts_in_cookie_jar_info.accounts| holds the last list of known
    //   accounts in the cookie jar.
    virtual void OnAccountsInCookieUpdated(
        const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
        const GoogleServiceAuthError& error) {}

    // Called when the Gaia cookie has been deleted explicitly by a user
    // action, e.g. from the settings or by an extension.
    virtual void OnAccountsCookieDeletedByUserAction() {}

    // Called after a batch of refresh token state chagnes is completed.
    virtual void OnEndBatchOfRefreshTokenStateChanges() {}

    // Called after an account is updated.
    virtual void OnExtendedAccountInfoUpdated(const AccountInfo& info) {}

    // Called after removing an account info.
    virtual void OnExtendedAccountInfoRemoved(const AccountInfo& info) {}
  };

  // Observer interface for classes that want to monitor status of various
  // requests. Mostly useful in tests and debugging contexts (e.g., WebUI).
  class DiagnosticsObserver {
   public:
    DiagnosticsObserver() = default;
    virtual ~DiagnosticsObserver() = default;

    DiagnosticsObserver(const DiagnosticsObserver&) = delete;
    DiagnosticsObserver& operator=(const DiagnosticsObserver&) = delete;

    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(const std::string& account_id,
                                        const std::string& consumer_id,
                                        const identity::ScopeSet& scopes) {}

    // Called when an access token request is completed. Contains diagnostic
    // information about the access token request.
    virtual void OnAccessTokenRequestCompleted(const std::string& account_id,
                                               const std::string& consumer_id,
                                               const ScopeSet& scopes,
                                               GoogleServiceAuthError error,
                                               base::Time expiration_time) {}

    // Called when an access token was removed.
    virtual void OnAccessTokenRemovedFromCache(const std::string& account_id,
                                               const ScopeSet& scopes) {}

    // Called when a new refresh token is available. Contains diagnostic
    // information about the source of the operation.
    virtual void OnRefreshTokenUpdatedForAccountFromSource(
        const std::string& account_id,
        bool is_refresh_token_valid,
        const std::string& source) {}

    // Called when a refreh token is removed. Contains diagnostic information
    // about the source that initiated the revokation operation.
    virtual void OnRefreshTokenRemovedForAccountFromSource(
        const std::string& account_id,
        const std::string& source) {}
  };

  // Possible values for the account ID migration state, needs to be kept in
  // sync with AccountTrackerService::AccountIdMigrationState.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED = 0,
    MIGRATION_IN_PROGRESS = 1,
    MIGRATION_DONE = 2,
    NUM_MIGRATION_STATES
  };

  IdentityManager(
      SigninManagerBase* signin_manager,
      ProfileOAuth2TokenService* token_service,
      AccountFetcherService* account_fetcher_service,
      AccountTrackerService* account_tracker_service,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      std::unique_ptr<PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<AccountsMutator> accounts_mutator,
      std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator,
      std::unique_ptr<DiagnosticsProvider> diagnostics_provider);
  ~IdentityManager() override;

  // Provides access to the extended information of the user's primary account.
  // Returns an empty struct if no such info is available, either because there
  // is no primary account or because the extended information for the primary
  // account has been removed (this happens when the refresh token is revoked,
  // for example).
  AccountInfo GetPrimaryAccountInfo() const;

  // Provides access to the account ID of the user's primary account. Note that
  // this may return a valid string even in cases where GetPrimaryAccountInfo()
  // returns an empty struct, as the extended information for the primary
  // account is removed on certain events (e.g., when its refresh token is
  // revoked).
  const std::string& GetPrimaryAccountId() const;

  // Returns whether the primary account is available. Simple convenience
  // wrapper over checking whether GetPrimaryAccountId() returns a non-empty
  // string.
  bool HasPrimaryAccount() const;

  // Provides access to the latest cached information of all accounts that have
  // refresh tokens.
  // NOTE: The accounts should not be assumed to be in any particular order; in
  // particular, they are not guaranteed to be in the order in which the
  // refresh tokens were added.
  std::vector<AccountInfo> GetAccountsWithRefreshTokens() const;

  // Provides access to the latest cached information of all accounts that are
  // present in the Gaia cookie in the cookie jar, ordered by their order in
  // the cookie.
  // If the returned accounts are not fresh, an internal update will be
  // triggered and there will be a subsequent invocation of
  // IdentityManager::Observer::OnAccountsInCookieJarChanged().
  AccountsInCookieJarInfo GetAccountsInCookieJar() const;

  // Returns true if a refresh token exists for |account_id|.
  bool HasAccountWithRefreshToken(const std::string& account_id) const;

  // Returns true if (a) a refresh token exists for |account_id|, and (b) the
  // refresh token is in a persistent error state (defined as
  // GoogleServiceAuthError::IsPersistentError() returning true for the error
  // returned by GetErrorStateOfRefreshTokenForAccount(account_id)).
  bool HasAccountWithRefreshTokenInPersistentErrorState(
      const std::string& account_id) const;

  // Returns the error state of the refresh token associated with |account_id|.
  // In particular: Returns GoogleServiceAuthError::AuthErrorNone() if either
  // (a) no refresh token exists for |account_id|, or (b) the refresh token is
  // not in a persistent error state. Otherwise, returns the last persistent
  // error that was detected when using the refresh token.
  GoogleServiceAuthError GetErrorStateOfRefreshTokenForAccount(
      const std::string& account_id) const;

  // Returns true if (a) the primary account exists, and (b) a refresh token
  // exists for the primary account.
  bool HasPrimaryAccountWithRefreshToken() const;

  // Returns true if all refresh tokens have been loaded from disk.
  bool AreRefreshTokensLoaded() const;

  // Returns extended information for account identified by |account_info|.
  // The information will be returned if the information is available and
  // refresh token is available for account.
  base::Optional<AccountInfo> FindExtendedAccountInfoForAccount(
      const CoreAccountInfo& account_info) const;

  // Looks up and returns information for account with given |account_id|. If
  // the account cannot be found, return an empty optional. This is equivalent
  // to searching on the vector returned by GetAccountsWithRefreshTokens() but
  // without allocating memory for the vector.
  base::Optional<AccountInfo>
  FindAccountInfoForAccountWithRefreshTokenByAccountId(
      const std::string& account_id) const;

  // Looks up and returns information for account with given |email_address|. If
  // the account cannot be found, return an empty optional. This is equivalent
  // to searching on the vector returned by GetAccountsWithRefreshTokens() but
  // without allocating memory for the vector.
  base::Optional<AccountInfo>
  FindAccountInfoForAccountWithRefreshTokenByEmailAddress(
      const std::string& email_address) const;

  // Looks up and returns information for account with given |gaia_id|. If the
  // account cannot be found, return an empty optional. This is equivalent to
  // searching on the vector returned by GetAccountsWithRefreshTokens() but
  // without allocating memory for the vector.
  base::Optional<AccountInfo> FindAccountInfoForAccountWithRefreshTokenByGaiaId(
      const std::string& gaia_id) const;

  // Creates an AccessTokenFetcher given the passed-in information.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForAccount(
      const std::string& account_id,
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode);

  // Creates an AccessTokenFetcher given the passed-in information, allowing
  // to specify a custom |url_loader_factory| as well.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForAccount(
      const std::string& account_id,
      const std::string& oauth_consumer_name,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode);

  // Creates an AccessTokenFetcher given the passed-in information, allowing to
  // specify custom |client_id| and |client_secret| to identify the OAuth client
  // app.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForClient(
      const std::string& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode);

  // If an entry exists in the Identity Service's cache corresponding to the
  // given information, removes that entry; in this case, the next access token
  // request for |account_id| and |scopes| will fetch a new token from the
  // network. Otherwise, is a no-op.
  void RemoveAccessTokenFromCache(const std::string& account_id,
                                  const identity::ScopeSet& scopes,
                                  const std::string& access_token);

  // Creates an UbertokenFetcher given the passed-in information, allowing
  // to specify a custom |url_loader_factory| as well.
  std::unique_ptr<signin::UbertokenFetcher> CreateUbertokenFetcherForAccount(
      const std::string& account_id,
      signin::UbertokenFetcher::CompletionCallback callback,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool bound_to_channel_id = true);

  // Returns |true| if migration of the account ID from normalized email is
  // supported for the current platform.
  static bool IsAccountIdMigrationSupported();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Marks the migration state for account IDs as finished.
  void LegacySetAccountIdMigrationDone();

  // Returns the currently saved state for the migration of accounts IDs.
  AccountIdMigrationState GetAccountIdMigrationState() const;

  // Returns pointer to the object used to change the signed-in state of the
  // primary account, if supported on the current platform. Otherwise, returns
  // null.
  PrimaryAccountMutator* GetPrimaryAccountMutator();

  // Returns pointer to the object used to seed accounts and mutate state of
  // accounts' refresh tokens, if supported on the current platform. Otherwise,
  // returns null.
  AccountsMutator* GetAccountsMutator();

  // Returns pointer to the object used to manipulate the cookies stored and the
  // accounts associated with them. Guaranteed to be non-null.
  AccountsCookieMutator* GetAccountsCookieMutator();

  // Performs initalization that is dependent on the network being initialized.
  void OnNetworkInitialized();

  // Explicitly triggers the loading of accounts in the context of supervised
  // users.
  // TODO(https://crbug.com/860492): Eliminate the need to expose this.
  void LegacyLoadCredentialsForSupervisedUser(
      const std::string& primary_account_id);

  // Returns pointer to the object used to obtain diagnostics about the internal
  // state of IdentityManager.
  DiagnosticsProvider* GetDiagnosticsProvider();

  // Picks the correct account_id for the specified account depending on the
  // migration state.
  // NOTE: This method is added temporarily until when the delegate is moved
  // inside the component. So, do not call this method in normal usage.
  // TODO(https://crbug.com/922471): Remove the need to expose this method.
  std::string LegacyPickAccountIdForAccount(const std::string& gaia,
                                            const std::string& email) const;

  // Seeds the account whose account_id is given by
  // AccountTrackerService::PickAccountIdForAccount() with its corresponding
  // account information. Returns the same value PickAccountIdForAccount()
  // when given the same arguments.
  // NOTE: In normal usage, this method SHOULD NOT be called for getting the
  // account id. It's only for replacement of production code.
  std::string LegacySeedAccountInfo(const AccountInfo& info);

#if defined(OS_IOS)
  // Forces the processing of GaiaCookieManagerService::OnCookieChange. On
  // iOS, it's necessary to force-trigger the processing of cookie changes
  // from the client as the normal mechanism for internally observing them
  // is not wired up.
  // TODO(https://crbug.com/930582) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  void ForceTriggerOnCookieChange();

  // Adds a given account to the token service from a system account. This
  // API calls OAuth2TokenServiceDelegate::AddAccountFromSystem and it
  // triggers platform specific implementation for IOS.
  // NOTE: In normal usage, this method SHOULD NOT be called.
  // TODO(https://crbug.com/930094): Eliminate the need to expose this.
  void LegacyAddAccountFromSystem(const std::string& account_id);
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Reloads the accounts in the token service from the system accounts. This
  // API calls OAuth2TokenServiceDelegate::ReloadAccountsFromSystem and it
  // triggers platform specific implementation for Android and IOS.
  // NOTE: In normal usage, this method SHOULD NOT be called.
  // TODO(https://crbug.com/930094): Eliminate the need to expose this.
  void LegacyReloadAccountsFromSystem();
#endif

#if defined(OS_ANDROID)
  // Returns a pointer to the AccountTrackerService Java instance associated
  // with this object.
  // TODO(https://crbug.com/934688): Eliminate this method once
  // AccountTrackerService.java has no more client usage.
  base::android::ScopedJavaLocalRef<jobject>
  LegacyGetAccountTrackerServiceJavaObject();

  // Returns a pointer to the OAuth2TokenService Java instance associated
  // with this object.
  // TODO(https://crbug.com/934688): Eliminate this method once
  // OAuth2TokenService.java has no more client usage.
  base::android::ScopedJavaLocalRef<jobject>
  LegacyGetOAuth2TokenServiceJavaObject();

  // This method has the contractual assumption that the account is a known
  // account and has as its semantics that it fetches the account info for the
  // account, triggering an OnExtendedAccountInfoUpdated() callback if the info
  // was successfully fetched.
  void ForceRefreshOfExtendedAccountInfo(const std::string& refresh_token);
#endif

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

 private:
  // These test helpers need to use some of the private methods below.
  friend CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                           const std::string& email);
  friend void SetRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager,
      const std::string& token_value);
  friend void SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager);
  friend void RemoveRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager);
  friend AccountInfo MakePrimaryAccountAvailable(
      IdentityManager* identity_manager,
      const std::string& email);
  friend void ClearPrimaryAccount(IdentityManager* identity_manager,
                                  ClearPrimaryAccountPolicy policy);
  friend AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                          const std::string& email);
  friend void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                                        const std::string& account_id,
                                        const std::string& token_value);
  friend void SetInvalidRefreshTokenForAccount(
      IdentityManager* identity_manager,
      const std::string& account_id);
  friend void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                           const std::string& account_id);
  friend void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                          AccountInfo account_info);
  friend void SetFreshnessOfAccountsInGaiaCookie(
      IdentityManager* identity_manager,
      bool accounts_are_fresh);
  friend void UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManager* identity_manager,
      const std::string& account_id,
      const GoogleServiceAuthError& auth_error);

  friend void DisableAccessTokenFetchRetries(IdentityManager* identity_manager);

  friend void CancelAllOngoingGaiaCookieOperations(
      IdentityManager* identity_manager);

  friend void SetCookieAccounts(
      IdentityManager* identity_manager,
      network::TestURLLoaderFactory* test_url_loader_factory,
      const std::vector<identity::CookieParams>& cookie_accounts);

  // These clients needs to call SetPrimaryAccountSynchronously().
  friend ArcSupportHostTest;
  friend arc::ArcTermsOfServiceDefaultNegotiatorTest;
  friend chromeos::ChromeSessionManager;
  friend chromeos::UserSessionManager;
  friend IdentityManagerFactory;

  // Private getters used for testing only (i.e. see identity_test_utils.h).
  SigninManagerBase* GetSigninManager();
  ProfileOAuth2TokenService* GetTokenService();
  AccountTrackerService* GetAccountTrackerService();
  GaiaCookieManagerService* GetGaiaCookieManagerService();

  // Sets the primary account info synchronously with both the IdentityManager
  // and its backing SigninManager/ProfileOAuth2TokenService instances.
  // Prefer using the methods in identity_test_{environment, utils}.h to using
  // this method directly.
  void SetPrimaryAccountSynchronouslyForTests(const std::string& gaia_id,
                                              const std::string& email_address,
                                              const std::string& refresh_token);

  // Sets the primary account info synchronously with both the IdentityManager
  // and its backing SigninManager instance. If |refresh_token| is not empty,
  // sets the refresh token with the backing ProfileOAuth2TokenService
  // instance. This method should not be used directly; it exists only to serve
  // one legacy use case at this point.
  // TODO(https://crbug.com/814787): Eliminate the need for this method.
  void SetPrimaryAccountSynchronously(const std::string& gaia_id,
                                      const std::string& email_address,
                                      const std::string& refresh_token);

  // Populates and returns an AccountInfo object corresponding to |account_id|,
  // which must be an account with a refresh token.
  AccountInfo GetAccountInfoForAccountWithRefreshToken(
      const std::string& account_id) const;

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnEndBatchChanges() override;
  void OnAuthErrorChanged(const std::string& account_id,
                          const GoogleServiceAuthError& auth_error) override;

  // GaiaCookieManagerService::Observer:
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error) override;
  void OnGaiaCookieDeletedByUserAction() override;

  // OAuth2TokenService::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override;
  void OnFetchAccessTokenComplete(const std::string& account_id,
                                  const std::string& consumer_id,
                                  const ScopeSet& scopes,
                                  GoogleServiceAuthError error,
                                  base::Time expiration_time) override;
  void OnAccessTokenRemoved(const std::string& account_id,
                            const ScopeSet& scopes) override;
  void OnRefreshTokenAvailableFromSource(const std::string& account_id,
                                         bool is_refresh_token_valid,
                                         const std::string& source) override;
  void OnRefreshTokenRevokedFromSource(const std::string& account_id,
                                       const std::string& source) override;

  // AccountTrackerService::Observer:
  void OnAccountUpdated(const AccountInfo& info) override;
  void OnAccountRemoved(const AccountInfo& info) override;

  // Backing signin classes. NOTE: We strive to limit synchronous access to
  // these classes in the IdentityManager implementation, as all such
  // synchronous access will become impossible when IdentityManager is
  // backed by the Identity Service.
  SigninManagerBase* signin_manager_;
  ProfileOAuth2TokenService* token_service_;
  AccountFetcherService* account_fetcher_service_;
  AccountTrackerService* account_tracker_service_;
  GaiaCookieManagerService* gaia_cookie_manager_service_;

  // PrimaryAccountMutator instance. May be null if mutation of the primary
  // account state is not supported on the current platform.
  std::unique_ptr<PrimaryAccountMutator> primary_account_mutator_;

  // AccountsMutator instance. May be null if mutation of accounts is not
  // supported on the current platform.
  std::unique_ptr<AccountsMutator> accounts_mutator_;

  // AccountsCookieMutator instance. Guaranteed to be non-null, as this
  // functionality is supported on all platforms.
  std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator_;

  // DiagnosticsProvider instance.
  std::unique_ptr<DiagnosticsProvider> diagnostics_provider_;

  // Lists of observers.
  // Makes sure lists are empty on destruction.
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  base::ObserverList<DiagnosticsObserver, true>::Unchecked
      diagnostics_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManager);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_
