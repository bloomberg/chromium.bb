// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_FEED_USER_SEGMENT_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_FEED_USER_SEGMENT_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// List of sub-segments for Feed segment.
enum class FeedUserSubsegment {
  kUnknown = 0,
  kOther = 1,
  kActiveOnFeedOnly = 2,
  kActiveOnFeedAndNtpFeatures = 3,
  kNoFeedAndNtpFeatures = 4,
  kMvtOnly = 5,
  kReturnToCurrentTabOnly = 6,
  kUsedNtpWithoutModules = 7,
  kNoNTPOrHomeOpened = 8,
  kMaxValue = kNoNTPOrHomeOpened
};

// Segmentation Chrome Feed user model provider. Provides a default model and
// metadata for the Feed user optimization target.
class FeedUserSegment : public ModelProvider {
 public:
  FeedUserSegment();
  ~FeedUserSegment() override = default;

  FeedUserSegment(FeedUserSegment&) = delete;
  FeedUserSegment& operator=(FeedUserSegment&) = delete;

  // Returns the name of the subsegment for the given segment and the
  // `subsegment_rank`. The `subsegment_rank` should be computed based on the
  // subsegment discrete mapping in the model metadata.
  static absl::optional<std::string> GetSubsegmentName(int subsegment_rank);

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_DEFAULT_MODEL_FEED_USER_SEGMENT_H_
