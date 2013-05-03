// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/accounts_client/test_accounts_client.h"

#include <algorithm>
#include <curl/curl.h>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/values.h"

using std::string;
using std::vector;

static const int kMaxSessionLifetimeSeconds = 30 * 60;
static const string kClaimPath = "claim";
static const string kReleasePath = "release";

AccountSession::AccountSession() {}
AccountSession::~AccountSession() {}

TestAccountsClient::TestAccountsClient(const string& server,
                                       const string& account_space,
                                       const vector<string>& usernames)
    : server_(server), account_space_(account_space), usernames_(usernames) {
}

TestAccountsClient::~TestAccountsClient() {}

AccountSession TestAccountsClient::ClaimAccount() {
  string post_fields;
  base::StringAppendF(&post_fields, "account_space=%s", account_space_.c_str());
  base::StringAppendF(&post_fields, "&max_lifetime_seconds=%d",
      kMaxSessionLifetimeSeconds);

  // TODO(pvalenzuela): Select N random usernames instead of all usernames.
  for (vector<string>::iterator it = usernames_.begin();
       it != usernames_.end(); ++it) {
    base::StringAppendF(&post_fields, "&username=%s", it->c_str());
  }

  string response = SendRequest(kClaimPath, post_fields);
  scoped_ptr<Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict_value;
  AccountSession session;
  if (value != NULL && value->GetAsDictionary(&dict_value) &&
      dict_value != NULL) {
    dict_value->GetString("username", &session.username);
    dict_value->GetString("account_space", &session.account_space);
    dict_value->GetString("session_id", &session.session_id);
    dict_value->GetString("expiration_time", &session.expiration_time);
  } else {
    session.error = response;
  }
  return session;
}

void TestAccountsClient::ReleaseAccount(const AccountSession& session) {
  string post_fields;
  // The expiration_time field is ignored since it isn't passed as part of the
  // release request.
  if (session.username.empty() || session.account_space.empty() ||
      account_space_.compare(session.account_space) != 0 ||
      session.session_id.empty()) {
    return;
  }

  base::StringAppendF(&post_fields, "account_space=%s",
      session.account_space.c_str());
  base::StringAppendF(&post_fields, "&username=%s", session.username.c_str());
  base::StringAppendF(&post_fields, "&session_id=%s",
      session.session_id.c_str());

  // This operation is best effort, so don't send any errors back to the caller.
  SendRequest(kReleasePath, post_fields);
}

namespace {
int CurlWriteFunction(char* data,
                      size_t size,
                      size_t nmemb,
                      string* write_data) {
  if (write_data == NULL) {
    return 0;
  }
  write_data->append(data, size * nmemb);
  return size * nmemb;
}
} // namespace

string TestAccountsClient::SendRequest(const string& path,
                                       const string& post_fields) {
  CURLcode res;
  string response_buffer;
  char error_buffer[CURL_ERROR_SIZE];
  CURL* curl = curl_easy_init();
  if (curl) {
    string url;
    base::SStringPrintf(&url, "%s/%s", server_.c_str(), path.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
      string error(error_buffer);
      return error;
    }
    return response_buffer;
  }
  return "There was an error establishing the connection.";
}
