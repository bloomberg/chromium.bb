// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_oauth2_token_service_delegate.h"

#include <vector>

#include "content/public/browser/storage_partition.h"

namespace extensions {

ShellOAuth2TokenServiceDelegate::ShellOAuth2TokenServiceDelegate(
    std::string account_id,
    std::string refresh_token)
    : account_id_(account_id), refresh_token_(refresh_token) {}

ShellOAuth2TokenServiceDelegate::~ShellOAuth2TokenServiceDelegate() {
}

bool ShellOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  if (account_id != account_id_)
    return false;

  return !refresh_token_.empty();
}

OAuth2AccessTokenFetcher*
ShellOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK_EQ(account_id, account_id_);
  DCHECK(!refresh_token_.empty());
  return new OAuth2AccessTokenFetcherImpl(consumer, url_loader_factory,
                                          refresh_token_);
}

std::vector<std::string> ShellOAuth2TokenServiceDelegate::GetAccounts() {
  std::vector<std::string> accounts;
  accounts.push_back(account_id_);
  return accounts;
}

void ShellOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  account_id_ = account_id;
  refresh_token_ = refresh_token;
}

}  // namespace extensions
