// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager_builder.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#endif

#if defined(OS_IOS)
#include "components/signin/public/identity_manager/ios/fake_device_accounts_provider.h"
#endif

namespace signin {

class IdentityManagerBuilderTest : public testing::Test {
 protected:
  IdentityManagerBuilderTest()
      : signin_client_(&pref_service_, &test_url_loader_factory_) {
    IdentityManager::RegisterProfilePrefs(pref_service_.registry());
  }

  ~IdentityManagerBuilderTest() override { signin_client_.Shutdown(); }

  TestSigninClient* GetSigninClient() { return &signin_client_; }

  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return &pref_service_;
  }

#if defined(OS_CHROMEOS)
  chromeos::AccountManagerFactory* GetAccountManagerFactory() {
    return &account_manager_factory_;
  }
#endif

 public:
  IdentityManagerBuilderTest(const IdentityManagerBuilderTest&) = delete;
  IdentityManagerBuilderTest& operator=(const IdentityManagerBuilderTest&) =
      delete;

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestSigninClient signin_client_;
#if defined(OS_CHROMEOS)
  chromeos::AccountManagerFactory account_manager_factory_;
#endif
};

// Test that IdentityManagerBuilder properly set all required parameters to the
// IdentityManager::InitParameters struct
TEST_F(IdentityManagerBuilderTest, BuildIdentityManagerInitParameters) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath dest_path = temp_dir.GetPath();

#if defined(OS_ANDROID)
  DisableInteractionWithSystemAccounts();
#endif

  IdentityManagerBuildParams params;
  params.account_consistency = AccountConsistencyMethod::kDisabled;
  params.image_decoder = std::make_unique<image_fetcher::FakeImageDecoder>();
  params.local_state = GetPrefService();
  params.network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  params.pref_service = GetPrefService();
  params.profile_path = dest_path;
  params.signin_client = GetSigninClient();

#if defined(OS_IOS)
  params.device_accounts_provider =
      std::make_unique<FakeDeviceAccountsProvider>();
#endif

#if defined(OS_CHROMEOS)
  chromeos::AccountManager* account_manager =
      GetAccountManagerFactory()->GetAccountManager(dest_path.value());
  account_manager->Initialize(
      dest_path, GetSigninClient()->GetURLLoaderFactory(),
      base::BindRepeating(
          [](base::OnceClosure closure) -> void { std::move(closure).Run(); }));
  params.account_manager = account_manager;
  params.is_regular_profile = true;
#endif

  const IdentityManager::InitParameters init_params =
      signin::BuildIdentityManagerInitParameters(&params);

  EXPECT_NE(init_params.account_tracker_service, nullptr);
  EXPECT_NE(init_params.token_service, nullptr);
  EXPECT_NE(init_params.gaia_cookie_manager_service, nullptr);
  EXPECT_NE(init_params.primary_account_manager, nullptr);
  EXPECT_NE(init_params.account_fetcher_service, nullptr);
  EXPECT_NE(init_params.primary_account_mutator, nullptr);
  EXPECT_NE(init_params.accounts_cookie_mutator, nullptr);
  EXPECT_NE(init_params.diagnostics_provider, nullptr);
#if defined(OS_IOS) || defined(OS_ANDROID)
  EXPECT_NE(init_params.device_accounts_synchronizer, nullptr);
  EXPECT_EQ(init_params.accounts_mutator, nullptr);
#else
  EXPECT_EQ(init_params.device_accounts_synchronizer, nullptr);
  EXPECT_NE(init_params.accounts_mutator, nullptr);
#endif
#if defined(OS_CHROMEOS)
  EXPECT_NE(init_params.chromeos_account_manager, nullptr);
#endif

  // Manually shut down AccountFetcherService to avoid DCHECK failure inside its
  // destructor.
  init_params.account_fetcher_service->Shutdown();
}

}  // namespace signin
