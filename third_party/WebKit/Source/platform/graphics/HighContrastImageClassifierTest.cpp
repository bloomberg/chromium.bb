// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/HighContrastImageClassifier.h"

#include "platform/SharedBuffer.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HighContrastImageClassifierTest : public ::testing::Test {
 public:
  // Loads the image from |file_name|, computes features vector into |features|,
  // and returns the classification result.
  bool GetFeaturesAndClassification(const std::string& file_name,
                                    std::vector<float>* features) {
    SCOPED_TRACE(file_name);
    RefPtr<BitmapImage> image = LoadImage(file_name);
    classifier_.ComputeImageFeaturesForTesting(*image.get(), features);
    return classifier_.ShouldApplyHighContrastFilterToImage(*image.get());
  }

  // Compares two vectors of computed features and expected ones. Comparison is
  // done with 0.000001 precision to ignore minor, negligible differences.
  bool AreFeaturesEqual(const std::vector<float>& features,
                        const std::vector<float>& expected_features) {
    if (features.size() != expected_features.size()) {
      LOG(ERROR) << "Different sized vectors.";
      return false;
    }
    for (unsigned i = 0; i < features.size(); i++) {
      if (fabs(features[i] - expected_features[i]) > 0.000001) {
        LOG(ERROR) << "Different values at index " << i << ", " << features[i]
                   << " vs " << expected_features[i];
        return false;
      }
    }
    return true;
  }

 protected:
  RefPtr<BitmapImage> LoadImage(const std::string& file_name) {
    String file_path = testing::BlinkRootDir();
    file_path.append(file_name.c_str());
    RefPtr<SharedBuffer> image_data = testing::ReadFromFile(file_path);
    EXPECT_TRUE(image_data.get());

    RefPtr<BitmapImage> image = BitmapImage::Create();
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
  EXPECT_TRUE(AreFeaturesEqual(features, {1.0f, 0.0336914f}));

  // Test Case 2:
  EXPECT_TRUE(GetFeaturesAndClassification(
      "/LayoutTests/images/resources/grid-large.png", &features));
  EXPECT_TRUE(AreFeaturesEqual(features, {0.0f, 0.1875f}));
}

}  // namespace blink
