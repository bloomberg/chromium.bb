// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

namespace {
// Values used from |MutableProfileOAuth2TokenServiceDelegate|.
const net::BackoffEntry::Policy kBackoffPolicy = {
    0 /* int num_errors_to_ignore */,

    1000 /* int initial_delay_ms */,

    2.0 /* double multiply_factor */,

    0.2 /* double jitter_factor */,

    15 * 60 * 1000 /* int64_t maximum_backoff_ms */,

    -1 /* int64_t entry_lifetime_ms */,

    false /* bool always_use_initial_delay */,
};
}  // namespace

FakeOAuth2TokenServiceDelegate::AccountInfo::AccountInfo(
    const std::string& refresh_token)
    : refresh_token(refresh_token),
      error(GoogleServiceAuthError::NONE) {}

FakeOAuth2TokenServiceDelegate::FakeOAuth2TokenServiceDelegate()
    : shared_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      backoff_entry_(&kBackoffPolicy) {}

FakeOAuth2TokenServiceDelegate::~FakeOAuth2TokenServiceDelegate() {
}

OAuth2AccessTokenFetcher*
FakeOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  auto it = refresh_tokens_.find(account_id);
  DCHECK(it != refresh_tokens_.end());
  return new OAuth2AccessTokenFetcherImpl(consumer, url_loader_factory,
                                          it->second->refresh_token);
}

bool FakeOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  return !GetRefreshToken(account_id).empty();
}

GoogleServiceAuthError FakeOAuth2TokenServiceDelegate::GetAuthError(
    const std::string& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  return (it == refresh_tokens_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                                       : it->second->error;
}

std::string FakeOAuth2TokenServiceDelegate::GetRefreshToken(
    const std::string& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  if (it != refresh_tokens_.end())
    return it->second->refresh_token;
  return std::string();
}

const net::BackoffEntry* FakeOAuth2TokenServiceDelegate::BackoffEntry() const {
  return &backoff_entry_;
}

std::vector<std::string> FakeOAuth2TokenServiceDelegate::GetAccounts() {
  std::vector<std::string> account_ids;
  for (const auto& token : refresh_tokens_)
    account_ids.push_back(token.first);
  return account_ids;
}

void FakeOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  std::vector<std::string> account_ids = GetAccounts();
  for (const auto& account : account_ids)
    RevokeCredentials(account);
}

void FakeOAuth2TokenServiceDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  FireRefreshTokensLoaded();
}

void FakeOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  IssueRefreshTokenForUser(account_id, refresh_token);
}

void FakeOAuth2TokenServiceDelegate::IssueRefreshTokenForUser(
    const std::string& account_id,
    const std::string& token) {
  ScopedBatchChange batch(this);
  if (token.empty()) {
    refresh_tokens_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  } else {
    refresh_tokens_[account_id].reset(new AccountInfo(token));
    // If the token is a special "invalid" value, then that means the token was
    // rejected by the client and is thus not valid. So set the appropriate
    // error in that case. This logic is essentially duplicated from
    // MutableProfileOAuth2TokenServiceDelegate.
    if (token == kInvalidRefreshToken) {
      refresh_tokens_[account_id]->error =
          GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
              GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                  CREDENTIALS_REJECTED_BY_CLIENT);
    }
    FireRefreshTokenAvailable(account_id);
  }
}

void FakeOAuth2TokenServiceDelegate::RevokeCredentials(
    const std::string& account_id) {
  IssueRefreshTokenForUser(account_id, std::string());
}

void FakeOAuth2TokenServiceDelegate::ExtractCredentials(
    OAuth2TokenService* to_service,
    const std::string& account_id) {
  auto it = refresh_tokens_.find(account_id);
  DCHECK(it != refresh_tokens_.end());
  to_service->GetDelegate()->UpdateCredentials(account_id,
                                               it->second->refresh_token);
  RevokeCredentials(account_id);
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return shared_factory_;
}

bool FakeOAuth2TokenServiceDelegate::FixRequestErrorIfPossible() {
  return fix_request_if_possible_;
}

void FakeOAuth2TokenServiceDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  backoff_entry_.InformOfRequest(!error.IsTransientError());
  // Drop transient errors to match OAuth2TokenService's stated contract for
  // GetAuthError() and to allow clients to test proper behavior in the case of
  // transient errors.
  if (error.IsTransientError())
    return;

  if (GetAuthError(account_id) == error)
    return;

  auto it = refresh_tokens_.find(account_id);
  DCHECK(it != refresh_tokens_.end());
  it->second->error = error;
  FireAuthErrorChanged(account_id, error);
}
