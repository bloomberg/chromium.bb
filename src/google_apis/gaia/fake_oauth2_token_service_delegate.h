// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_FAKE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}

class FakeOAuth2TokenServiceDelegate : public OAuth2TokenServiceDelegate {
 public:
  FakeOAuth2TokenServiceDelegate();
  ~FakeOAuth2TokenServiceDelegate() override;

  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;

  // Overriden to make sure it works on Android.
  bool RefreshTokenIsAvailable(const std::string& account_id) const override;

  GoogleServiceAuthError GetAuthError(
      const std::string& account_id) const override;
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;
  std::vector<std::string> GetAccounts() override;
  void RevokeAllCredentials() override;
  void LoadCredentials(const std::string& primary_account_id) override;
  void UpdateCredentials(const std::string& account_id,
                         const std::string& refresh_token) override;
  void RevokeCredentials(const std::string& account_id) override;
  void ExtractCredentials(OAuth2TokenService* to_service,
                          const std::string& account_id) override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;

  bool FixRequestErrorIfPossible() override;

  std::string GetRefreshToken(const std::string& account_id) const;

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void set_fix_request_if_possible(bool value) {
    fix_request_if_possible_ = value;
  }

  const net::BackoffEntry* BackoffEntry() const override;

 private:
  struct AccountInfo {
    AccountInfo(const std::string& refresh_token);

    const std::string refresh_token;
    GoogleServiceAuthError error;
  };

  void IssueRefreshTokenForUser(const std::string& account_id,
                                const std::string& token);

  // Maps account ids to info.
  std::map<std::string, std::unique_ptr<AccountInfo>> refresh_tokens_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  bool fix_request_if_possible_ = false;

  net::BackoffEntry backoff_entry_;

  DISALLOW_COPY_AND_ASSIGN(FakeOAuth2TokenServiceDelegate);
};
#endif
