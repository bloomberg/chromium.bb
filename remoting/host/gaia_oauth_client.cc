// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gaia_oauth_client.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

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
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core>,
      public net::URLFetcherDelegate {
 public:
  Core(const OAuthProviderInfo& info,
       net::URLRequestContextGetter* request_context_getter)
      : provider_info_(info),
        request_context_getter_(request_context_getter),
        delegate_(NULL),
        url_fetcher_type_(URL_FETCHER_NONE) {
  }

  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    GaiaOAuthClient::Delegate* delegate);

  // net::URLFetcherDelegate interface
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;

  enum URLFetcherType {
    URL_FETCHER_NONE,
    URL_FETCHER_REFRESH_TOKEN,
    URL_FETCHER_GET_USER_INFO
  };

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
  scoped_ptr<net::URLFetcher> request_;
  URLFetcherType url_fetcher_type_;

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
  request_.reset(net::URLFetcher::Create(
      GURL(provider_info_.access_token_url), net::URLFetcher::POST, this));
  request_->SetRequestContext(request_context_getter_);
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);
  url_fetcher_type_ = URL_FETCHER_REFRESH_TOKEN;
  request_->Start();
}

void GaiaOAuthClient::Core::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::string response_string;
  source->GetResponseAsString(&response_string);
  switch (url_fetcher_type_) {
    case URL_FETCHER_REFRESH_TOKEN:
      OnAuthTokenFetchComplete(source->GetStatus(),
                              source->GetResponseCode(),
                              response_string);
      break;
    case URL_FETCHER_GET_USER_INFO:
      OnUserInfoFetchComplete(source->GetStatus(),
                              source->GetResponseCode(),
                              response_string);
      break;
    default:
      LOG(ERROR) << "Unrecognised URLFetcher type: " << url_fetcher_type_;
  }
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
      std::string access_token;
      response_dict->GetString(kAccessTokenValue, &access_token);
      if (access_token.find("\r\n") != std::string::npos) {
        LOG(ERROR) << "Gaia response: access token include CRLF";
        delegate_->OnOAuthError();
        return;
      }
      access_token_ = access_token;
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
  request_.reset(net::URLFetcher::Create(
      GURL(provider_info_.user_info_url), net::URLFetcher::GET, this));
  request_->SetRequestContext(request_context_getter_);
  request_->AddExtraRequestHeader("Authorization: Bearer " + access_token_);
  url_fetcher_type_ = URL_FETCHER_GET_USER_INFO;
  request_->Start();
}

void GaiaOAuthClient::Core::OnUserInfoFetchComplete(
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& response) {
  request_.reset();
  url_fetcher_type_ = URL_FETCHER_NONE;
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
