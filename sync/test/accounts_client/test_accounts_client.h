// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ACCOUNTS_CLIENT_TEST_ACCOUNTS_CLIENT_H_
#define SYNC_TEST_ACCOUNTS_CLIENT_TEST_ACCOUNTS_CLIENT_H_

#include <curl/curl.h>
#include <string>
#include <vector>

using std::string;
using std::vector;

// The data associated with an account session.
struct AccountSession {
  AccountSession();
  ~AccountSession();

  string username;
  string account_space;
  string session_id;
  string expiration_time;

  // Only set if there was an error.
  string error;
};

// A test-side client for the Test Accounts service. This service provides
// short-term, exclusive access to test accounts for the purpose of testing
// against real Chrome Sync servers.
class TestAccountsClient {
 public:
  // Creates a client associated with the given |server| URL (e.g.,
  // http://service-runs-here.com), |account_space| (for account segregation),
  // and |usernames| (the collection of accounts to be chosen from).
  TestAccountsClient(const string& server,
                     const string& account_space,
                     const vector<string>& usernames);

  virtual ~TestAccountsClient();

  // Attempts to claim an account via the Test Accounts service. If
  // successful, an AccountSession is returned containing the data associated
  // with the session. If an error occurred, then the AccountSession will only
  // have its error field set.
  AccountSession ClaimAccount();

  // Attempts to release an account via the Test Accounts service. The value
  // of |session| should be one returned from ClaimAccount(). This function
  // is best-effort and fails silently.
  void ReleaseAccount(const AccountSession& session);

  // Sends an HTTP POST request to the Test Accounts service.
  virtual string SendRequest(const string& path, const string& post_fields);

 private:
  const string server_;
  const string account_space_;
  vector<string> usernames_;
};

#endif  // SYNC_TEST_ACCOUNTS_CLIENT_TEST_ACCOUNTS_CLIENT_H_
