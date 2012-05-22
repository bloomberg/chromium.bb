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
const char kAccessTokenValue[] = "access_token";
const char kRefreshTokenValue[] = "refresh_token";
const char kExpiresInValue[] = "expires_in";
}  // namespace

namespace remoting {

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core> {
 public:
  Core(const std::string& gaia_url,
       net::URLRequestContextGetter* request_context_getter)
      : gaia_url_(gaia_url),
        request_context_getter_(request_context_getter),
        delegate_(NULL) {
  }

  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    GaiaOAuthClient::Delegate* delegate);

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core() {}

  void OnUrlFetchComplete(const net::URLRequestStatus& status,
                          int response_code,
                          const std::string& response);

  GURL gaia_url_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  GaiaOAuthClient::Delegate* delegate_;
  scoped_ptr<UrlFetcher> request_;
};

void GaiaOAuthClient::Core::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    GaiaOAuthClient::Delegate* delegate) {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";

  std::string post_body =
      "refresh_token=" + net::EscapeUrlEncodedData(refresh_token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&grant_type=refresh_token";
  delegate_ = delegate;
  request_.reset(new UrlFetcher(gaia_url_, UrlFetcher::POST));
  request_->SetRequestContext(request_context_getter_);
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);
  request_->Start(base::Bind(&GaiaOAuthClient::Core::OnUrlFetchComplete, this));
}

void GaiaOAuthClient::Core::OnUrlFetchComplete(
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

  std::string access_token;
  std::string refresh_token;
  int expires_in_seconds = 0;
  if (response_code == net::HTTP_OK) {
    scoped_ptr<Value> message_value(base::JSONReader::Read(response));
    if (message_value.get() &&
        message_value->IsType(Value::TYPE_DICTIONARY)) {
      scoped_ptr<DictionaryValue> response_dict(
          static_cast<DictionaryValue*>(message_value.release()));
      response_dict->GetString(kAccessTokenValue, &access_token);
      response_dict->GetString(kRefreshTokenValue, &refresh_token);
      response_dict->GetInteger(kExpiresInValue, &expires_in_seconds);
    }
    VLOG(1) << "Gaia response: acess_token='" << access_token
            << "', refresh_token='" << refresh_token
            << "', expires in " << expires_in_seconds << " second(s)";
  } else {
    LOG(ERROR) << "Gaia response: response code=" << response_code;
  }

  if (access_token.empty()) {
    delegate_->OnNetworkError(response_code);
  } else if (refresh_token.empty()) {
    // If we only have an access token, then this was a refresh request.
    delegate_->OnRefreshTokenResponse(access_token, expires_in_seconds);
  }
}

GaiaOAuthClient::GaiaOAuthClient(const std::string& gaia_url,
                                 net::URLRequestContextGetter* context_getter) {
  core_ = new Core(gaia_url, context_getter);
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
