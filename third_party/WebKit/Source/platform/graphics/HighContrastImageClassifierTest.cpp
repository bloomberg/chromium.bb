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
  RefPtr<BitmapImage> LoadImage(const char* file_name) {
    String file_path = testing::BlinkRootDir();
    file_path.append(file_name);
    RefPtr<SharedBuffer> image_data = testing::ReadFromFile(file_path);
    EXPECT_TRUE(image_data.Get());

    RefPtr<BitmapImage> image = BitmapImage::Create();
    image->SetData(image_data, true);
    return image;
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  HighContrastImageClassifier classifier_;
};

TEST_F(HighContrastImageClassifierTest, ShouldApplyHighContrastFilterToImage) {
  RefPtr<BitmapImage> image = LoadImage(
      "/LayoutTests/images/resources/blue-wheel-srgb-color-profile.png");
  EXPECT_FALSE(classifier_.ShouldApplyHighContrastFilterToImage(*image.Get()));

  image = LoadImage("/LayoutTests/images/resources/grid.png");
  EXPECT_TRUE(classifier_.ShouldApplyHighContrastFilterToImage(*image.Get()));
}

}  // namespace blink
