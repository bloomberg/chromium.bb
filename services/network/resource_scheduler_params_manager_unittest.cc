// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler_params_manager.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class ResourceSchedulerParamsManagerTest : public testing::Test {
 public:
  ResourceSchedulerParamsManagerTest() : field_trial_list_(nullptr) {}

  ~ResourceSchedulerParamsManagerTest() override {}

  void ReadConfigTestHelper(size_t num_ranges) {
    const char kTrialName[] = "TrialName";
    const char kGroupName[] = "GroupName";
    const char kThrottleDelayable[] = "ThrottleDelayable";

    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> params;
    int index = 1;
    net::EffectiveConnectionType effective_connection_type =
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    while (effective_connection_type <
           net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G + num_ranges) {
      std::string index_str = base::NumberToString(index);
      params["EffectiveConnectionType" + index_str] =
          net::GetNameForEffectiveConnectionType(effective_connection_type);
      params["MaxDelayableRequests" + index_str] = index_str + "0";
      params["NonDelayableWeight" + index_str] = "0";
      effective_connection_type = static_cast<net::EffectiveConnectionType>(
          static_cast<int>(effective_connection_type) + 1);
      ++index;
    }

    base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
    std::unique_ptr<base::FeatureList> feature_list(
        std::make_unique<base::FeatureList>());
    feature_list->RegisterFieldTrialOverride(
        kThrottleDelayable, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        field_trial);
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    ResourceSchedulerParamsManager resource_scheduler_params_manager;

    effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    while (effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST) {
      if (effective_connection_type <
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G + num_ranges) {
        int index = static_cast<int>(effective_connection_type) - 1;
        EXPECT_EQ(index * 10u, resource_scheduler_params_manager
                                   .GetParamsForEffectiveConnectionType(
                                       effective_connection_type)
                                   .max_delayable_requests);
        EXPECT_EQ(0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(
                             effective_connection_type)
                         .non_delayable_weight);
      } else {
        VerifyDefaultParams(resource_scheduler_params_manager,
                            effective_connection_type);
      }
      effective_connection_type = static_cast<net::EffectiveConnectionType>(
          static_cast<int>(effective_connection_type) + 1);
    }
  }

  void VerifyDefaultParams(
      const ResourceSchedulerParamsManager& resource_scheduler_params_manager,
      net::EffectiveConnectionType effective_connection_type) const {
    switch (effective_connection_type) {
      case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
      case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
      case net::EFFECTIVE_CONNECTION_TYPE_3G:
      case net::EFFECTIVE_CONNECTION_TYPE_4G:
        EXPECT_EQ(10u, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .max_delayable_requests);
        EXPECT_EQ(0.0, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .non_delayable_weight);
        return;

      case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
      case net::EFFECTIVE_CONNECTION_TYPE_2G:
        EXPECT_EQ(8u, resource_scheduler_params_manager
                          .GetParamsForEffectiveConnectionType(
                              effective_connection_type)
                          .max_delayable_requests);
        EXPECT_EQ(3.0, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .non_delayable_weight);
        return;
      case net::EFFECTIVE_CONNECTION_TYPE_LAST:
        NOTREACHED();
        return;
    }
  }

 private:
  base::FieldTrialList field_trial_list_;
  DISALLOW_COPY_AND_ASSIGN(ResourceSchedulerParamsManagerTest);
};

TEST_F(ResourceSchedulerParamsManagerTest, VerifyAllDefaultParams) {
  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  for (int effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST;
       ++effective_connection_type) {
    VerifyDefaultParams(
        resource_scheduler_params_manager,
        static_cast<net::EffectiveConnectionType>(effective_connection_type));
  }
}

// Test that a configuration with bad strings does not break the parser, and
// the parser stops reading the configuration after it encounters the first
// missing index.
TEST_F(ResourceSchedulerParamsManagerTest, ReadInvalidConfigTest) {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  const char kTrialName[] = "TrialName";
  const char kGroupName[] = "GroupName";
  const char kThrottleDelayable[] = "ThrottleDelayable";

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> params;
  // Skip configuration parameters for index 2 to test that the parser stops
  // when it cannot find the parameters for an index.
  for (int range_index : {1, 3, 4}) {
    std::string index_str = base::IntToString(range_index);
    params["EffectiveConnectionType" + index_str] = "Slow-2G";
    params["MaxDelayableRequests" + index_str] = index_str + "0";
    params["NonDelayableWeight" + index_str] = "0";
  }
  // Add some bad configuration strigs to ensure that the parser does not break.
  params["BadConfigParam1"] = "100";
  params["BadConfigParam2"] = "100";

  base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  std::unique_ptr<base::FeatureList> feature_list(
      std::make_unique<base::FeatureList>());
  feature_list->RegisterFieldTrialOverride(
      kThrottleDelayable, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      field_trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  // Only the first configuration parameter must be read because a match was not
  // found for index 2. The configuration parameters with index 3 and 4 must be
  // ignored, even though they are valid configuration parameters.
  EXPECT_EQ(10u, resource_scheduler_params_manager
                     .GetParamsForEffectiveConnectionType(
                         net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                     .max_delayable_requests);
  EXPECT_EQ(0.0, resource_scheduler_params_manager
                     .GetParamsForEffectiveConnectionType(
                         net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                     .non_delayable_weight);

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_2G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_3G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_4G);
}

// Test that a configuration with 2 ranges is read correctly.
TEST_F(ResourceSchedulerParamsManagerTest, ReadValidConfigTest2) {
  ReadConfigTestHelper(2);
}

// Test that a configuration with 3 ranges is read correctly.
TEST_F(ResourceSchedulerParamsManagerTest, ReadValidConfigTest3) {
  ReadConfigTestHelper(3);
}

TEST_F(ResourceSchedulerParamsManagerTest, ThrottleDelayableDisabled) {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

  const char kTrialName[] = "TrialName";
  const char kGroupName[] = "GroupName";

  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  base::test::ScopedFeatureList scoped_feature_list;

  std::unique_ptr<base::FeatureList> feature_list(
      std::make_unique<base::FeatureList>());

  feature_list->RegisterFieldTrialOverride(
      "ThrottleDelayable", base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      field_trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_2G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_3G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_4G);
}

TEST_F(ResourceSchedulerParamsManagerTest,
       MaxDelayableRequestsAndNonDelayableWeightSet) {
  base::test::ScopedFeatureList scoped_feature_list;

  std::map<std::string, std::string> params;

  params["EffectiveConnectionType1"] = "Slow-2G";
  size_t max_delayable_requests_slow_2g = 2u;
  double non_delayable_weight_slow_2g = 2.0;
  params["MaxDelayableRequests1"] =
      base::NumberToString(max_delayable_requests_slow_2g);
  params["NonDelayableWeight1"] =
      base::NumberToString(non_delayable_weight_slow_2g);

  params["EffectiveConnectionType2"] = "3G";
  size_t max_delayable_requests_3g = 4u;
  double non_delayable_weight_3g = 0.0;
  params["MaxDelayableRequests2"] =
      base::NumberToString(max_delayable_requests_3g);
  params["NonDelayableWeight2"] = base::NumberToString(non_delayable_weight_3g);

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  const char kTrialName[] = "TrialName";
  const char kGroupName[] = "GroupName";

  ASSERT_TRUE(base::AssociateFieldTrialParams(kTrialName, kGroupName, params));
  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  ASSERT_TRUE(field_trial);

  std::unique_ptr<base::FeatureList> feature_list(
      std::make_unique<base::FeatureList>());

  feature_list->RegisterFieldTrialOverride(
      "ThrottleDelayable", base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      field_trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  EXPECT_EQ(max_delayable_requests_slow_2g,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                .max_delayable_requests);
  EXPECT_EQ(non_delayable_weight_slow_2g,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                .non_delayable_weight);

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_2G);

  EXPECT_EQ(max_delayable_requests_3g,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_3G)
                .max_delayable_requests);
  EXPECT_EQ(non_delayable_weight_3g, resource_scheduler_params_manager
                                         .GetParamsForEffectiveConnectionType(
                                             net::EFFECTIVE_CONNECTION_TYPE_3G)
                                         .non_delayable_weight);

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_4G);
}

}  // unnamed namespace

}  // namespace network
