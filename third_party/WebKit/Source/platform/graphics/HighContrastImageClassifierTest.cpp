// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/HighContrastImageClassifier.h"

#include "platform/SharedBuffer.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const float kEpsilon = 0.00001;

}  // namespace

namespace blink {

class HighContrastImageClassifierTest : public ::testing::Test {
 public:
  // Loads the image from |file_name|, computes features vector into |features|,
  // and returns the classification result.
  bool GetFeaturesAndClassification(const std::string& file_name,
                                    std::vector<float>* features) {
    SCOPED_TRACE(file_name);
    scoped_refptr<BitmapImage> image = LoadImage(file_name);
    classifier_.SetRandomGeneratorForTesting();
    classifier_.ComputeImageFeaturesForTesting(*image.get(), features);
    return classifier_.ShouldApplyHighContrastFilterToImage(*image.get());
  }

  void AssertFeaturesEqual(const std::vector<float>& features,
                           const std::vector<float>& expected_features) {
    EXPECT_EQ(features.size(), expected_features.size());
    for (unsigned i = 0; i < features.size(); i++) {
      EXPECT_NEAR(features[i], expected_features[i], kEpsilon)
          << "Feature " << i;
    }
  }

 protected:
  scoped_refptr<BitmapImage> LoadImage(const std::string& file_name) {
    String file_path = testing::BlinkRootDir();
    file_path.append(file_name.c_str());
    scoped_refptr<SharedBuffer> image_data = testing::ReadFromFile(file_path);
    EXPECT_TRUE(image_data.get());

    scoped_refptr<BitmapImage> image = BitmapImage::Create();
    image->SetData(image_data, true);
    return image;
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  HighContrastImageClassifier classifier_;
};

TEST_F(HighContrastImageClassifierTest, FeaturesAndClassification) {
  std::vector<float> features;

  // Test Case 1:
  EXPECT_FALSE(GetFeaturesAndClassification(
      "/LayoutTests/images/resources/blue-wheel-srgb-color-profile.png",
      &features));
  AssertFeaturesEqual(features, {1.0f, 0.03027f, 0.0f, 0.24f});

  // Test Case 2:
  EXPECT_TRUE(GetFeaturesAndClassification(
      "/LayoutTests/images/resources/grid-large.png", &features));
  AssertFeaturesEqual(features, {0.0f, 0.1875f, 0.0f, 0.1f});
}

}  // namespace blink
