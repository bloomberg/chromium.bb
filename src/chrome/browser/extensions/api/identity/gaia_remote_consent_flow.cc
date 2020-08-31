// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

GaiaRemoteConsentFlow::Delegate::~Delegate() = default;

GaiaRemoteConsentFlow::GaiaRemoteConsentFlow(
    Delegate* delegate,
    Profile* profile,
    const ExtensionTokenKey& token_key,
    const RemoteConsentResolutionData& resolution_data)
    : delegate_(delegate),
      profile_(profile),
      account_id_(token_key.account_id),
      resolution_data_(resolution_data),
      web_flow_started_(false),
      scoped_observer_(this) {}

GaiaRemoteConsentFlow::~GaiaRemoteConsentFlow() {
  if (web_flow_)
    web_flow_.release()->DetachDelegateAndDelete();
}

void GaiaRemoteConsentFlow::Start() {
  if (!web_flow_) {
    web_flow_ = std::make_unique<WebAuthFlow>(
        this, profile_, resolution_data_.url, WebAuthFlow::INTERACTIVE,
        WebAuthFlow::GET_AUTH_TOKEN);
  }

  SetAccountsInCookie();
}

void GaiaRemoteConsentFlow::OnSetAccountsComplete(
    signin::SetAccountsInCookieResult result) {
  set_accounts_in_cookie_task_.reset();
  if (web_flow_started_) {
    return;
  }

  if (result != signin::SetAccountsInCookieResult::kSuccess) {
    delegate_->OnGaiaRemoteConsentFlowFailed(
        GaiaRemoteConsentFlow::Failure::SET_ACCOUNTS_IN_COOKIE_FAILED);
    return;
  }

  network::mojom::CookieManager* cookie_manager =
      GetCookieManagerForPartition();
  net::CookieOptions options;
  for (const auto& cookie : resolution_data_.cookies) {
    cookie_manager->SetCanonicalCookie(
        cookie,
        net::cookie_util::SimulatedCookieSource(cookie, url::kHttpsScheme),
        options, network::mojom::CookieManager::SetCanonicalCookieCallback());
  }

  identity_api_set_consent_result_subscription_ =
      IdentityAPI::GetFactoryInstance()
          ->Get(profile_)
          ->RegisterOnSetConsentResultCallback(
              base::Bind(&GaiaRemoteConsentFlow::OnConsentResultSet,
                         base::Unretained(this)));

  scoped_observer_.Add(IdentityManagerFactory::GetForProfile(profile_));
  web_flow_->Start();
  web_flow_started_ = true;
}

void GaiaRemoteConsentFlow::OnConsentResultSet(
    const std::string& consent_result,
    const std::string& window_id) {
  if (!web_flow_ || window_id != web_flow_->GetAppWindowKey())
    return;

  identity_api_set_consent_result_subscription_.reset();

  bool consent_approved = false;
  std::string gaia_id;
  if (!gaia::ParseOAuth2MintTokenConsentResult(consent_result,
                                               &consent_approved, &gaia_id)) {
    delegate_->OnGaiaRemoteConsentFlowFailed(
        GaiaRemoteConsentFlow::INVALID_CONSENT_RESULT);
    return;
  }

  if (!consent_approved) {
    delegate_->OnGaiaRemoteConsentFlowFailed(GaiaRemoteConsentFlow::NO_GRANT);
    return;
  }

  delegate_->OnGaiaRemoteConsentFlowApproved(consent_result, gaia_id);
}

void GaiaRemoteConsentFlow::OnAuthFlowFailure(WebAuthFlow::Failure failure) {
  GaiaRemoteConsentFlow::Failure gaia_failure;

  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      gaia_failure = GaiaRemoteConsentFlow::WINDOW_CLOSED;
      break;
    case WebAuthFlow::LOAD_FAILED:
      gaia_failure = GaiaRemoteConsentFlow::LOAD_FAILED;
      break;
    case WebAuthFlow::INTERACTION_REQUIRED:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      gaia_failure = GaiaRemoteConsentFlow::LOAD_FAILED;
      break;
  }

  delegate_->OnGaiaRemoteConsentFlowFailed(gaia_failure);
}

std::unique_ptr<GaiaAuthFetcher>
GaiaRemoteConsentFlow::CreateGaiaAuthFetcherForPartition(
    GaiaAuthConsumer* consumer) {
  return std::make_unique<GaiaAuthFetcher>(
      consumer, gaia::GaiaSource::kChrome,
      web_flow_->GetGuestPartition()->GetURLLoaderFactoryForBrowserProcess());
}

network::mojom::CookieManager*
GaiaRemoteConsentFlow::GetCookieManagerForPartition() {
  return web_flow_->GetGuestPartition()->GetCookieManagerForBrowserProcess();
}

void GaiaRemoteConsentFlow::OnEndBatchOfRefreshTokenStateChanges() {
  SetAccountsInCookie();
}

void GaiaRemoteConsentFlow::SetWebAuthFlowForTesting(
    std::unique_ptr<WebAuthFlow> web_auth_flow) {
  if (web_flow_)
    web_flow_.release()->DetachDelegateAndDelete();
  web_flow_ = std::move(web_auth_flow);
}

void GaiaRemoteConsentFlow::SetAccountsInCookie() {
  // Reset a task that is already in flight because it contains stale
  // information.
  if (set_accounts_in_cookie_task_)
    set_accounts_in_cookie_task_.reset();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  std::vector<CoreAccountId> accounts;
  if (IdentityAPI::GetFactoryInstance()
          ->Get(profile_)
          ->AreExtensionsRestrictedToPrimaryAccount()) {
    CoreAccountId primary_account_id = identity_manager->GetPrimaryAccountId();
    accounts.push_back(primary_account_id);
  } else {
    auto chrome_accounts_with_refresh_tokens =
        identity_manager->GetAccountsWithRefreshTokens();
    for (const auto& chrome_account : chrome_accounts_with_refresh_tokens) {
      // An account in persistent error state would make multilogin fail.
      // Showing only a subset of accounts seems to be a better alternative than
      // failing with an error.
      if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              chrome_account.account_id)) {
        continue;
      }
      accounts.push_back(chrome_account.account_id);
    }
  }

  // base::Unretained() is safe here because this class owns
  // |set_accounts_in_cookie_task_| that will eventually invoke this callback.
  set_accounts_in_cookie_task_ =
      identity_manager->GetAccountsCookieMutator()
          ->SetAccountsInCookieForPartition(
              this,
              {gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
               accounts},
              base::BindOnce(&GaiaRemoteConsentFlow::OnSetAccountsComplete,
                             base::Unretained(this)));
}

}  // namespace extensions
