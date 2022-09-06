// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"

#include <cmath>

#include "base/bit_cast.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using Segmentation_ModelExecution = ukm::builders::Segmentation_ModelExecution;

namespace segmentation_platform {

namespace {

// Round errors allowed during conversion.
static const double kRoundingError = 1E-5;

float Int64ToFloat(int64_t encoded) {
  return static_cast<float>(base::bit_cast<double>(encoded));
}

void CompareEncodeDecodeDifference(float tensor) {
  ASSERT_LT(
      std::abs(tensor -
               Int64ToFloat(
                   segmentation_platform::SegmentationUkmHelper::FloatToInt64(
                       tensor))),
      kRoundingError);
}

absl::optional<proto::PredictionResult> GetPredictionResult() {
  proto::PredictionResult result;
  result.set_result(0.5);
  return result;
}

}  // namespace

class SegmentationUkmHelperTest : public testing::Test {
 public:
  SegmentationUkmHelperTest() = default;

  SegmentationUkmHelperTest(const SegmentationUkmHelperTest&) = delete;
  SegmentationUkmHelperTest& operator=(const SegmentationUkmHelperTest&) =
      delete;

  ~SegmentationUkmHelperTest() override = default;

  void SetUp() override { test_recorder_.Purge(); }

  void ExpectUkmMetrics(const base::StringPiece entry_name,
                        const std::vector<base::StringPiece>& keys,
                        const std::vector<int64_t>& values) {
    const auto& entries = test_recorder_.GetEntriesByName(entry_name);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
      const size_t keys_size = keys.size();
      EXPECT_EQ(keys_size, values.size());
      for (size_t i = 0; i < keys_size; ++i) {
        test_recorder_.ExpectEntryMetric(entry, keys[i], values[i]);
      }
    }
  }

  void ExpectEmptyUkmMetrics(const base::StringPiece entry_name) {
    EXPECT_EQ(0u, test_recorder_.GetEntriesByName(entry_name).size());
  }

  void InitializeAllowedSegmentIds(const std::string& allowed_ids) {
    std::map<std::string, std::string> params = {
        {kSegmentIdsAllowedForReportingKey, allowed_ids}};
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSegmentationStructuredMetricsFeature, params);
    SegmentationUkmHelper::GetInstance()->Initialize();
  }

  void DisableStructureMetrics() {
    feature_list_.InitAndDisableFeature(
        features::kSegmentationStructuredMetricsFeature);
    SegmentationUkmHelper::GetInstance()->Initialize();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that basic execution results recording works properly.
TEST_F(SegmentationUkmHelperTest, TestExecutionResultReporting) {
  // Allow results for OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB to be recorded.
  InitializeAllowedSegmentIds("4");
  std::vector<float> input_tensors = {0.1, 0.7, 0.8, 0.5};
  SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors, 0.6);
  ExpectUkmMetrics(Segmentation_ModelExecution::kEntryName,
                   {Segmentation_ModelExecution::kOptimizationTargetName,
                    Segmentation_ModelExecution::kModelVersionName,
                    Segmentation_ModelExecution::kInput0Name,
                    Segmentation_ModelExecution::kInput1Name,
                    Segmentation_ModelExecution::kInput2Name,
                    Segmentation_ModelExecution::kInput3Name,
                    Segmentation_ModelExecution::kPredictionResultName},
                   {
                       proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                       101,
                       SegmentationUkmHelper::FloatToInt64(0.1),
                       SegmentationUkmHelper::FloatToInt64(0.7),
                       SegmentationUkmHelper::FloatToInt64(0.8),
                       SegmentationUkmHelper::FloatToInt64(0.5),
                       SegmentationUkmHelper::FloatToInt64(0.6),
                   });
}

// Tests that the training data collection recording works properly.
TEST_F(SegmentationUkmHelperTest, TestTrainingDataCollectionReporting) {
  // Allow results for OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB to be recorded.
  InitializeAllowedSegmentIds("4");
  std::vector<float> input_tensors = {0.1};
  std::vector<float> outputs = {1.0, 0.0};
  std::vector<int> output_indexes = {2, 3};

  SelectedSegment selected_segment(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  selected_segment.selection_time = base::Time::Now() - base::Seconds(10);
  SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
      outputs, output_indexes, GetPredictionResult(), selected_segment);
  ExpectUkmMetrics(Segmentation_ModelExecution::kEntryName,
                   {Segmentation_ModelExecution::kOptimizationTargetName,
                    Segmentation_ModelExecution::kModelVersionName,
                    Segmentation_ModelExecution::kInput0Name,
                    Segmentation_ModelExecution::kActualResult3Name,
                    Segmentation_ModelExecution::kActualResult4Name,
                    Segmentation_ModelExecution::kPredictionResultName,
                    Segmentation_ModelExecution::kSelectionResultName,
                    Segmentation_ModelExecution::kOutputDelaySecName},
                   {
                       proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                       101,
                       SegmentationUkmHelper::FloatToInt64(0.1),
                       SegmentationUkmHelper::FloatToInt64(1.0),
                       SegmentationUkmHelper::FloatToInt64(0.0),
                       SegmentationUkmHelper::FloatToInt64(0.5),
                       proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                       10,
                   });
}

// Tests that recording is disabled if kSegmentationStructuredMetricsFeature
// is disabled.
TEST_F(SegmentationUkmHelperTest, TestDisabledStructuredMetrics) {
  DisableStructureMetrics();
  std::vector<float> input_tensors = {0.1, 0.7, 0.8, 0.5};
  SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors, 0.6);
  ExpectEmptyUkmMetrics(Segmentation_ModelExecution::kEntryName);
}

// Tests that recording is disabled for segment IDs that are not in the allowed
// list.
TEST_F(SegmentationUkmHelperTest, TestNotAllowedSegmentId) {
  InitializeAllowedSegmentIds("7, 8");
  std::vector<float> input_tensors = {0.1, 0.7, 0.8, 0.5};
  SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors, 0.6);
  ExpectEmptyUkmMetrics(Segmentation_ModelExecution::kEntryName);
}

// Tests that float encoding works properly.
TEST_F(SegmentationUkmHelperTest, TestFloatEncoding) {
  // Compare the numbers with their IEEE754 binary representations in double.
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.5), 0x3FE0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.25), 0x3FD0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.125), 0x3FC0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0.75), 0x3FE8000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(1), 0x3FF0000000000000);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(0), 0);
  ASSERT_EQ(SegmentationUkmHelper::FloatToInt64(10), 0x4024000000000000);
}

// Tests that floats encoded can be properly decoded later.
TEST_F(SegmentationUkmHelperTest, FloatEncodeDeocode) {
  CompareEncodeDecodeDifference(0.1);
  CompareEncodeDecodeDifference(0.5);
  CompareEncodeDecodeDifference(0.88);
  CompareEncodeDecodeDifference(0.01);
  ASSERT_EQ(0, Int64ToFloat(SegmentationUkmHelper::FloatToInt64(0)));
  ASSERT_EQ(1, Int64ToFloat(SegmentationUkmHelper::FloatToInt64(1)));
}

// Tests that there are too many input tensors to record.
TEST_F(SegmentationUkmHelperTest, TooManyInputTensors) {
  base::HistogramTester tester;
  std::string histogram_name(
      "SegmentationPlatform.StructuredMetrics.TooManyTensors.Count");
  InitializeAllowedSegmentIds("4");
  std::vector<float> input_tensors(100, 0.1);
  ukm::SourceId source_id =
      SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
          proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
          0.6);
  ASSERT_EQ(source_id, ukm::kInvalidSourceId);
  tester.ExpectTotalCount(histogram_name, 1);
  ASSERT_EQ(tester.GetTotalSum(histogram_name), 100);
}

// Tests output validation for |RecordTrainingData|.
TEST_F(SegmentationUkmHelperTest, OutputsValidation) {
  InitializeAllowedSegmentIds("4");
  std::vector<float> input_tensors{0.1};

  // outputs, output_indexes size doesn't match.
  std::vector<float> outputs{1.0, 0.0};
  std::vector<int> output_indexes{0};

  ukm::SourceId source_id =
      SegmentationUkmHelper::GetInstance()->RecordTrainingData(
          proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
          outputs, output_indexes, GetPredictionResult(), absl::nullopt);
  ASSERT_EQ(source_id, ukm::kInvalidSourceId);

  // output_indexes value too large.
  output_indexes = {100, 1000};
  source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
      outputs, output_indexes, GetPredictionResult(), absl::nullopt);
  ASSERT_EQ(source_id, ukm::kInvalidSourceId);

  // Valid outputs.
  output_indexes = {3, 0};
  source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 101, input_tensors,
      outputs, output_indexes, GetPredictionResult(), absl::nullopt);
  ASSERT_NE(source_id, ukm::kInvalidSourceId);
}

TEST_F(SegmentationUkmHelperTest, AllowedToUploadData) {
  TestingPrefServiceSimple prefs;
  SegmentationPlatformService::RegisterLocalStatePrefs(prefs.registry());
  LocalStateHelper::GetInstance().Initialize(&prefs);
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());

  // If pref is not initialized, AllowedToUploadData() always return false.
  ASSERT_FALSE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(1), &clock));

  LocalStateHelper::GetInstance().SetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey, clock.Now());

  ASSERT_FALSE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(1), &clock));
  clock.Advance(base::Seconds(10));
  ASSERT_TRUE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(1), &clock));
  ASSERT_FALSE(
      SegmentationUkmHelper::AllowedToUploadData(base::Seconds(11), &clock));
}

}  // namespace segmentation_platform
