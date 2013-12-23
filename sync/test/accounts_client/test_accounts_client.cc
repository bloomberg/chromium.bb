// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/accounts_client/test_accounts_client.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/test/accounts_client/url_request_context_getter.h"

using std::string;
using std::vector;

static const int kMaxSessionLifetimeSeconds = 30 * 60;
static const string kClaimPath = "claim";
static const string kReleasePath = "release";
static const base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(10);

AccountSession::AccountSession() {}
AccountSession::~AccountSession() {}

class AccountsRequestDelegate : public net::URLFetcherDelegate {
 public:
  AccountsRequestDelegate(base::RunLoop* run_loop) : response_(""),
      success_(false), run_loop_(run_loop) {}

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    string url = source->GetURL().spec();
    source->GetResponseAsString(&response_);

    if (!source->GetStatus().is_success()) {
      int error = source->GetStatus().error();
      DVLOG(0) << "The request failed with error code " << error << "."
               << "\nRequested URL: " << url << ".";
    } else if (source->GetResponseCode() != net::HTTP_OK) {
      DVLOG(0) << "The request failed with response code "
               << source->GetResponseCode() << "."
               << "\nRequested URL: " << url
               << "\nResponse body: \"" << response_ << "\"";
    } else {
      success_ = true;
    }

    run_loop_->Quit();
  }

  string response() const { return response_; }
  bool success() const { return success_; }

 private:
  string response_;
  bool success_;
  base::RunLoop* run_loop_;
};

TestAccountsClient::TestAccountsClient(const string& server,
                                       const string& account_space,
                                       const vector<string>& usernames)
    : server_(server), account_space_(account_space), usernames_(usernames) {
}

TestAccountsClient::~TestAccountsClient() {}

bool TestAccountsClient::ClaimAccount(AccountSession* session) {
  GURL url = CreateGURLWithPath(kClaimPath);
  url = net::AppendQueryParameter(url, "account_space", account_space_);
  string max_lifetime_seconds = base::StringPrintf("%d",
                                                   kMaxSessionLifetimeSeconds);
  url = net::AppendQueryParameter(url, "max_lifetime_seconds",
                                  max_lifetime_seconds);

  // TODO(pvalenzuela): Select N random usernames instead of all usernames.
  for (vector<string>::iterator it = usernames_.begin();
       it != usernames_.end(); ++it) {
    url = net::AppendQueryParameter(url, "username", *it);
  }

  string response;
  if (!SendRequest(url, &response)) {
    return false;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict_value;
  if (value != NULL && value->GetAsDictionary(&dict_value) &&
      dict_value != NULL) {
    dict_value->GetString("username", &session->username);
    dict_value->GetString("account_space", &session->account_space);
    dict_value->GetString("session_id", &session->session_id);
    dict_value->GetString("expiration_time", &session->expiration_time);
  } else {
    return false;
  }

  return true;
}

void TestAccountsClient::ReleaseAccount(const AccountSession& session) {
  // The expiration_time field is ignored since it isn't passed as part of the
  // release request.
  if (session.username.empty() || session.account_space.empty() ||
      account_space_.compare(session.account_space) != 0 ||
      session.session_id.empty()) {
    return;
  }

  GURL url = CreateGURLWithPath(kReleasePath);
  url = net::AppendQueryParameter(url, "account_space", session.account_space);
  url = net::AppendQueryParameter(url, "username", session.username);
  url = net::AppendQueryParameter(url, "session_id", session.session_id);

  // This operation is best effort, so don't send any errors back to the caller.
  string response;
  SendRequest(url, &response);
}

GURL TestAccountsClient::CreateGURLWithPath(const string& path) {
  return GURL(base::StringPrintf("%s/%s", server_.c_str(), path.c_str()));
}


bool TestAccountsClient::SendRequest(const GURL& url, string* response) {
  base::MessageLoop* loop = base::MessageLoop::current();
  scoped_refptr<URLRequestContextGetter> context_getter(
      new URLRequestContextGetter(loop->message_loop_proxy()));

  base::RunLoop run_loop;

  AccountsRequestDelegate delegate(&run_loop);
  scoped_ptr<net::URLFetcher> fetcher(net::URLFetcher::Create(
          url, net::URLFetcher::POST, &delegate));
  fetcher->SetRequestContext(context_getter.get());
  fetcher->SetUploadData("application/json", "");
  fetcher->Start();

  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
    run_loop.QuitClosure(),
    kRequestTimeout);
  run_loop.Run();

  if (delegate.success()) {
    *response = delegate.response();
  }

  return delegate.success();
}
