// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/identity/identity_service.h"
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

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  ServiceTestClient(service_manager::test::ServiceTest* test,
                    SigninManagerBase* signin_manager)
      : service_manager::test::ServiceTestClient(test),
        signin_manager_(signin_manager) {
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
          base::MakeUnique<IdentityService>(signin_manager_),
          std::move(request)));
    }
  }

  void Create(const service_manager::BindSourceInfo& source_info,
              service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  SigninManagerBase* signin_manager_;
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

  void OnReceivedPrimaryAccountId(base::Closure quit_closure,
                                  const base::Optional<AccountId>& account_id) {
    primary_account_id_ = account_id;
    quit_closure.Run();
  }

 protected:
  void SetUp() override {
    ServiceTest::SetUp();

    connector()->BindInterface(mojom::kServiceName, &identity_manager_);
  }

  // service_manager::test::ServiceTest:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(this, &signin_manager_);
  }

  mojom::IdentityManagerPtr identity_manager_;
  base::Optional<AccountId> primary_account_id_;

  SigninManagerBase* signin_manager() { return &signin_manager_; }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeSigninManagerBase signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerTest);
};

// Check that the primary account ID is null if not signed in.
TEST_F(IdentityManagerTest, NotSignedIn) {
  base::RunLoop run_loop;
  identity_manager_->GetPrimaryAccountId(
      base::Bind(&IdentityManagerTest::OnReceivedPrimaryAccountId,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(primary_account_id_);
}

// Check that the primary account ID has expected values if signed in.
TEST_F(IdentityManagerTest, SignedIn) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  identity_manager_->GetPrimaryAccountId(
      base::Bind(&IdentityManagerTest::OnReceivedPrimaryAccountId,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_id_);
  EXPECT_EQ(kTestGaiaId, primary_account_id_->GetGaiaId());
  EXPECT_EQ(kTestEmail, primary_account_id_->GetUserEmail());
}

}  // namespace
}  // namespace identity
