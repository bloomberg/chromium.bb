// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/ubertoken_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace extensions {

GaiaWebAuthFlow::GaiaWebAuthFlow(Delegate* delegate,
                                 Profile* profile,
                                 const ExtensionTokenKey* token_key,
                                 const std::string& oauth2_client_id,
                                 const std::string& locale)
    : delegate_(delegate),
      profile_(profile),
      account_id_(token_key->account_id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "identity", "GaiaWebAuthFlow", this, "extension_id",
      token_key->extension_id, "account_id", token_key->account_id.ToString());

  const char kOAuth2RedirectPathFormat[] = "/%s";
  const char kOAuth2AuthorizeFormat[] =
      "?response_type=token&approval_prompt=force&authuser=0&"
      "client_id=%s&"
      "scope=%s&"
      "origin=chrome-extension://%s/&"
      "redirect_uri=%s:/%s&"
      "hl=%s";
  // Additional parameters to pass if device_id is enabled.
  const char kOAuth2AuthorizeFormatDeviceIdAddendum[] =
      "&device_id=%s&"
      "device_type=chrome";

  std::vector<base::StringPiece> scopes(token_key->scopes.begin(),
                                        token_key->scopes.end());
  std::vector<base::StringPiece> client_id_parts = base::SplitStringPiece(
      oauth2_client_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(client_id_parts.begin(), client_id_parts.end());
  redirect_scheme_ = base::JoinString(client_id_parts, ".");
  std::string signin_scoped_device_id;
  if (profile_)
    signin_scoped_device_id = GetSigninScopedDeviceIdForProfile(profile_);

  redirect_path_prefix_ = base::StringPrintf(kOAuth2RedirectPathFormat,
                                             token_key->extension_id.c_str());

  std::string oauth2_authorize_params = base::StringPrintf(
      kOAuth2AuthorizeFormat,
      oauth2_client_id.c_str(),
      net::EscapeUrlEncodedData(base::JoinString(scopes, " "), true).c_str(),
      token_key->extension_id.c_str(),
      redirect_scheme_.c_str(),
      token_key->extension_id.c_str(),
      locale.c_str());
  if (!signin_scoped_device_id.empty()) {
    oauth2_authorize_params += base::StringPrintf(
        kOAuth2AuthorizeFormatDeviceIdAddendum,
        net::EscapeUrlEncodedData(signin_scoped_device_id, true).c_str());
  }
  auth_url_ = GaiaUrls::GetInstance()->oauth2_auth_url().Resolve(
      oauth2_authorize_params);
}

GaiaWebAuthFlow::~GaiaWebAuthFlow() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("identity", "GaiaWebAuthFlow", this);

  if (web_flow_)
    web_flow_.release()->DetachDelegateAndDelete();
}

void GaiaWebAuthFlow::Start() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("identity", "UbertokenFetch", this);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  ubertoken_fetcher_ = identity_manager->CreateUbertokenFetcherForAccount(
      account_id_,
      base::BindOnce(&GaiaWebAuthFlow::OnUbertokenFetchComplete,
                     base::Unretained(this)),
      gaia::GaiaSource::kChrome, profile_->GetURLLoaderFactory());
}

void GaiaWebAuthFlow::OnUbertokenFetchComplete(GoogleServiceAuthError error,
                                               const std::string& token) {
  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    TRACE_EVENT_NESTABLE_ASYNC_END1("identity", "UbertokenFetch", this, "error",
                                    error.ToString());

    DVLOG(1) << "OnUbertokenFetchComplete failure: " << error.error_message();
    delegate_->OnGaiaFlowFailure(GaiaWebAuthFlow::SERVICE_AUTH_ERROR, error,
                                 std::string());
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("identity", "UbertokenFetch", this);

  const char kMergeSessionQueryFormat[] = "?uberauth=%s&"
                                          "continue=%s&"
                                          "source=appsv2";

  std::string merge_query = base::StringPrintf(
      kMergeSessionQueryFormat,
      net::EscapeUrlEncodedData(token, true).c_str(),
      net::EscapeUrlEncodedData(auth_url_.spec(), true).c_str());
  GURL merge_url(
      GaiaUrls::GetInstance()->merge_session_url().Resolve(merge_query));

  web_flow_ = CreateWebAuthFlow(merge_url);
  network::mojom::CookieManager* cookie_manager =
      web_flow_->GetGuestPartition()->GetCookieManagerForBrowserProcess();
  cookie_manager->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(),
      base::BindOnce(&GaiaWebAuthFlow::OnCookiesDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GaiaWebAuthFlow::OnCookiesDeleted(uint32_t num_deleted) {
  web_flow_->Start();
}

void GaiaWebAuthFlow::OnAuthFlowFailure(WebAuthFlow::Failure failure) {
  GaiaWebAuthFlow::Failure gaia_failure;

  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      gaia_failure = GaiaWebAuthFlow::WINDOW_CLOSED;
      break;
    case WebAuthFlow::LOAD_FAILED:
      DVLOG(1) << "OnAuthFlowFailure LOAD_FAILED";
      gaia_failure = GaiaWebAuthFlow::LOAD_FAILED;
      break;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      gaia_failure = GaiaWebAuthFlow::LOAD_FAILED;
      break;
  }

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("identity", "OnAuthFlowFailure", this,
                                      "error", gaia_failure);

  delegate_->OnGaiaFlowFailure(
      gaia_failure,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      std::string());
}

void GaiaWebAuthFlow::OnAuthFlowURLChange(const GURL& url) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("identity", "OnAuthFlowURLChange", this);

  const char kOAuth2RedirectAccessTokenKey[] = "access_token";
  const char kOAuth2RedirectErrorKey[] = "error";
  const char kOAuth2ExpiresInKey[] = "expires_in";

  // The format of the target URL is:
  //     reversed.oauth.client.id:/extensionid#access_token=TOKEN

  if (url.scheme() == redirect_scheme_ && !url.has_host() && !url.has_port() &&
      base::StartsWith(url.GetContent(), redirect_path_prefix_,
                       base::CompareCase::SENSITIVE) &&
      url.has_ref()) {
    web_flow_.release()->DetachDelegateAndDelete();

    std::string fragment = url.ref();
    base::StringPairs pairs;
    base::SplitStringIntoKeyValuePairs(fragment, '=', '&', &pairs);
    std::string access_token;
    std::string error;
    std::string expiration;

    for (auto it = pairs.begin(); it != pairs.end(); ++it) {
      if (it->first == kOAuth2RedirectAccessTokenKey)
        access_token = it->second;
      else if (it->first == kOAuth2RedirectErrorKey)
        error = it->second;
      else if (it->first == kOAuth2ExpiresInKey)
        expiration = it->second;
    }

    if (access_token.empty() && error.empty()) {
      delegate_->OnGaiaFlowFailure(
          GaiaWebAuthFlow::INVALID_REDIRECT,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          std::string());
    } else if (!error.empty()) {
      delegate_->OnGaiaFlowFailure(
          GaiaWebAuthFlow::OAUTH_ERROR,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          error);
    } else {
      delegate_->OnGaiaFlowCompleted(access_token, expiration);
    }
  }
}

void GaiaWebAuthFlow::OnAuthFlowTitleChange(const std::string& title) {
  // On the final page the title will be "Loading <redirect-url>".
  // Treat it as though we'd really been redirected to <redirect-url>.
  const char kRedirectPrefix[] = "Loading ";
  std::string prefix(kRedirectPrefix);

  if (base::StartsWith(title, prefix, base::CompareCase::SENSITIVE)) {
    GURL url(title.substr(prefix.length(), std::string::npos));
    if (url.is_valid())
      OnAuthFlowURLChange(url);
  }
}

std::unique_ptr<WebAuthFlow> GaiaWebAuthFlow::CreateWebAuthFlow(GURL url) {
  return std::unique_ptr<WebAuthFlow>(
      new WebAuthFlow(this, profile_, url, WebAuthFlow::INTERACTIVE,
                      WebAuthFlow::GET_AUTH_TOKEN));
}

}  // namespace extensions
