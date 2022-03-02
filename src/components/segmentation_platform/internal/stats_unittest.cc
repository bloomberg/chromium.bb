// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
using proto::SignalType;
namespace stats {

class StatsTest : public testing::Test {
 public:
  ~StatsTest() override = default;
};

TEST_F(StatsTest, ModelExecutionZeroValuePercent) {
  base::HistogramTester tester;
  std::vector<float> empty{};
  std::vector<float> single_zero{0};
  std::vector<float> single_non_zero{1};
  std::vector<float> all_zeroes{0, 0, 0};
  std::vector<float> one_non_zero{0, 2, 0};
  std::vector<float> all_non_zero{1, 2, 3};

  RecordModelExecutionZeroValuePercent(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, empty);
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));

  RecordModelExecutionZeroValuePercent(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      single_zero);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 100));

  RecordModelExecutionZeroValuePercent(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      single_non_zero);
  EXPECT_EQ(
      2, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));

  RecordModelExecutionZeroValuePercent(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, all_zeroes);
  EXPECT_EQ(
      2,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 100));

  RecordModelExecutionZeroValuePercent(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      one_non_zero);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 66));

  RecordModelExecutionZeroValuePercent(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      all_non_zero);
  EXPECT_EQ(
      3, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));
}

TEST_F(StatsTest, AdaptiveToolbarSegmentSwitch) {
  std::string histogram("SegmentationPlatform.AdaptiveToolbar.SegmentSwitched");
  base::HistogramTester tester;

  // Share -> New tab.
  RecordSegmentSelectionComputed(
      kAdaptiveToolbarSegmentationKey,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  // None -> Share.
  RecordSegmentSelectionComputed(
      kAdaptiveToolbarSegmentationKey,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      absl::nullopt);

  // Share -> Share.
  RecordSegmentSelectionComputed(
      kAdaptiveToolbarSegmentationKey,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kNoneToShare), 1),
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kShareToNewTab),
              1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed", 3);
}

TEST_F(StatsTest, BooleanSegmentSwitch) {
  std::string histogram(
      "SegmentationPlatform.ChromeStartAndroid.SegmentSwitched");
  base::HistogramTester tester;

  // Start to none.
  RecordSegmentSelectionComputed(
      kChromeStartAndroidSegmentationKey,
      OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN,
      OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID);

  tester.ExpectTotalCount(histogram, 1);
  EXPECT_THAT(tester.GetAllSamples(histogram),
              testing::ElementsAre(base::Bucket(
                  static_cast<int>(BooleanSegmentSwitch::kEnabledToNone), 1)));
  // None to start.
  RecordSegmentSelectionComputed(
      kChromeStartAndroidSegmentationKey,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      absl::nullopt);

  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(BooleanSegmentSwitch::kNoneToEnabled),
                       1),
          base::Bucket(static_cast<int>(BooleanSegmentSwitch::kEnabledToNone),
                       1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.ChromeStartAndroid.SegmentSelection.Computed2", 2);
}

TEST_F(StatsTest, SignalsListeningCount) {
  base::HistogramTester tester;
  std::set<uint64_t> user_actions{1, 2, 3, 4};
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  histograms.insert(std::make_pair("hist1", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist2", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist3", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist4", SignalType::HISTOGRAM_VALUE));
  histograms.insert(std::make_pair("hist5", SignalType::HISTOGRAM_VALUE));

  RecordSignalsListeningCount(user_actions, histograms);

  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.Signals.ListeningCount.UserAction", 4));
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramEnum", 3));
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramValue", 2));
}

}  // namespace stats
}  // namespace segmentation_platform
