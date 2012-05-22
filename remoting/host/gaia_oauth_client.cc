// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gaia_oauth_client.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "remoting/host/url_fetcher.h"

namespace {

const char kDefaultOAuth2TokenUrl[] =
    "https://accounts.google.com/o/oauth2/token";
const char kDefaultOAuth2UserInfoUrl[] =
    "https://www.googleapis.com/oauth2/v1/userinfo";

// Values used to parse token response.
const char kAccessTokenValue[] = "access_token";
const char kRefreshTokenValue[] = "refresh_token";
const char kExpiresInValue[] = "expires_in";

// Values used when parsing userinfo response.
const char kEmailValue[] = "email";

}  // namespace

namespace remoting {

// static
OAuthProviderInfo OAuthProviderInfo::GetDefault() {
  OAuthProviderInfo result;
  result.access_token_url = kDefaultOAuth2TokenUrl;
  result.user_info_url = kDefaultOAuth2UserInfoUrl;
  return result;
}

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core> {
 public:
  Core(const OAuthProviderInfo& info,
       net::URLRequestContextGetter* request_context_getter)
      : request_context_getter_(request_context_getter),
        delegate_(NULL) {
  }

  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    GaiaOAuthClient::Delegate* delegate);

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core() {}

  void OnAuthTokenFetchComplete(const net::URLRequestStatus& status,
                                int response_code,
                                const std::string& response);
  void FetchUserInfoAndInvokeCallback();
  void OnUserInfoFetchComplete(const net::URLRequestStatus& status,
                               int response_code,
                               const std::string& response);

  OAuthProviderInfo provider_info_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  GaiaOAuthClient::Delegate* delegate_;
  scoped_ptr<UrlFetcher> request_;

  std::string access_token_;
  int expires_in_seconds_;
};

void GaiaOAuthClient::Core::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    GaiaOAuthClient::Delegate* delegate) {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";

  delegate_ = delegate;

  access_token_.clear();
  expires_in_seconds_ = 0;

  std::string post_body =
      "refresh_token=" + net::EscapeUrlEncodedData(refresh_token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&grant_type=refresh_token";
  request_.reset(new UrlFetcher(GURL(provider_info_.access_token_url),
                                UrlFetcher::POST));
  request_->SetRequestContext(request_context_getter_);
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);
  request_->Start(
      base::Bind(&GaiaOAuthClient::Core::OnAuthTokenFetchComplete, this));
}

void GaiaOAuthClient::Core::OnAuthTokenFetchComplete(
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& response) {
  request_.reset();

  if (!status.is_success()) {
    delegate_->OnNetworkError(response_code);
    return;
  }

  // HTTP_BAD_REQUEST means the arguments are invalid.
  if (response_code == net::HTTP_BAD_REQUEST) {
    LOG(ERROR) << "Gaia response: response code=net::HTTP_BAD_REQUEST.";
    delegate_->OnOAuthError();
    return;
  }

  if (response_code == net::HTTP_OK) {
    scoped_ptr<Value> message_value(base::JSONReader::Read(response));
    if (message_value.get() &&
        message_value->IsType(Value::TYPE_DICTIONARY)) {
      scoped_ptr<DictionaryValue> response_dict(
          static_cast<DictionaryValue*>(message_value.release()));
      response_dict->GetString(kAccessTokenValue, &access_token_);
      response_dict->GetInteger(kExpiresInValue, &expires_in_seconds_);
    }
    VLOG(1) << "Gaia response: acess_token='" << access_token_
            << "', expires in " << expires_in_seconds_ << " second(s)";
  } else {
    LOG(ERROR) << "Gaia response: response code=" << response_code;
  }

  if (access_token_.empty()) {
    delegate_->OnNetworkError(response_code);
  } else {
    FetchUserInfoAndInvokeCallback();
  }
}

void GaiaOAuthClient::Core::FetchUserInfoAndInvokeCallback() {
  request_.reset(new UrlFetcher(
      GURL(provider_info_.user_info_url), UrlFetcher::GET));
  request_->SetRequestContext(request_context_getter_);
  request_->SetHeader("Authorization", "Bearer " + access_token_);
  request_->Start(
      base::Bind(&GaiaOAuthClient::Core::OnUserInfoFetchComplete, this));
}

void GaiaOAuthClient::Core::OnUserInfoFetchComplete(
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& response) {
  std::string email;
  if (response_code == net::HTTP_OK) {
    scoped_ptr<Value> message_value(base::JSONReader::Read(response));
    if (message_value.get() &&
        message_value->IsType(Value::TYPE_DICTIONARY)) {
      scoped_ptr<DictionaryValue> response_dict(
          static_cast<DictionaryValue*>(message_value.release()));
      response_dict->GetString(kEmailValue, &email);
    }
  }

  if (email.empty()) {
    delegate_->OnNetworkError(response_code);
  } else {
    delegate_->OnRefreshTokenResponse(
        email, access_token_, expires_in_seconds_);
  }
}

GaiaOAuthClient::GaiaOAuthClient(const OAuthProviderInfo& provider_info,
                                 net::URLRequestContextGetter* context_getter) {
  core_ = new Core(provider_info, context_getter);
}

GaiaOAuthClient::~GaiaOAuthClient() {
}

void GaiaOAuthClient::RefreshToken(const OAuthClientInfo& oauth_client_info,
                                   const std::string& refresh_token,
                                   Delegate* delegate) {
  return core_->RefreshToken(oauth_client_info,
                             refresh_token,
                             delegate);
}

}  // namespace remoting
