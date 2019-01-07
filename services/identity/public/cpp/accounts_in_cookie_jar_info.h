// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_IN_COOKIE_JAR_INFO_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_IN_COOKIE_JAR_INFO_H_

#include <vector>

#include "components/signin/core/browser/account_info.h"

namespace identity {

// Container for a response to get the accounts in the cookie jar.
struct AccountsInCookieJarInfo {
  // True if the accounts info from cookie is fresh and does not need to be
  // updated.
  bool accounts_are_fresh;

  // The current list of signed in accounts from the cookie jar.
  std::vector<AccountInfo> signed_in_accounts;

  // The current list of signed out accounts from the cookie jar.
  std::vector<AccountInfo> signed_out_accounts;

  AccountsInCookieJarInfo();
  AccountsInCookieJarInfo(
      bool accounts_are_fresh_param,
      const std::vector<AccountInfo>& signed_in_accounts_param,
      const std::vector<AccountInfo>& signed_out_accounts_param);
  AccountsInCookieJarInfo(const AccountsInCookieJarInfo& other);
  ~AccountsInCookieJarInfo();
};

}  // namespace identity

#endif  // _USR_LOCAL_GOOGLE_WORK_CHROMIUM_SRC_SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_IN_COOKIE_JAR_INFO_H_
