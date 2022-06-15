// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/feed_user_segment.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class FeedUserModelTest : public testing::Test {
 public:
  FeedUserModelTest() = default;
  ~FeedUserModelTest() override = default;

  void SetUp() override {
    feed_user_model_ = std::make_unique<FeedUserSegment>();
  }

  void TearDown() override {
    feed_user_model_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    feed_user_model_->InitAndFetchModel(
        base::BindRepeating(&FeedUserModelTest::OnInitFinishedCallback,
                            base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnInitFinishedCallback(base::RepeatingClosure closure,
                              proto::SegmentId target,
                              proto::SegmentationModelMetadata metadata,
                              int64_t) {
    EXPECT_EQ(metadata_utils::ValidateMetadataAndFeatures(metadata),
              metadata_utils::ValidationResult::kValidationSuccess);
    fetched_metadata_ = metadata;
    std::move(closure).Run();
  }

  absl::optional<float> ExpectExecutionWithInput(
      const std::vector<float>& inputs) {
    absl::optional<float> result;
    base::RunLoop loop;
    feed_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&FeedUserModelTest::OnExecutionFinishedCallback,
                       base::Unretained(this), loop.QuitClosure(), &result));
    loop.Run();
    return result;
  }

  void OnExecutionFinishedCallback(base::RepeatingClosure closure,
                                   absl::optional<float>* output,
                                   const absl::optional<float>& result) {
    *output = result;
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FeedUserSegment> feed_user_model_;
  absl::optional<proto::SegmentationModelMetadata> fetched_metadata_;
};

TEST_F(FeedUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FeedUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::vector<float> input(13, 0);

  absl::optional<float> result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "NoNTPOrHomeOpened",
      FeedUserSegment::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  input[4] = 3;
  input[5] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "UsedNtpWithoutModules",
      FeedUserSegment::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  input[3] = 3;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "MvtOnly",
      FeedUserSegment::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  input[10] = 3;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(
      "NtpAndFeedEngagedSimple",
      FeedUserSegment::GetSubsegmentName(metadata_utils::ConvertToDiscreteScore(
          "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  EXPECT_FALSE(ExpectExecutionWithInput({}));
  EXPECT_FALSE(ExpectExecutionWithInput({1, 2}));
}

}  // namespace segmentation_platform
