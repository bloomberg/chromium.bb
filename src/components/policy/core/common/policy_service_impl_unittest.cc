// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_migrator.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace policy {

namespace {

const char kExtension[] = "extension-id";
const char kSameLevelPolicy[] = "policy-same-level-and-scope";
const char kDiffLevelPolicy[] = "chrome-diff-level-and-scope";
const std::string kUrl1 = "example.com";
const std::string kUrl2 = "gmail.com";
const std::string kUrl3 = "google.com";
const std::string kUrl4 = "youtube.com";
const std::string kAffiliationId1 = "abc";
const std::string kAffiliationId2 = "def";

// Helper to compare the arguments to an EXPECT_CALL of OnPolicyUpdated() with
// their expected values.
MATCHER_P(PolicyEquals, expected, "") {
  return arg.Equals(*expected);
}

// Helper to compare the arguments to an EXPECT_CALL of OnPolicyValueUpdated()
// with their expected values.
MATCHER_P(ValueEquals, expected, "") {
  return *expected == *arg;
}

// Helper that fills |bundle| with test policies.
void AddTestPolicies(PolicyBundle* bundle,
                     const char* value,
                     PolicyLevel level,
                     PolicyScope scope) {
  PolicyMap* policy_map =
      &bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map->Set(kSameLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(value),
                  nullptr);
  policy_map->Set(kDiffLevelPolicy, level, scope, POLICY_SOURCE_CLOUD,
                  base::Value(value), nullptr);
  policy_map =
      &bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension));
  policy_map->Set(kSameLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(value),
                  nullptr);
  policy_map->Set(kDiffLevelPolicy, level, scope, POLICY_SOURCE_CLOUD,
                  base::Value(value), nullptr);
}

// Observer class that changes the policy in the passed provider when the
// callback is invoked.
class ChangePolicyObserver : public PolicyService::Observer {
 public:
  explicit ChangePolicyObserver(MockConfigurationPolicyProvider* provider)
      : provider_(provider),
        observer_invoked_(false) {}

  void OnPolicyUpdated(const PolicyNamespace&,
                       const PolicyMap& previous,
                       const PolicyMap& current) override {
    PolicyMap new_policy;
    new_policy.Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, base::Value(14), nullptr);
    provider_->UpdateChromePolicy(new_policy);
    observer_invoked_ = true;
  }

  bool observer_invoked() const { return observer_invoked_; }

 private:
  raw_ptr<MockConfigurationPolicyProvider> provider_;
  bool observer_invoked_;
};

class MockPolicyMigrator : public PolicyMigrator {
 public:
  MOCK_METHOD1(Migrate, void(PolicyBundle* bundle));
};

}  // namespace

class PolicyServiceTest : public testing::Test {
 public:
  PolicyServiceTest() = default;
  PolicyServiceTest(const PolicyServiceTest&) = delete;
  PolicyServiceTest& operator=(const PolicyServiceTest&) = delete;
  void SetUp() override {
    EXPECT_CALL(provider0_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider1_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider2_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider0_, IsFirstPolicyLoadComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
        .WillRepeatedly(Return(true));

    provider0_.Init();
    provider1_.Init();
    provider2_.Init();

    policy0_.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(13), nullptr);
    provider0_.UpdateChromePolicy(policy0_);

    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider0_);
    providers.push_back(&provider1_);
    providers.push_back(&provider2_);
    auto migrator = std::make_unique<MockPolicyMigrator>();
    EXPECT_CALL(*migrator, Migrate(_))
        .WillRepeatedly(Invoke([](PolicyBundle* bundle) {
          bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
              .Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);
        }));
    PolicyServiceImpl::Migrators migrators;
    migrators.push_back(std::move(migrator));
    policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers),
                                                          std::move(migrators));
  }

  void TearDown() override {
    provider0_.Shutdown();
    provider1_.Shutdown();
    provider2_.Shutdown();
  }

  MOCK_METHOD2(OnPolicyValueUpdated, void(const base::Value*,
                                          const base::Value*));

  MOCK_METHOD0(OnPolicyRefresh, void());

  // Returns true if the policies for namespace |ns| match |expected|.
  bool VerifyPolicies(const PolicyNamespace& ns,
                      const PolicyMap& expected) {
    return policy_service_->GetPolicies(ns).Equals(expected);
  }

  std::unique_ptr<PolicyBundle> CreateBundle(
      PolicyScope scope,
      PolicySource source,
      std::vector<std::pair<std::string, base::Value>> policies,
      PolicyNamespace policy_namespace) {
    auto policy_bundle = std::make_unique<PolicyBundle>();
    PolicyMap& policy_map = policy_bundle->Get(policy_namespace);

    for (auto& policy : policies) {
      policy_map.Set(std::move(policy.first), POLICY_LEVEL_MANDATORY, scope,
                     source, std::move(policy.second), nullptr);
    }
    return policy_bundle;
  }

  void RunUntilIdle() {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockConfigurationPolicyProvider provider0_;
  MockConfigurationPolicyProvider provider1_;
  MockConfigurationPolicyProvider provider2_;
  PolicyMap policy0_;
  PolicyMap policy1_;
  PolicyMap policy2_;
  std::unique_ptr<PolicyServiceImpl> policy_service_;
};

TEST_F(PolicyServiceTest, LoadsPoliciesBeforeProvidersRefresh) {
  PolicyMap expected;
  expected.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(13), nullptr);
  expected.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);
  EXPECT_TRUE(VerifyPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), expected));
}

TEST_F(PolicyServiceTest, NotifyObservers) {
  MockPolicyServiceObserver observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &observer);

  PolicyMap expectedPrevious;
  expectedPrevious.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(13),
                       nullptr);
  expectedPrevious.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap expectedCurrent;
  expectedCurrent = expectedPrevious.Clone();
  expectedCurrent.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_CLOUD, base::Value(123), nullptr);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(123), nullptr);
  EXPECT_CALL(observer, OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                                        std::string()),
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // No changes.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _)).Times(0);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(VerifyPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), expectedCurrent));

  // New policy.
  expectedPrevious = expectedCurrent.Clone();
  expectedCurrent.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_CLOUD, base::Value(456), nullptr);
  policy0_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(456), nullptr);
  EXPECT_CALL(observer, OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                                        std::string()),
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // Removed policy.
  expectedPrevious = expectedCurrent.Clone();
  expectedCurrent.Erase("bbb");
  policy0_.Erase("bbb");
  EXPECT_CALL(observer, OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                                        std::string()),
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // Changed policy.
  expectedPrevious = expectedCurrent.Clone();
  expectedCurrent.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_CLOUD, base::Value(789), nullptr);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(789), nullptr);

  EXPECT_CALL(observer, OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                                        std::string()),
                                        PolicyEquals(&expectedPrevious),
                                        PolicyEquals(&expectedCurrent)));
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);

  // No changes again.
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _)).Times(0);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(VerifyPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), expectedCurrent));

  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
}

TEST_F(PolicyServiceTest, NotifyObserversInMultipleNamespaces) {
  const std::string kExtension0("extension-0");
  const std::string kExtension1("extension-1");
  const std::string kExtension2("extension-2");
  MockPolicyServiceObserver chrome_observer;
  MockPolicyServiceObserver extension_observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &chrome_observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, &extension_observer);

  PolicyMap previous_policy_map;
  previous_policy_map.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                          POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(13),
                          nullptr);
  previous_policy_map.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                          POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);
  PolicyMap policy_map;
  policy_map = previous_policy_map.Clone();
  policy_map.Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);

  auto bundle = std::make_unique<PolicyBundle>();
  // The initial setup includes a policy for chrome that is now changing.
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
      policy_map.Clone();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension0)) =
      policy_map.Clone();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension1)) =
      policy_map.Clone();

  const PolicyMap kEmptyPolicyMap;
  EXPECT_CALL(
      chrome_observer,
      OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()),
                      PolicyEquals(&previous_policy_map),
                      PolicyEquals(&policy_map)));
  EXPECT_CALL(
      extension_observer,
      OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension0),
                      PolicyEquals(&kEmptyPolicyMap),
                      PolicyEquals(&policy_map)));
  EXPECT_CALL(
      extension_observer,
      OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension1),
                      PolicyEquals(&kEmptyPolicyMap),
                      PolicyEquals(&policy_map)));
  provider0_.UpdatePolicy(std::move(bundle));
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&chrome_observer);
  Mock::VerifyAndClearExpectations(&extension_observer);

  // Chrome policy stays the same, kExtension0 is gone, kExtension1 changes,
  // and kExtension2 is new.
  previous_policy_map = policy_map.Clone();
  bundle = std::make_unique<PolicyBundle>();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
      policy_map.Clone();
  policy_map.Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("another value"), nullptr);
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension1)) =
      policy_map.Clone();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension2)) =
      policy_map.Clone();

  EXPECT_CALL(chrome_observer, OnPolicyUpdated(_, _, _)).Times(0);
  EXPECT_CALL(
      extension_observer,
      OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension0),
                      PolicyEquals(&previous_policy_map),
                      PolicyEquals(&kEmptyPolicyMap)));
  EXPECT_CALL(
      extension_observer,
      OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension1),
                      PolicyEquals(&previous_policy_map),
                      PolicyEquals(&policy_map)));
  EXPECT_CALL(
      extension_observer,
      OnPolicyUpdated(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension2),
                      PolicyEquals(&kEmptyPolicyMap),
                      PolicyEquals(&policy_map)));
  provider0_.UpdatePolicy(std::move(bundle));
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&chrome_observer);
  Mock::VerifyAndClearExpectations(&extension_observer);

  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &chrome_observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS,
                                  &extension_observer);
}

TEST_F(PolicyServiceTest, ObserverChangesPolicy) {
  ChangePolicyObserver observer(&provider0_);
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &observer);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(123), nullptr);
  policy0_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(1234), nullptr);
  // Should not crash.
  provider0_.UpdateChromePolicy(policy0_);
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
  EXPECT_TRUE(observer.observer_invoked());
}

TEST_F(PolicyServiceTest, HasProvider) {
  MockConfigurationPolicyProvider other_provider;
  EXPECT_TRUE(policy_service_->HasProvider(&provider0_));
  EXPECT_TRUE(policy_service_->HasProvider(&provider1_));
  EXPECT_TRUE(policy_service_->HasProvider(&provider2_));
  EXPECT_FALSE(policy_service_->HasProvider(&other_provider));
}

TEST_F(PolicyServiceTest, NotifyProviderUpdateObserver) {
  MockPolicyServiceProviderUpdateObserver provider_update_observer;
  policy_service_->AddProviderUpdateObserver(&provider_update_observer);

  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(123), nullptr);
  EXPECT_CALL(provider_update_observer,
              OnProviderUpdatePropagated(&provider0_));
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&provider_update_observer);

  // No changes, ProviderUpdateObserver still notified.
  EXPECT_CALL(provider_update_observer,
              OnProviderUpdatePropagated(&provider0_));
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(&provider_update_observer);

  policy_service_->RemoveProviderUpdateObserver(&provider_update_observer);
}

TEST_F(PolicyServiceTest, Priorities) {
  PolicyMap expected;
  expected.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(13), nullptr);
  expected.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);
  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  expected.GetMutable("aaa")->AddMessage(PolicyMap::MessageType::kWarning,
                                         IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.GetMutable("aaa")->AddMessage(PolicyMap::MessageType::kWarning,
                                         IDS_POLICY_CONFLICT_DIFF_VALUE);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  policy1_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  policy2_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  provider1_.UpdateChromePolicy(policy1_);
  provider2_.UpdateChromePolicy(policy2_);
  expected.GetMutable("aaa")->AddConflictingPolicy(
      policy1_.Get("aaa")->DeepCopy());
  expected.GetMutable("aaa")->AddConflictingPolicy(
      policy2_.Get("aaa")->DeepCopy());

  EXPECT_TRUE(VerifyPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  expected.GetMutable("aaa")->AddMessage(PolicyMap::MessageType::kWarning,
                                         IDS_POLICY_CONFLICT_DIFF_VALUE);
  policy0_.Erase("aaa");
  provider0_.UpdateChromePolicy(policy0_);
  expected.GetMutable("aaa")->AddConflictingPolicy(
      policy2_.Get("aaa")->DeepCopy());
  EXPECT_TRUE(VerifyPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), expected));

  expected.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  expected.GetMutable("aaa")->AddMessage(PolicyMap::MessageType::kInfo,
                                         IDS_POLICY_CONFLICT_SAME_VALUE);
  policy1_.Set("aaa", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  expected.GetMutable("aaa")->AddConflictingPolicy(
      policy2_.Get("aaa")->DeepCopy());
  provider1_.UpdateChromePolicy(policy2_);
  EXPECT_TRUE(VerifyPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), expected));
}

TEST_F(PolicyServiceTest, PolicyChangeRegistrar) {
  std::unique_ptr<PolicyChangeRegistrar> registrar(new PolicyChangeRegistrar(
      policy_service_.get(),
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));

  // Starting to observe existing policies doesn't trigger a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  registrar->Observe(
      "pre", base::BindRepeating(&PolicyServiceTest::OnPolicyValueUpdated,
                                 base::Unretained(this)));
  registrar->Observe(
      "aaa", base::BindRepeating(&PolicyServiceTest::OnPolicyValueUpdated,
                                 base::Unretained(this)));
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  // Changing it now triggers a notification.
  base::Value kValue0(0);
  EXPECT_CALL(*this, OnPolicyValueUpdated(NULL, ValueEquals(&kValue0)));
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue0.Clone(), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // Changing other values doesn't trigger a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  policy0_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue0.Clone(), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // Modifying the value triggers a notification.
  base::Value kValue1(1);
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue0),
                                          ValueEquals(&kValue1)));
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue1.Clone(), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // Removing the value triggers a notification.
  EXPECT_CALL(*this, OnPolicyValueUpdated(ValueEquals(&kValue1), NULL));
  policy0_.Erase("aaa");
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  // No more notifications after destroying the registrar.
  EXPECT_CALL(*this, OnPolicyValueUpdated(_, _)).Times(0);
  registrar.reset();
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue1.Clone(), nullptr);
  policy0_.Set("pre", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, kValue1.Clone(), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(PolicyServiceTest, RefreshPolicies) {
  EXPECT_CALL(provider0_, RefreshPolicies()).Times(AnyNumber());
  EXPECT_CALL(provider1_, RefreshPolicies()).Times(AnyNumber());
  EXPECT_CALL(provider2_, RefreshPolicies()).Times(AnyNumber());

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy_service_->RefreshPolicies(base::BindOnce(
      &PolicyServiceTest::OnPolicyRefresh, base::Unretained(this)));
  // Let any queued observer tasks run.
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  base::Value kValue0(0);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue0.Clone(), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  base::Value kValue1(1);
  policy1_.Set("aaa", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue1.Clone(), nullptr);
  provider1_.UpdateChromePolicy(policy1_);
  Mock::VerifyAndClearExpectations(this);

  // A provider can refresh more than once after a RefreshPolicies call, but
  // OnPolicyRefresh should be triggered only after all providers are
  // refreshed.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy1_.Set("bbb", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue1.Clone(), nullptr);
  provider1_.UpdateChromePolicy(policy1_);
  Mock::VerifyAndClearExpectations(this);

  // If another RefreshPolicies() call happens while waiting for a previous
  // one to complete, then all providers must refresh again.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy_service_->RefreshPolicies(base::BindOnce(
      &PolicyServiceTest::OnPolicyRefresh, base::Unretained(this)));
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyRefresh()).Times(0);
  policy2_.Set("bbb", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue0.Clone(), nullptr);
  provider2_.UpdateChromePolicy(policy2_);
  Mock::VerifyAndClearExpectations(this);

  // Providers 0 and 1 must reload again.
  EXPECT_CALL(*this, OnPolicyRefresh()).Times(2);
  base::Value kValue2(2);
  policy0_.Set("aaa", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, kValue2.Clone(), nullptr);
  provider0_.UpdateChromePolicy(policy0_);
  provider1_.UpdateChromePolicy(policy1_);
  Mock::VerifyAndClearExpectations(this);

  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_EQ(kValue2, *policies.GetValue("aaa"));
  EXPECT_EQ(kValue0, *policies.GetValue("bbb"));
}

TEST_F(PolicyServiceTest, NamespaceMerge) {
  auto bundle0 = std::make_unique<PolicyBundle>();
  auto bundle1 = std::make_unique<PolicyBundle>();
  auto bundle2 = std::make_unique<PolicyBundle>();

  AddTestPolicies(bundle0.get(), "bundle0",
                  POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER);
  AddTestPolicies(bundle1.get(), "bundle1",
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  AddTestPolicies(bundle2.get(), "bundle2",
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);

  PolicyMap expected;
  // For policies of the same level and scope, the first provider takes
  // precedence, on every namespace.
  expected.Set(kSameLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value("bundle0"),
               nullptr);
  expected.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);
  expected.GetMutable(kSameLevelPolicy)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.GetMutable(kSameLevelPolicy)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.GetMutable(kSameLevelPolicy)
      ->AddConflictingPolicy(
          bundle1->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
              .Get(kSameLevelPolicy)
              ->DeepCopy());
  expected.GetMutable(kSameLevelPolicy)
      ->AddConflictingPolicy(
          bundle2->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
              .Get(kSameLevelPolicy)
              ->DeepCopy());
  // For policies with different levels and scopes, the highest priority
  // level/scope combination takes precedence, on every namespace.
  expected.Set(kDiffLevelPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               POLICY_SOURCE_CLOUD, base::Value("bundle2"), nullptr);
  expected.GetMutable(kDiffLevelPolicy)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected.GetMutable(kDiffLevelPolicy)
      ->AddConflictingPolicy(
          bundle0->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
              .Get(kDiffLevelPolicy)
              ->DeepCopy());
  expected.GetMutable(kDiffLevelPolicy)
      ->AddConflictingPolicy(
          bundle1->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
              .Get(kDiffLevelPolicy)
              ->DeepCopy());

  provider0_.UpdatePolicy(std::move(bundle0));
  provider1_.UpdatePolicy(std::move(bundle1));
  provider2_.UpdatePolicy(std::move(bundle2));
  RunUntilIdle();

  EXPECT_TRUE(policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())).Equals(expected));
  expected.Erase("migrated");
  EXPECT_TRUE(policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtension)).Equals(expected));
}

TEST_F(PolicyServiceTest, IsInitializationComplete) {
  // |provider0_| has all domains initialized.
  Mock::VerifyAndClearExpectations(&provider1_);
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider1_, IsInitializationComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsInitializationComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider0_);
  providers.push_back(&provider1_);
  providers.push_back(&provider2_);
  policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // |provider2_| still doesn't have POLICY_DOMAIN_CHROME initialized, so
  // the initialization status of that domain won't change.
  MockPolicyServiceObserver observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);
  EXPECT_CALL(observer, OnPolicyServiceInitialized(_)).Times(0);
  Mock::VerifyAndClearExpectations(&provider1_);
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider1_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider1_,
              IsInitializationComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(false));
  const PolicyMap kPolicyMap;
  provider1_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Same if |provider1_| doesn't have POLICY_DOMAIN_EXTENSIONS initialized.
  EXPECT_CALL(observer, OnPolicyServiceInitialized(_)).Times(0);
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_,
              IsInitializationComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  provider2_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Now initialize POLICY_DOMAIN_CHROME on all the providers.
  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_,
              IsInitializationComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  provider2_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  // Other domains are still not initialized.
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Initialize the remaining domains.
  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_CALL(observer,
              OnPolicyServiceInitialized(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
  Mock::VerifyAndClearExpectations(&provider1_);
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider1_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_,
              IsInitializationComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  provider1_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Cleanup.
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);
}

using DomainParameters = std::tuple<bool,  // provider initialized
                                    bool,  // first policy fetched
                                    bool   // observer present
                                    >;
using ObserverTestParameters = std::tuple<DomainParameters,  // CHROME
                                          DomainParameters,  // EXTENSIONS
                                          DomainParameters  // SIGNIN_EXTENSIONS
                                          >;

class PolicyServiceTestForObservers
    : public testing::Test,
      public testing::WithParamInterface<ObserverTestParameters> {
 public:
  PolicyServiceTestForObservers() = default;
  PolicyServiceTestForObservers(const PolicyServiceTestForObservers& other) =
      delete;
  PolicyServiceTestForObservers& operator=(
      const PolicyServiceTestForObservers& other) = delete;
  ~PolicyServiceTestForObservers() override = default;

  void SetUp() override {
    SetupDomain<POLICY_DOMAIN_CHROME>();
    SetupDomain<POLICY_DOMAIN_EXTENSIONS>();
    SetupDomain<POLICY_DOMAIN_SIGNIN_EXTENSIONS>();

    provider_.Init();
  }

  void AddObservers(PolicyService* service) {
    AddObserver<POLICY_DOMAIN_CHROME>(service);
    AddObserver<POLICY_DOMAIN_EXTENSIONS>(service);
    AddObserver<POLICY_DOMAIN_SIGNIN_EXTENSIONS>(service);
  }

  void RemoveObservers(PolicyService* service) {
    RemoveObserver<POLICY_DOMAIN_CHROME>(service);
    RemoveObserver<POLICY_DOMAIN_EXTENSIONS>(service);
    RemoveObserver<POLICY_DOMAIN_SIGNIN_EXTENSIONS>(service);
  }

  void TearDown() override { provider_.Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockConfigurationPolicyProvider provider_;
  MockPolicyServiceObserver observer_;

 private:
  template <PolicyDomain domain>
  void SetupDomain() {
    DomainParameters params = std::get<domain>(GetParam());

    EXPECT_CALL(provider_, IsInitializationComplete(domain))
        .WillRepeatedly(Return(std::get<0>(params)));
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(domain))
        .WillRepeatedly(Return(std::get<1>(params)));
  }
  template <PolicyDomain domain>
  void AddObserver(PolicyService* service) {
    DomainParameters params = std::get<domain>(GetParam());

    const bool isInitialized = std::get<0>(params);
    const bool isPolicyFetched = std::get<1>(params);
    const bool hasObserver = std::get<2>(params);
    if (hasObserver)
      service->AddObserver(domain, &observer_);
    EXPECT_CALL(observer_, OnPolicyServiceInitialized(domain))
        .Times(isInitialized && hasObserver);
    EXPECT_CALL(observer_, OnFirstPoliciesLoaded(domain))
        .Times(isInitialized && isPolicyFetched && hasObserver);
  }
  template <PolicyDomain domain>
  void RemoveObserver(PolicyService* service) {
    DomainParameters params = std::get<domain>(GetParam());

    const bool hasObserver = std::get<2>(params);
    if (hasObserver)
      service->RemoveObserver(domain, &observer_);
  }
};

TEST_P(PolicyServiceTestForObservers, MaybeNotifyPolicyDomainStatusChange) {
  auto local_policy_service =
      PolicyServiceImpl::CreateWithThrottledInitialization(
          PolicyServiceImpl::Providers{&provider_});

  AddObservers(local_policy_service.get());

  local_policy_service->UnthrottleInitialization();

  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&provider_);

  RemoveObservers(local_policy_service.get());
}

INSTANTIATE_TEST_SUITE_P(
    AllDomains,
    PolicyServiceTestForObservers,
    testing::Combine(
        testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()),
        testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()),
        testing::Combine(testing::Bool(), testing::Bool(), testing::Bool())));

TEST_F(PolicyServiceTest, IsInitializationCompleteMightDestroyThis) {
  Mock::VerifyAndClearExpectations(&provider0_);
  EXPECT_CALL(provider0_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider0_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(true));
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider0_);
  auto local_policy_service =
      PolicyServiceImpl::CreateWithThrottledInitialization(
          std::move(providers));
  EXPECT_FALSE(
      local_policy_service->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  MockPolicyServiceObserver observer;
  local_policy_service->AddObserver(POLICY_DOMAIN_CHROME, &observer);

  // Now initialize policy domains on provider0.
  // One of our observers destroys the policy service.
  // This happens in the wild: https://crbug.com/747817
  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME))
      .WillOnce([&local_policy_service, &observer](auto) {
        local_policy_service->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
        local_policy_service.reset();
      });

  local_policy_service->UnthrottleInitialization();
  EXPECT_FALSE(local_policy_service);

  Mock::VerifyAndClearExpectations(&observer);
  Mock::VerifyAndClearExpectations(&provider0_);
}

// Tests initialization throttling of PolicyServiceImpl.
// This actually tests two cases:
// (1) A domain was initialized before UnthrottleInitialization is called.
//     Observers only get notified after calling UnthrottleInitialization.
//     This is tested on POLICY_DOMAIN_CHROME.
// (2) A domain becomes initialized after UnthrottleInitialization has already
//     been called. Because initialization is not throttled anymore, observers
//     get notified immediately when the domain becomes initialized.
//     This is tested on POLICY_DOMAIN_EXTENSIONS and
//     POLICY_DOMAIN_SIGNIN_EXTENSIONS.
TEST_F(PolicyServiceTest, InitializationThrottled) {
  // |provider0_| and |provider1_| has all domains initialized, |provider2_| has
  // no domain initialized.
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsInitializationComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider0_);
  providers.push_back(&provider1_);
  providers.push_back(&provider2_);
  policy_service_ = PolicyServiceImpl::CreateWithThrottledInitialization(
      std::move(providers));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  MockPolicyServiceObserver observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);

  // Now additionally initialize POLICY_DOMAIN_CHROME on |provider2_|.
  // Note: VerifyAndClearExpectations is called to reset the previously set
  // action for IsInitializationComplete and IsFirstPolicyLoadComplete on
  // |provider_2|.
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_,
              IsInitializationComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_,
              IsFirstPolicyLoadComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(false));

  // Nothing will happen because initialization is still throttled.
  EXPECT_CALL(observer, OnPolicyServiceInitialized(_)).Times(0);
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(_)).Times(0);
  const PolicyMap kPolicyMap;
  provider2_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Unthrottle initialization. This will signal that POLICY_DOMAIN_CHROME is
  // initialized, the other domains should still not be initialized because
  // |provider2_| is returning false in IsInitializationComplete and
  // IsFirstPolicyLoadComplete for them.
  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_CHROME));
  policy_service_->UnthrottleInitialization();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Initialize the remaining domains.
  // Note: VerifyAndClearExpectations is called to reset the previously set
  // action for IsInitializationComplete and IsFirstPolicyLoadComplete on
  // |provider_2|.
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_CALL(observer,
              OnPolicyServiceInitialized(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
  provider2_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Cleanup.
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);
}

TEST_F(PolicyServiceTest, InitializationThrottledProvidersAlreadyInitialized) {
  // All providers have all domains initialized.
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider0_);
  providers.push_back(&provider1_);
  providers.push_back(&provider2_);
  policy_service_ = PolicyServiceImpl::CreateWithThrottledInitialization(
      std::move(providers));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  MockPolicyServiceObserver observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);

  // Unthrottle initialization. This will signal that all domains are
  // initialized.
  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer, OnPolicyServiceInitialized(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_CALL(observer,
              OnPolicyServiceInitialized(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
  policy_service_->UnthrottleInitialization();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(
      policy_service_->IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(policy_service_->IsInitializationComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Cleanup.
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);
}

TEST_F(PolicyServiceTest, IsFirstPolicyLoadComplete) {
  // |provider0_| has all domains initialized.
  Mock::VerifyAndClearExpectations(&provider1_);
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider1_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(_))
      .WillRepeatedly(Return(false));
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider0_);
  providers.push_back(&provider1_);
  providers.push_back(&provider2_);
  policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // |provider2_| still doesn't have POLICY_DOMAIN_CHROME initialized, so
  // the initialization status of that domain won't change.
  MockPolicyServiceObserver observer;
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->AddObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(_)).Times(0);
  Mock::VerifyAndClearExpectations(&provider1_);
  EXPECT_CALL(provider1_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider1_,
              IsFirstPolicyLoadComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(false));
  const PolicyMap kPolicyMap;
  provider1_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Same if |provider1_| doesn't have POLICY_DOMAIN_EXTENSIONS initialized.
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(_)).Times(0);
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_,
              IsFirstPolicyLoadComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  provider2_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Now initialize POLICY_DOMAIN_CHROME on all the providers.
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&provider2_);
  EXPECT_CALL(provider2_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider2_,
              IsFirstPolicyLoadComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  provider2_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  // Other domains are still not initialized.
  EXPECT_FALSE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_FALSE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Initialize the remaining domains.
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_CALL(observer, OnFirstPoliciesLoaded(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
  Mock::VerifyAndClearExpectations(&provider1_);
  EXPECT_CALL(provider1_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_, IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider1_,
              IsFirstPolicyLoadComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .WillRepeatedly(Return(true));
  provider1_.UpdateChromePolicy(kPolicyMap);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(
      policy_service_->IsFirstPolicyLoadComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(policy_service_->IsFirstPolicyLoadComplete(
      POLICY_DOMAIN_SIGNIN_EXTENSIONS));

  // Cleanup.
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_EXTENSIONS, &observer);
  policy_service_->RemoveObserver(POLICY_DOMAIN_SIGNIN_EXTENSIONS, &observer);
}

TEST_F(PolicyServiceTest, DictionaryPoliciesMerging) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  base::Value dict1 = base::Value(base::Value::Type::DICTIONARY);
  dict1.SetBoolKey(kUrl3, false);
  dict1.SetBoolKey(kUrl2, true);
  base::Value dict2 = base::Value(base::Value::Type::DICTIONARY);
  dict2.SetBoolKey(kUrl1, true);
  dict2.SetBoolKey(kUrl2, false);
  base::Value result = base::Value(base::Value::Type::DICTIONARY);
  result.SetBoolKey(kUrl1, true);
  result.SetBoolKey(kUrl2, true);
  result.SetBoolKey(kUrl3, false);

  std::unique_ptr<base::Value> policy =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  policy->Append(base::Value(key::kExtensionSettings));

  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyDictionaryMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kExtensionSettings, std::move(dict1));
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionSettings, std::move(dict2));
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);

  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyDictionaryMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionSettings)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionSettings)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionSettings, std::move(merged));

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

#if !defined(OS_CHROMEOS)
// Policy precedence changes are not supported on Chrome OS.
TEST_F(PolicyServiceTest, DictionaryPoliciesMerging_PrecedenceChange) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize affiliation IDs. User and device ID is identical.
  base::flat_set<std::string> ids;
  ids.insert(kAffiliationId1);

  // Initialize dictionaries of URLs used for ExtensionSettings policy values.
  base::Value dict1 = base::Value(base::Value::Type::DICTIONARY);
  dict1.SetBoolKey(kUrl2, true);
  dict1.SetBoolKey(kUrl3, false);
  base::Value dict2 = base::Value(base::Value::Type::DICTIONARY);
  dict2.SetBoolKey(kUrl1, true);
  dict2.SetBoolKey(kUrl2, false);
  base::Value dict3 = base::Value(base::Value::Type::DICTIONARY);
  dict3.SetBoolKey(kUrl3, true);
  dict3.SetBoolKey(kUrl4, false);
  base::Value result = base::Value(base::Value::Type::DICTIONARY);
  result.SetBoolKey(kUrl1, true);
  result.SetBoolKey(kUrl2, false);
  result.SetBoolKey(kUrl3, true);
  result.SetBoolKey(kUrl4, false);

  std::unique_ptr<base::Value> policy =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  policy->Append(base::Value(key::kExtensionSettings));

  // policy_bundle_1 is treated as a machine platform bundle. The metapolicies
  // are defined here.
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyDictionaryMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kCloudPolicyOverridesPlatformPolicy,
                          base::Value(true));
  policies_1.emplace_back(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                          base::Value(true));
  policies_1.emplace_back(key::kCloudUserPolicyMerge, base::Value(true));
  policies_1.emplace_back(key::kExtensionSettings, std::move(dict1));
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a machine cloud bundle. The device
  // affiliation IDs are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionSettings, std::move(dict2));
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);
  policy_bundle_2->Get(chrome_namespace).SetDeviceAffiliationIds(ids);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionSettings, std::move(dict3));
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(ids);

  // The expected_chrome PolicyMap contains the combined URLs from all three
  // policy bundles. The affiliation IDs don't need to be added as they're not
  // compared in the PolicyMap equality check.
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyDictionaryMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set(key::kCloudPolicyOverridesPlatformPolicy,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  expected_chrome.Set(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  expected_chrome.Set(key::kCloudUserPolicyMerge, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                      base::Value(true), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionSettings)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionSettings)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionSettings)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionSettings, std::move(merged));

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_3));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(PolicyServiceTest, ListsPoliciesMerging) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  base::Value list1(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl3));
  list1.Append(base::Value(kUrl2));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl1));
  list2.Append(base::Value(kUrl2));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl3));
  result.Append(base::Value(kUrl2));
  result.Append(base::Value(kUrl1));

  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);

  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

#if !defined(OS_CHROMEOS)
// The cloud user policy merging metapolicy is not applicable in Chrome OS.
TEST_F(PolicyServiceTest, ListsPoliciesMerging_CloudMetapolicy) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize affiliation IDs. User and device ID is identical.
  base::flat_set<std::string> ids;
  ids.insert(kAffiliationId1);

  base::Value list1(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl1));
  list1.Append(base::Value(kUrl2));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl2));
  list2.Append(base::Value(kUrl3));
  base::Value list3 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl3));
  list2.Append(base::Value(kUrl4));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl1));
  result.Append(base::Value(kUrl2));
  result.Append(base::Value(kUrl3));
  result.Append(base::Value(kUrl4));

  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  // policy_bundle_1 is treated as a machine platform bundle.
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a machine cloud bundle. In addition to the
  // device affiliation IDs, the metapolicies are also defined here to simulate
  // being set through CBCM.
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_2.emplace_back(key::kCloudUserPolicyMerge, base::Value(true));
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);
  policy_bundle_2->Get(chrome_namespace).SetDeviceAffiliationIds(ids);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionInstallForcelist, list3.Clone());
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(ids);

  // The expected_chrome PolicyMap contains the combined URLs from all three
  // policy bundles. The affiliation IDs don't need to be added as they're not
  // compared in the PolicyMap equality check.
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, policy->Clone(), nullptr);
  expected_chrome.Set(key::kCloudUserPolicyMerge, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(true), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));

  provider0_.UpdatePolicy(std::move(policy_bundle_3));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_1));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(PolicyServiceTest, GroupPoliciesMergingDisabledForCloudUsers) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  base::Value list1(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl3));
  base::Value list2(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl1));
  base::Value list3(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl4));
  base::Value result(base::Value::Type::LIST);
  result.Append(base::Value(kUrl3));
  result.Append(base::Value(kUrl1));

  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));
  policy->Append(base::Value(policy::key::kExtensionInstallBlocklist));

  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  policies_1.emplace_back(key::kExtensionInstallBlocklist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);
  // Unlike the rest of the bundle, this policy is set at the cloud user level.
  PolicyMap::Entry atomic_policy_enabled(POLICY_LEVEL_MANDATORY,
                                         POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                         base::Value(true), nullptr);
  policy_bundle_1->Get(chrome_namespace)
      .Set(key::kPolicyAtomicGroupsEnabled, atomic_policy_enabled.DeepCopy());

  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  policies_2.emplace_back(key::kExtensionInstallBlocklist, list2.Clone());
  policies_2.emplace_back(key::kExtensionInstallAllowlist, list3.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);

  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, merged.DeepCopy());
  expected_chrome.Set(key::kExtensionInstallBlocklist, std::move(merged));
  expected_chrome.Set(key::kExtensionInstallAllowlist,
                      policy_bundle_2->Get(chrome_namespace)
                          .Get(key::kExtensionInstallAllowlist)
                          ->DeepCopy());
  expected_chrome.Set(key::kPolicyAtomicGroupsEnabled,
                      atomic_policy_enabled.DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, GroupPoliciesMergingEnabled) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  base::Value list1(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl3));
  base::Value list2(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl1));
  base::Value list3(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl4));
  base::Value result(base::Value::Type::LIST);
  result.Append(base::Value(kUrl3));
  result.Append(base::Value(kUrl1));

  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));
  policy->Append(base::Value(policy::key::kExtensionInstallBlocklist));

  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  policies_1.emplace_back(key::kExtensionInstallBlocklist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);
  // Unlike the rest of the bundle, this policy is set at the cloud user level.
  PolicyMap::Entry atomic_policy_enabled(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value(true), nullptr);
  policy_bundle_1->Get(chrome_namespace)
      .Set(key::kPolicyAtomicGroupsEnabled, atomic_policy_enabled.DeepCopy());

  PolicyMap::Entry entry_list_3(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_CLOUD, list3.Clone(), nullptr);
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  policies_2.emplace_back(key::kExtensionInstallBlocklist, list2.Clone());
  policies_2.emplace_back(key::kExtensionInstallAllowlist,
                          entry_list_3.value()->Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);

  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  entry_list_3.SetIgnoredByPolicyAtomicGroup();
  expected_chrome.Set(key::kExtensionInstallForcelist, merged.DeepCopy());
  expected_chrome.Set(key::kExtensionInstallBlocklist, std::move(merged));
  expected_chrome.Set(key::kExtensionInstallAllowlist, std::move(entry_list_3));
  expected_chrome.Set(key::kPolicyAtomicGroupsEnabled,
                      atomic_policy_enabled.DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, CloudUserListPolicyMerge_Successful) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize affiliation IDs. User and device ID is identical.
  base::flat_set<std::string> ids;
  ids.insert(kAffiliationId1);

  // Initialize lists of URLs used for ExtensionInstallForcelist policy values.
  base::Value list1 = base::Value(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl1));
  list1.Append(base::Value(kUrl2));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl2));
  list2.Append(base::Value(kUrl3));
  base::Value list3 = base::Value(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl3));
  list3.Append(base::Value(kUrl4));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl1));
  result.Append(base::Value(kUrl2));
  result.Append(base::Value(kUrl3));
  result.Append(base::Value(kUrl4));

  // Populate separate policy bundles.
  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  // policy_bundle_1 is treated as a machine platform bundle. The metadata
  // policies (PolicyListMultipleSourceMergeList, CloudUserPolicyMerge) are
  // defined here.
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kCloudUserPolicyMerge, base::Value(true));
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a machine cloud bundle. The device
  // affiliation IDs are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);
  policy_bundle_2->Get(chrome_namespace).SetDeviceAffiliationIds(ids);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionInstallForcelist, list3.Clone());
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(ids);

  // The expected_chrome PolicyMap contains the combined URLs from all three
  // policy bundles. The affiliation IDs don't need to be added as they're not
  // compared in the PolicyMap equality check.
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));
  expected_chrome.Set(key::kCloudUserPolicyMerge,
                      policy_bundle_1->Get(chrome_namespace)
                          .Get(key::kCloudUserPolicyMerge)
                          ->DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_3));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, CloudUserListPolicyMerge_Unaffiliated) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize user and device affiliation IDs with no common ID.
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);
  base::flat_set<std::string> device_ids;
  device_ids.insert(kAffiliationId2);

  // Initialize lists of URLs used for ExtensionInstallForcelist policy values.
  base::Value list1 = base::Value(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl1));
  list1.Append(base::Value(kUrl2));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl3));
  base::Value list3 = base::Value(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl4));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl1));
  result.Append(base::Value(kUrl2));
  result.Append(base::Value(kUrl3));

  // Populate separate policy bundles.
  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  // policy_bundle_1 is treated as a machine platform bundle. The metadata
  // policies (PolicyListMultipleSourceMergeList, CloudUserPolicyMerge) are
  // defined here.
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kCloudUserPolicyMerge, base::Value(true));
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a machine cloud bundle. The device
  // affiliation IDs are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);
  policy_bundle_2->Get(chrome_namespace).SetDeviceAffiliationIds(device_ids);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionInstallForcelist, list3.Clone());
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(user_ids);

  // The expected_chrome PolicyMap contains the combined URLs from the non-user
  // policy bundles. The policy values from the user cloud bundle aren't merged
  // as there is no common affiliation ID between the user and device.
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));
  expected_chrome.Set(key::kCloudUserPolicyMerge,
                      policy_bundle_1->Get(chrome_namespace)
                          .Get(key::kCloudUserPolicyMerge)
                          ->DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_3));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, CloudUserListPolicyMerge_FalsePolicy) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize affiliation IDs. User and device ID is identical.
  base::flat_set<std::string> ids;
  ids.insert(kAffiliationId1);

  // Initialize lists of URLs used for ExtensionInstallForcelist policy values.
  base::Value list1 = base::Value(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl1));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl2));
  base::Value list3 = base::Value(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl3));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl1));
  result.Append(base::Value(kUrl2));

  // Populate separate policy bundles.
  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  // policy_bundle_1 is treated as a machine platform bundle. The metadata
  // policies (PolicyListMultipleSourceMergeList, CloudUserPolicyMerge) are
  // defined here. CloudUserPolicyMerge is set to false, preventing user cloud
  // policy values from being merged with values from other sources.
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kCloudUserPolicyMerge, base::Value(false));
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a machine cloud bundle. The device
  // affiliation IDs are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);
  policy_bundle_2->Get(chrome_namespace).SetDeviceAffiliationIds(ids);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionInstallForcelist, list3.Clone());
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(ids);

  // The expected_chrome PolicyMap contains the combined URLs from the non-user
  // policy bundles. The policy values from the user cloud bundle aren't merged
  // because CloudUserPolicyMerge is set to false.
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));
  expected_chrome.Set(key::kCloudUserPolicyMerge,
                      policy_bundle_1->Get(chrome_namespace)
                          .Get(key::kCloudUserPolicyMerge)
                          ->DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_3));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, PlatformUserListPolicyMerge_Affiliated) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize affiliation IDs. User and device ID is identical.
  base::flat_set<std::string> ids;
  ids.insert(kAffiliationId1);

  // Initialize lists of URLs used for ExtensionInstallForcelist policy values.
  base::Value list1 = base::Value(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl1));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl2));
  base::Value list3 = base::Value(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl3));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl2));
  result.Append(base::Value(kUrl3));

  // Populate separate policy bundles.
  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  // policy_bundle_1 is treated as a user platform bundle. The metadata policies
  // (PolicyListMultipleSourceMergeList, CloudUserPolicyMerge) are defined here.
  // Policy values with a user GPO source are currently not merged with values
  // from any other source(s).
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kCloudUserPolicyMerge, base::Value(true));
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
                                      std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a machine cloud bundle. The device
  // affiliation IDs are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                      std::move(policies_2), chrome_namespace);
  policy_bundle_2->Get(chrome_namespace).SetDeviceAffiliationIds(ids);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.t,
  // entry_list_3.DeepCopy()); policy_map_3.SetUserAffiliationIds(ids);
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionInstallForcelist, list3.Clone());
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(ids);

  // The expected_chrome PolicyMap contains the merged values from machine and
  // user policy sources. User platform policy values are not merged.
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));
  expected_chrome.Set(key::kCloudUserPolicyMerge,
                      policy_bundle_1->Get(chrome_namespace)
                          .Get(key::kCloudUserPolicyMerge)
                          ->DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_3));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, PlatformUserListPolicyMerge_Unaffiliated) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Initialize user affiliation IDs. This test doesn't contain a machine cloud
  // source, so no device affiliation IDs are set.
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);

  // Initialize lists of URLs used for ExtensionInstallForcelist policy values.
  base::Value list1 = base::Value(base::Value::Type::LIST);
  list1.Append(base::Value(kUrl1));
  base::Value list2 = base::Value(base::Value::Type::LIST);
  list2.Append(base::Value(kUrl2));
  base::Value list3 = base::Value(base::Value::Type::LIST);
  list3.Append(base::Value(kUrl3));
  base::Value result = base::Value(base::Value::Type::LIST);
  result.Append(base::Value(kUrl1));

  // Populate separate policy bundles.
  std::unique_ptr<base::ListValue> policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(policy::key::kExtensionInstallForcelist));

  // policy_bundle_1 is treated as a machine platform bundle. The metadata
  // policies (PolicyListMultipleSourceMergeList, CloudUserPolicyMerge) are
  // defined here.
  std::vector<std::pair<std::string, base::Value>> policies_1;
  policies_1.emplace_back(key::kPolicyListMultipleSourceMergeList,
                          policy->Clone());
  policies_1.emplace_back(key::kCloudUserPolicyMerge, base::Value(true));
  policies_1.emplace_back(key::kExtensionInstallForcelist, list1.Clone());
  auto policy_bundle_1 =
      CreateBundle(POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   std::move(policies_1), chrome_namespace);

  // policy_bundle_2 is treated as a user platform bundle. Policy values with a
  // user GPO source are currently not merged with values from any other
  // source(s).
  std::vector<std::pair<std::string, base::Value>> policies_2;
  policies_2.emplace_back(key::kExtensionInstallForcelist, list2.Clone());
  auto policy_bundle_2 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
                                      std::move(policies_2), chrome_namespace);

  // policy_bundle_3 is treated as a user cloud bundle. The user affiliation IDs
  // are defined here to reflect what would happen in reality.
  std::vector<std::pair<std::string, base::Value>> policies_3;
  policies_3.emplace_back(key::kExtensionInstallForcelist, list3.Clone());
  auto policy_bundle_3 = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                      std::move(policies_3), chrome_namespace);
  policy_bundle_3->Get(chrome_namespace).SetUserAffiliationIds(user_ids);

  // The expected_chrome PolicyMap only contains the URLs from the platform
  // machine policy source. Values from the user platform policy are not
  // mergeable. Values from the user cloud policy are not merged since the user
  // is not affiliated (browser isn't enrolled in CBCM).
  PolicyMap expected_chrome;
  expected_chrome.Set(key::kPolicyListMultipleSourceMergeList,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, policy->Clone(), nullptr);
  expected_chrome.Set("migrated", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(15), nullptr);

  PolicyMap::Entry merged(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_MERGED, std::move(result), nullptr);
  merged.AddConflictingPolicy(policy_bundle_2->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_3->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  merged.AddConflictingPolicy(policy_bundle_1->Get(chrome_namespace)
                                  .Get(key::kExtensionInstallForcelist)
                                  ->DeepCopy());
  expected_chrome.Set(key::kExtensionInstallForcelist, std::move(merged));
  expected_chrome.Set(key::kCloudUserPolicyMerge,
                      policy_bundle_1->Get(chrome_namespace)
                          .Get(key::kCloudUserPolicyMerge)
                          ->DeepCopy());

  provider0_.UpdatePolicy(std::move(policy_bundle_1));
  provider1_.UpdatePolicy(std::move(policy_bundle_2));
  provider2_.UpdatePolicy(std::move(policy_bundle_3));
  RunUntilIdle();

  EXPECT_TRUE(VerifyPolicies(chrome_namespace, expected_chrome));
}

TEST_F(PolicyServiceTest, IgnoreUserCloudPrecedencePolicies) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // The policies are set by a user cloud source.
  std::vector<std::pair<std::string, base::Value>> policies;
  policies.emplace_back(key::kCloudPolicyOverridesPlatformPolicy,
                        base::Value(true));
  policies.emplace_back(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                        base::Value(true));
  policies.emplace_back(key::kBookmarkBarEnabled, base::Value(true));
  auto policy_bundle = CreateBundle(POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                                    std::move(policies), chrome_namespace);

  provider0_.UpdatePolicy(std::move(policy_bundle));
  RunUntilIdle();

  // Precedence metapolicies set from a user cloud source are ignored.
  EXPECT_EQ(nullptr, policy_service_->GetPolicies(chrome_namespace)
                         .GetValue(key::kCloudPolicyOverridesPlatformPolicy));
  EXPECT_EQ(nullptr,
            policy_service_->GetPolicies(chrome_namespace)
                .GetValue(key::kCloudUserPolicyOverridesCloudMachinePolicy));

  // Other policies set from a user cloud source are not ignored.
  EXPECT_NE(nullptr, policy_service_->GetPolicies(chrome_namespace)
                         .GetValue(key::kBookmarkBarEnabled));
}

}  // namespace policy
