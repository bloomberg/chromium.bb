// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"
#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_auth_token_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"
#include "chrome/browser/extensions/api/identity/identity_launch_web_auth_flow_function.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/api/identity/identity_remove_cached_auth_token_function.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace extensions {

class IdentityTokenCacheValue {
 public:
  IdentityTokenCacheValue();
  IdentityTokenCacheValue(const IdentityTokenCacheValue& other);
  ~IdentityTokenCacheValue();

  static IdentityTokenCacheValue CreateIssueAdvice(
      const IssueAdviceInfo& issue_advice);
  static IdentityTokenCacheValue CreateRemoteConsent(
      const RemoteConsentResolutionData& resolution_data);
  static IdentityTokenCacheValue CreateRemoteConsentApproved(
      const std::string& consent_result);
  static IdentityTokenCacheValue CreateToken(const std::string& token,
                                             base::TimeDelta time_to_live);

  // Order of these entries is used to determine whether or not new
  // entries supersede older ones in SetCachedToken.
  enum CacheValueStatus {
    CACHE_STATUS_NOTFOUND,
    CACHE_STATUS_ADVICE,
    CACHE_STATUS_REMOTE_CONSENT,
    CACHE_STATUS_REMOTE_CONSENT_APPROVED,
    CACHE_STATUS_TOKEN
  };

  CacheValueStatus status() const;
  const IssueAdviceInfo& issue_advice() const;
  const RemoteConsentResolutionData& resolution_data() const;
  const std::string& consent_result() const;
  const std::string& token() const;
  const base::Time& expiration_time() const;

 private:
  bool is_expired() const;

  CacheValueStatus status_ = CACHE_STATUS_NOTFOUND;
  IssueAdviceInfo issue_advice_;
  RemoteConsentResolutionData resolution_data_;
  std::string consent_result_;
  std::string token_;
  base::Time expiration_time_;
};

class IdentityAPI : public BrowserContextKeyedAPI,
                    public signin::IdentityManager::Observer {
 public:
  using CachedTokens = std::map<ExtensionTokenKey, IdentityTokenCacheValue>;
  using OnSetConsentResultSignature = void(const std::string&,
                                           const std::string&);

  explicit IdentityAPI(content::BrowserContext* context);
  ~IdentityAPI() override;

  // Request serialization queue for getAuthToken.
  IdentityMintRequestQueue* mint_queue();

  // Token cache.
  void SetCachedToken(const ExtensionTokenKey& key,
                      const IdentityTokenCacheValue& token_data);
  void EraseCachedToken(const std::string& extension_id,
                        const std::string& token);
  void EraseAllCachedTokens();
  const IdentityTokenCacheValue& GetCachedToken(const ExtensionTokenKey& key);
  const CachedTokens& GetAllCachedTokens();

  // GAIA id cache.
  void SetGaiaIdForExtension(const std::string& extension_id,
                             const std::string& gaia_id);
  // Returns |base::nullopt| if no GAIA id is saved for |extension_id|.
  // Otherwise, returns GAIA id previously saved via SetGaiaIdForExtension().
  base::Optional<std::string> GetGaiaIdForExtension(
      const std::string& extension_id);
  void EraseGaiaIdForExtension(const std::string& extension_id);
  // If refresh tokens have been loaded, erases GAIA ids of accounts that are no
  // longer signed in to Chrome for all extensions.
  void EraseStaleGaiaIdsForAllExtensions();

  // Consent result.
  void SetConsentResult(const std::string& result,
                        const std::string& window_id);
  std::unique_ptr<base::CallbackList<OnSetConsentResultSignature>::Subscription>
  RegisterOnSetConsentResultCallback(
      const base::RepeatingCallback<OnSetConsentResultSignature>& callback);

  // BrowserContextKeyedAPI:
  void Shutdown() override;
  static BrowserContextKeyedAPIFactory<IdentityAPI>* GetFactoryInstance();

  std::unique_ptr<base::CallbackList<void()>::Subscription>
  RegisterOnShutdownCallback(const base::Closure& cb);

  // Callback that is used in testing contexts to test the implementation of
  // the chrome.identity.onSignInChanged event. Note that the passed-in Event is
  // valid only for the duration of the callback.
  using OnSignInChangedCallback = base::RepeatingCallback<void(Event*)>;
  void set_on_signin_changed_callback_for_testing(
      const OnSignInChangedCallback& callback) {
    on_signin_changed_callback_for_testing_ = callback;
  }

  // Whether the chrome.identity API should use all accounts or the primary
  // account only.
  bool AreExtensionsRestrictedToPrimaryAccount();

 private:
  friend class BrowserContextKeyedAPIFactory<IdentityAPI>;
  friend class IdentityAPITest;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "IdentityAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  // This constructor allows to mock keyed services in tests.
  IdentityAPI(Profile* profile,
              signin::IdentityManager* identity_manager,
              ExtensionPrefs* extension_prefs,
              EventRouter* event_router);

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  // NOTE: This class must listen for this callback rather than
  // OnRefreshTokenRemovedForAccount() to obtain the Gaia ID of the removed
  // account.
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // Fires the chrome.identity.onSignInChanged event.
  void FireOnAccountSignInChanged(const std::string& gaia_id,
                                  bool is_signed_in);

  Profile* const profile_;
  signin::IdentityManager* const identity_manager_;
  ExtensionPrefs* const extension_prefs_;
  EventRouter* const event_router_;

  IdentityMintRequestQueue mint_queue_;
  CachedTokens token_cache_;

  OnSignInChangedCallback on_signin_changed_callback_for_testing_;

  base::CallbackList<OnSetConsentResultSignature>
      on_set_consent_result_callback_list_;
  base::CallbackList<void()> on_shutdown_callback_list_;
};

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
