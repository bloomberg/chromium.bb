// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/message_loop/message_loop.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Class that observes updates from ProfileOAuth2TokenService.
class TestTokenServiceObserver : public OAuth2TokenService::Observer {
 public:
  explicit TestTokenServiceObserver(OAuth2TokenService* token_service)
      : token_service_(token_service) {
    token_service_->AddObserver(this);
  }
  ~TestTokenServiceObserver() override { token_service_->RemoveObserver(this); }

  void set_on_refresh_tokens_loaded_callback(base::OnceClosure callback) {
    on_refresh_tokens_loaded_callback_ = std::move(callback);
  }

 private:
  // OAuth2TokenService::Observer:
  void OnRefreshTokensLoaded() override {
    if (on_refresh_tokens_loaded_callback_)
      std::move(on_refresh_tokens_loaded_callback_).Run();
  }

  OAuth2TokenService* token_service_;
  base::OnceClosure on_refresh_tokens_loaded_callback_;
};

}  // namespace

namespace identity {
class AccountsMutatorImplTest : public testing::Test {
 public:
  AccountsMutatorImplTest()
      : token_service_(&pref_service_),
        token_service_observer_(&token_service_),
        accounts_mutator_(&token_service_) {
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

  TestTokenServiceObserver* token_service_observer() {
    return &token_service_observer_;
  }

  AccountsMutatorImpl* accounts_mutator() { return &accounts_mutator_; }

 private:
  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeProfileOAuth2TokenService token_service_;
  TestTokenServiceObserver token_service_observer_;
  AccountsMutatorImpl accounts_mutator_;

  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorImplTest);
};

TEST_F(AccountsMutatorImplTest, Basic) {
  // Should not crash.
}

}  // namespace identity
