// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/identity/identity_service.h"
#include "services/identity/public/cpp/account_state.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/interfaces/constants.mojom.h"
#include "services/identity/public/interfaces/identity_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

namespace identity {
namespace {

const std::string kTestGaiaId = "dummyId";
const std::string kTestEmail = "me@dummy.com";
const std::string kTestRefreshToken = "dummy-refresh-token";
const std::string kTestAccessToken = "access_token";

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  ServiceTestClient(service_manager::test::ServiceTest* test,
                    AccountTrackerService* account_tracker,
                    SigninManagerBase* signin_manager,
                    ProfileOAuth2TokenService* token_service)
      : service_manager::test::ServiceTestClient(test),
        account_tracker_(account_tracker),
        signin_manager_(signin_manager),
        token_service_(token_service) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::Bind(&ServiceTestClient::Create, base::Unretained(this)));
  }

 protected:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(source_info, interface_name,
                            std::move(interface_pipe));
  }

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == mojom::kServiceName) {
      identity_service_context_.reset(new service_manager::ServiceContext(
          base::MakeUnique<IdentityService>(account_tracker_, signin_manager_,
                                            token_service_),
          std::move(request)));
    }
  }

  void Create(const service_manager::BindSourceInfo& source_info,
              service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  AccountTrackerService* account_tracker_;
  SigninManagerBase* signin_manager_;
  ProfileOAuth2TokenService* token_service_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> identity_service_context_;
};

class IdentityManagerTest : public service_manager::test::ServiceTest {
 public:
  IdentityManagerTest()
      : ServiceTest("identity_unittests", false),
        signin_client_(&pref_service_),
        signin_manager_(&signin_client_, &account_tracker_) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    account_tracker_.Initialize(&signin_client_);
  }

  void TearDown() override {
    // Shut down the SigninManager so that the IdentityManager doesn't end up
    // outliving it.
    signin_manager_.Shutdown();
    ServiceTest::TearDown();
  }

  void OnReceivedPrimaryAccountInfo(
      base::Closure quit_closure,
      const base::Optional<AccountInfo>& account_info,
      const AccountState& account_state) {
    primary_account_info_ = account_info;
    primary_account_state_ = account_state;
    quit_closure.Run();
  }

  void OnReceivedAccountInfoFromGaiaId(
      base::Closure quit_closure,
      const base::Optional<AccountInfo>& account_info,
      const AccountState& account_state) {
    account_info_from_gaia_id_ = account_info;
    account_state_from_gaia_id_ = account_state;
    quit_closure.Run();
  }

  void OnReceivedAccessToken(base::Closure quit_closure,
                             const base::Optional<std::string>& access_token,
                             base::Time expiration_time,
                             const GoogleServiceAuthError& error) {
    access_token_ = access_token;
    access_token_error_ = error;
    quit_closure.Run();
  }

 protected:
  void SetUp() override {
    ServiceTest::SetUp();

    connector()->BindInterface(mojom::kServiceName, &identity_manager_);
  }

  // service_manager::test::ServiceTest:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(
        this, &account_tracker_, &signin_manager_, &token_service_);
  }

  mojom::IdentityManagerPtr identity_manager_;
  base::Optional<AccountInfo> primary_account_info_;
  AccountState primary_account_state_;
  base::Optional<AccountInfo> account_info_from_gaia_id_;
  AccountState account_state_from_gaia_id_;
  base::Optional<std::string> access_token_;
  GoogleServiceAuthError access_token_error_;

  AccountTrackerService* account_tracker() { return &account_tracker_; }
  SigninManagerBase* signin_manager() { return &signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeSigninManagerBase signin_manager_;
  FakeProfileOAuth2TokenService token_service_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerTest);
};

// Tests that the Identity Manager destroys itself on SigninManager shutdown.
TEST_F(IdentityManagerTest, SigninManagerShutdown) {
  base::RunLoop run_loop;
  identity_manager_.set_connection_error_handler(run_loop.QuitClosure());

  // Ensure that the IdentityManager instance has actually been created before
  // invoking SigninManagerBase::Shutdown(), since otherwise this test will
  // spin forever.
  identity_manager_.FlushForTesting();
  signin_manager()->Shutdown();
  run_loop.Run();
}

// Check that the primary account info is null if not signed in.
TEST_F(IdentityManagerTest, GetPrimaryAccountInfoNotSignedIn) {
  base::RunLoop run_loop;
  identity_manager_->GetPrimaryAccountInfo(
      base::Bind(&IdentityManagerTest::OnReceivedPrimaryAccountInfo,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(primary_account_info_);
}

// Check that the primary account info has expected values if signed in without
// a refresh token available.
TEST_F(IdentityManagerTest, GetPrimaryAccountInfoSignedInNoRefreshToken) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  identity_manager_->GetPrimaryAccountInfo(
      base::Bind(&IdentityManagerTest::OnReceivedPrimaryAccountInfo,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_info_->gaia);
  EXPECT_EQ(kTestEmail, primary_account_info_->email);
  EXPECT_FALSE(primary_account_state_.has_refresh_token);
}

// Check that the primary account info has expected values if signed in with a
// refresh token available.
TEST_F(IdentityManagerTest, GetPrimaryAccountInfoSignedInRefreshToken) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  base::RunLoop run_loop;
  identity_manager_->GetPrimaryAccountInfo(
      base::Bind(&IdentityManagerTest::OnReceivedPrimaryAccountInfo,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_info_->gaia);
  EXPECT_EQ(kTestEmail, primary_account_info_->email);
  EXPECT_TRUE(primary_account_state_.has_refresh_token);
}

// Check that the account info for a given GAIA ID is null if that GAIA ID is
// unknown.
TEST_F(IdentityManagerTest, GetAccountInfoForUnknownGaiaID) {
  base::RunLoop run_loop;
  identity_manager_->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::Bind(&IdentityManagerTest::OnReceivedAccountInfoFromGaiaId,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(account_info_from_gaia_id_);
}

// Check that the account info for a given GAIA ID has expected values if that
// GAIA ID is known and there is no refresh token available for it.
TEST_F(IdentityManagerTest, GetAccountInfoForKnownGaiaIdNoRefreshToken) {
  std::string account_id =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  identity_manager_->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::Bind(&IdentityManagerTest::OnReceivedAccountInfoFromGaiaId,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_FALSE(account_state_from_gaia_id_.has_refresh_token);
}

// Check that the account info for a given GAIA ID has expected values if that
// GAIA ID is known and has a refresh token available.
TEST_F(IdentityManagerTest, GetAccountInfoForKnownGaiaIdRefreshToken) {
  std::string account_id =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, kTestRefreshToken);
  base::RunLoop run_loop;
  identity_manager_->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::Bind(&IdentityManagerTest::OnReceivedAccountInfoFromGaiaId,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_TRUE(account_state_from_gaia_id_.has_refresh_token);
}

// Check that the expected error is received if requesting an access token when
// not signed in.
TEST_F(IdentityManagerTest, GetAccessTokenNotSignedIn) {
  base::RunLoop run_loop;
  identity_manager_->GetAccessToken(
      kTestGaiaId, ScopeSet(), "dummy_consumer",
      base::Bind(&IdentityManagerTest::OnReceivedAccessToken,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(access_token_);
  EXPECT_EQ(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP,
            access_token_error_.state());
}

// Check that the expected access token is received if requesting an access
// token when signed in.
TEST_F(IdentityManagerTest, GetAccessTokenSignedIn) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, kTestRefreshToken);
  token_service()->set_auto_post_fetch_response_on_message_loop(true);
  base::RunLoop run_loop;

  identity_manager_->GetAccessToken(
      account_id, ScopeSet(), "dummy_consumer",
      base::Bind(&IdentityManagerTest::OnReceivedAccessToken,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(access_token_);
  EXPECT_EQ(kTestAccessToken, access_token_.value());
  EXPECT_EQ(GoogleServiceAuthError::State::NONE, access_token_error_.state());
}

}  // namespace
}  // namespace identity
