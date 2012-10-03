// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gaia_user_email_fetcher.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
const char kOAuth2UserInfoUrl[] =
    "https://www.googleapis.com/oauth2/v1/userinfo";
}  // namespace

namespace remoting {

class GaiaUserEmailFetcher::Core
    : public base::RefCountedThreadSafe<GaiaUserEmailFetcher::Core>,
      public net::URLFetcherDelegate {
 public:
  Core(net::URLRequestContextGetter* request_context_getter)
      : request_context_getter_(request_context_getter),
        delegate_(NULL) {
  }

  void GetUserEmail(const std::string& oauth_access_token, Delegate* delegate);

  // net::URLFetcherDelegate interface
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;

  virtual ~Core() {}

  void FetchUserInfoAndInvokeCallback();
  void OnUserInfoFetchComplete(const net::URLRequestStatus& status,
                               int response_code,
                               const std::string& response);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  GaiaUserEmailFetcher::Delegate* delegate_;
  scoped_ptr<net::URLFetcher> request_;
};

void GaiaUserEmailFetcher::Core::GetUserEmail(
    const std::string& oauth_access_token, Delegate* delegate) {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";

  delegate_ = delegate;

  request_.reset(net::URLFetcher::Create(
      GURL(kOAuth2UserInfoUrl), net::URLFetcher::GET, this));
  request_->SetRequestContext(request_context_getter_);
  request_->AddExtraRequestHeader(
      "Authorization: OAuth " + oauth_access_token);
  request_->Start();
}

void GaiaUserEmailFetcher::Core::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::string response_string;
  source->GetResponseAsString(&response_string);
  OnUserInfoFetchComplete(source->GetStatus(),
                          source->GetResponseCode(),
                          response_string);
}

void GaiaUserEmailFetcher::Core::OnUserInfoFetchComplete(
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& response) {
  request_.reset();
  std::string email;
  if (response_code == net::HTTP_OK) {
    scoped_ptr<Value> response_value(base::JSONReader::Read(response));
    if (response_value.get()) {
      base::DictionaryValue* response_dict = NULL;
      if (response_value->GetAsDictionary(&response_dict)) {
        response_dict->GetString("email", &email);
      }
    }
  }
  if (email.empty()) {
    delegate_->OnNetworkError(response_code);
  } else {
    delegate_->OnGetUserEmailResponse(email);
  }
}

GaiaUserEmailFetcher::GaiaUserEmailFetcher(
    net::URLRequestContextGetter* context_getter) {
  core_ = new Core(context_getter);
}

GaiaUserEmailFetcher::~GaiaUserEmailFetcher() {
}

void GaiaUserEmailFetcher::GetUserEmail(
    const std::string& oauth_access_token,
    Delegate* delegate) {
  return core_->GetUserEmail(oauth_access_token, delegate);
}

}  // namespace remoting
