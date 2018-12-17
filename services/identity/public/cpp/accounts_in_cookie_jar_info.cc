// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_in_cookie_jar_info.h"

namespace identity {

AccountsInCookieJarInfo::AccountsInCookieJarInfo() = default;

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    bool accounts_are_fresh_param,
    const std::vector<AccountInfo>& accounts_param)
    : accounts_are_fresh(accounts_are_fresh_param), accounts(accounts_param) {}

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    const AccountsInCookieJarInfo& other) {
  if (this == &other)
    return;
  accounts_are_fresh = other.accounts_are_fresh;
  accounts = other.accounts;
}

AccountsInCookieJarInfo::~AccountsInCookieJarInfo() = default;

}  // namespace identity