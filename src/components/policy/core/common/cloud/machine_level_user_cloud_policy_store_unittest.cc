// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace policy {

// The unit test for MachineLevelUserCloudPolicyStore. Note that most of test
// cases are covered by UserCloudPolicyStoreTest so that the cases here are
// focus on testing the unique part of MachineLevelUserCloudPolicyStore.
class MachineLevelUserCloudPolicyStoreTest : public ::testing::Test {
 public:
  MachineLevelUserCloudPolicyStoreTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    policy_.SetDefaultInitialSigningKey();
    policy_.policy_data().set_policy_type(
        dm_protocol::kChromeMachineLevelUserCloudPolicyType);
    policy_.payload().mutable_searchsuggestenabled()->set_value(false);
    policy_.Build();
  }

  ~MachineLevelUserCloudPolicyStoreTest() override {}

  void SetUp() override {
    ASSERT_TRUE(tmp_policy_dir_.CreateUniqueTempDir());
    updater_policy_path_ =
        tmp_policy_dir_.GetPath().AppendASCII("updater_policies");
    store_ = CreateStore();
  }

  void SetExpectedPolicyMap(PolicySource source) {
    expected_policy_map_.Clear();
    expected_policy_map_.Set("SearchSuggestEnabled", POLICY_LEVEL_MANDATORY,
                             POLICY_SCOPE_MACHINE, source,
                             std::make_unique<base::Value>(false), nullptr);
  }

  std::unique_ptr<MachineLevelUserCloudPolicyStore> CreateStore(
      bool cloud_policy_overrides = false) {
    std::unique_ptr<MachineLevelUserCloudPolicyStore> store =
        MachineLevelUserCloudPolicyStore::Create(
            DMToken::CreateValidTokenForTesting(PolicyBuilder::kFakeToken),
            PolicyBuilder::kFakeDeviceId, updater_policy_path_,
            tmp_policy_dir_.GetPath(), cloud_policy_overrides,
            base::ThreadTaskRunnerHandle::Get());
    store->AddObserver(&observer_);
    return store;
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_.reset();
    base::RunLoop().RunUntilIdle();
  }

  const base::FilePath& updater_policy_path() const {
    return updater_policy_path_;
  }
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store_;

  base::ScopedTempDir tmp_policy_dir_;
  base::FilePath updater_policy_path_;
  UserPolicyBuilder policy_;
  UserPolicyBuilder updater_policy_;
  MockCloudPolicyStoreObserver observer_;
  PolicyMap expected_policy_map_;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyStoreTest);
};

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadWithoutDMToken) {
  store_->SetupRegistration(DMToken::CreateEmptyTokenForTesting(),
                            std::string());
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  EXPECT_CALL(observer_, OnStoreLoaded(_)).Times(0);
  EXPECT_CALL(observer_, OnStoreError(_)).Times(0);

  store_->Load();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadImmediatelyWithoutDMToken) {
  store_->SetupRegistration(DMToken::CreateEmptyTokenForTesting(),
                            std::string());
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  EXPECT_CALL(observer_, OnStoreLoaded(_)).Times(0);
  EXPECT_CALL(observer_, OnStoreError(_)).Times(0);

  store_->LoadImmediately();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadWithNoFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Load();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, StorePolicy) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  const base::FilePath policy_path = tmp_policy_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Machine Level User Cloud Policy"));
  const base::FilePath signing_key_path = tmp_policy_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Machine Level User Cloud Policy Signing Key"));
  EXPECT_FALSE(base::PathExists(policy_path));
  EXPECT_FALSE(base::PathExists(signing_key_path));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(base::PathExists(policy_path));
  EXPECT_TRUE(base::PathExists(signing_key_path));

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, StoreThenLoadPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  SetExpectedPolicyMap(POLICY_SOURCE_CLOUD);
  ASSERT_TRUE(loader->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            loader->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadOlderExternalPolicies) {
  // Create a fake updater policy file.
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  policy_.policy_data().set_timestamp(1000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.Build();
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&observer_);
  ASSERT_TRUE(base::Move(
      tmp_policy_dir_.GetPath().AppendASCII("Machine Level User Cloud Policy"),
      updater_policy_path()));

  // Create a policy file made by the browser.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(store.get()));
  policy_.policy_data().set_timestamp(2000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(false);
  policy_.Build();
  store->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  store->RemoveObserver(&observer_);
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  SetExpectedPolicyMap(POLICY_SOURCE_CLOUD);
  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_policy_map_.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadRecentExternalPolicies) {
  // Create a fake updater policy file.
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  policy_.policy_data().set_timestamp(2000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.Build();
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&observer_);
  ASSERT_TRUE(base::Move(
      tmp_policy_dir_.GetPath().AppendASCII("Machine Level User Cloud Policy"),
      updater_policy_path()));

  // Create a policy file made by the browser.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(store.get()));
  policy_.policy_data().set_timestamp(1000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(false);
  policy_.Build();
  store->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  store->RemoveObserver(&observer_);
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  PolicyMap expected_updater_policy_map;
  expected_updater_policy_map.Set(
      "SearchSuggestEnabled", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);

  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_updater_policy_map.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadExternalOnlyPolicies) {
  // Create a fake updater policy file.
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  policy_.policy_data().set_timestamp(2000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.Build();
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&observer_);
  ASSERT_TRUE(base::Move(
      tmp_policy_dir_.GetPath().AppendASCII("Machine Level User Cloud Policy"),
      updater_policy_path()));

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  PolicyMap expected_updater_policy_map;
  expected_updater_policy_map.Set(
      "SearchSuggestEnabled", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);

  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_updater_policy_map.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
  loader->RemoveObserver(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest,
       StoreAndLoadPolicyWithCloudPriority) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore(true);
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  SetExpectedPolicyMap(POLICY_SOURCE_PRIORITY_CLOUD);
  ASSERT_TRUE(loader->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            loader->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest,
       StoreAndLoadWithInvalidTokenPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  loader->SetupRegistration(DMToken(DMToken::Status::kValid, "bad_token"),
                            "invalid_client_id");
  EXPECT_CALL(observer_, OnStoreError(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(loader->policy());
  EXPECT_TRUE(loader->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, KeyRotation) {
  EXPECT_FALSE(policy_.policy().has_new_public_key_signature());
  std::string original_policy_key = policy_.policy().new_public_key();

  // Store the original key
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(original_policy_key, store_->policy_signature_public_key());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Store the new key.
  policy_.SetDefaultSigningKey();
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  EXPECT_TRUE(policy_.policy().has_new_public_key_signature());
  EXPECT_NE(original_policy_key, policy_.policy().new_public_key());

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(policy_.policy().new_public_key(),
            store_->policy_signature_public_key());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
