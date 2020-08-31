// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/store_update_data.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(StoreUpdateDataTest, BuildComponentStoreUpdateData) {
  // Verify creating a Component Hint update package.
  base::Version v1("1.2.3.4");
  proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");
  proto::Hint hint2;
  hint2.set_key("bar.com");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  proto::PageHint* page_hint2 = hint2.add_page_hints();
  page_hint2->set_page_pattern("slowpagealso");

  std::unique_ptr<StoreUpdateData> component_update =
      StoreUpdateData::CreateComponentStoreUpdateData(v1);
  component_update->MoveHintIntoUpdateData(std::move(hint1));
  component_update->MoveHintIntoUpdateData(std::move(hint2));
  EXPECT_TRUE(component_update->component_version().has_value());
  EXPECT_FALSE(component_update->update_time().has_value());
  EXPECT_EQ(v1, *component_update->component_version());
  // Verify there are 3 store entries: 1 for the metadata entry plus
  // the 2 added hint entries.
  EXPECT_EQ(3ul, component_update->TakeUpdateEntries()->size());
}

TEST(StoreUpdateDataTest, BuildFetchUpdateDataUsesDefaultCacheDuration) {
  // Verify creating a Fetched Hint update package.
  base::Time update_time = base::Time::Now();
  proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");

  std::unique_ptr<StoreUpdateData> fetch_update =
      StoreUpdateData::CreateFetchedStoreUpdateData(update_time);
  fetch_update->MoveHintIntoUpdateData(std::move(hint1));
  EXPECT_FALSE(fetch_update->component_version().has_value());
  EXPECT_TRUE(fetch_update->update_time().has_value());
  EXPECT_EQ(update_time, *fetch_update->update_time());
  // Verify there are 2 store entries: 1 for the metadata entry plus
  // the 1 added hint entries.
  const auto update_entries = fetch_update->TakeUpdateEntries();
  EXPECT_EQ(2ul, update_entries->size());
  // Verify expiry time taken from hint rather than the default expiry time of
  // the store update data.
  for (const auto& entry : *update_entries) {
    proto::StoreEntry store_entry = entry.second;
    if (store_entry.entry_type() == proto::FETCHED_HINT) {
      base::Time expected_expiry_time =
          base::Time::Now() + features::StoredFetchedHintsFreshnessDuration();
      EXPECT_EQ(expected_expiry_time.ToDeltaSinceWindowsEpoch().InSeconds(),
                store_entry.expiry_time_secs());
      break;
    }
  }
}

TEST(StoreUpdateDataTest,
     BuildFetchUpdateDataUsesCacheDurationFromHintIfAvailable) {
  // Verify creating a Fetched Hint update package.
  int max_cache_duration_secs = 60;
  base::Time update_time = base::Time::Now();
  proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  hint1.mutable_max_cache_duration()->set_seconds(max_cache_duration_secs);
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");

  std::unique_ptr<StoreUpdateData> fetch_update =
      StoreUpdateData::CreateFetchedStoreUpdateData(update_time);
  fetch_update->MoveHintIntoUpdateData(std::move(hint1));
  EXPECT_FALSE(fetch_update->component_version().has_value());
  EXPECT_TRUE(fetch_update->update_time().has_value());
  EXPECT_EQ(update_time, *fetch_update->update_time());
  // Verify there are 2 store entries: 1 for the metadata entry plus
  // the 1 added hint entries.
  const auto update_entries = fetch_update->TakeUpdateEntries();
  EXPECT_EQ(2ul, update_entries->size());
  // Verify expiry time taken from hint rather than the default expiry time of
  // the store update data.
  for (const auto& entry : *update_entries) {
    proto::StoreEntry store_entry = entry.second;
    if (store_entry.entry_type() == proto::FETCHED_HINT) {
      base::Time expected_expiry_time =
          base::Time::Now() +
          base::TimeDelta::FromSeconds(max_cache_duration_secs);
      EXPECT_EQ(expected_expiry_time.ToDeltaSinceWindowsEpoch().InSeconds(),
                store_entry.expiry_time_secs());
      break;
    }
  }
}

TEST(StoreUpdateDataTest, BuildPredictionModelUpdateData) {
  // Verify creating a Prediction Model update data.
  proto::PredictionModel prediction_model;

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_features(
      proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  model_info->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);

  std::unique_ptr<StoreUpdateData> prediction_model_update =
      StoreUpdateData::CreatePredictionModelStoreUpdateData();
  prediction_model_update->CopyPredictionModelIntoUpdateData(prediction_model);
  EXPECT_FALSE(prediction_model_update->component_version().has_value());
  EXPECT_FALSE(prediction_model_update->update_time().has_value());
  // Verify there is 1 store entry.
  EXPECT_EQ(1ul, prediction_model_update->TakeUpdateEntries()->size());
}

TEST(StoreUpdateDataTest, BuildHostModelFeaturesUpdateData) {
  // Verify creating a Prediction Model update data.
  base::Time host_model_features_update_time = base::Time::Now();

  proto::HostModelFeatures host_model_features;
  host_model_features.set_host("foo.com");
  proto::ModelFeature* model_feature = host_model_features.add_model_features();
  model_feature->set_feature_name("host_feat1");
  model_feature->set_double_value(2.0);

  std::unique_ptr<StoreUpdateData> host_model_features_update =
      StoreUpdateData::CreateHostModelFeaturesStoreUpdateData(
          host_model_features_update_time,
          host_model_features_update_time +
              optimization_guide::features::
                  StoredHostModelFeaturesFreshnessDuration());
  host_model_features_update->CopyHostModelFeaturesIntoUpdateData(
      std::move(host_model_features));
  EXPECT_FALSE(host_model_features_update->component_version().has_value());
  EXPECT_TRUE(host_model_features_update->update_time().has_value());
  EXPECT_EQ(host_model_features_update_time,
            *host_model_features_update->update_time());
  // Verify there are 2 store entries, 1 for the metadata entry and 1 for the
  // added host model features entry.
  EXPECT_EQ(2ul, host_model_features_update->TakeUpdateEntries()->size());
}

}  // namespace

}  // namespace optimization_guide
