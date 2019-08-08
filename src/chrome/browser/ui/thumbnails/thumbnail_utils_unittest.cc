// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"

#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/surface/transport_dib.h"

using content::WebContents;

typedef testing::Test SimpleThumbnailCropTest;

namespace thumbnails {

// Test which generates thumbnails from various source scales.
class ThumbnailUtilsTest : public testing::TestWithParam<float> {
 public:
  ThumbnailUtilsTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThumbnailUtilsTest);
};

static float GetAspect(const gfx::Size& size) {
  return float{size.width()} / float{size.height()};
}

INSTANTIATE_TEST_SUITE_P(,
                         ThumbnailUtilsTest,
                         testing::ValuesIn({1.0f, 1.25f, 1.62f, 2.0f}));

TEST_P(ThumbnailUtilsTest, GetCanvasCopyInfo) {
  constexpr gfx::Size kThumbnailSize(200, 120);
  const float scale_factor = GetParam();
  const gfx::Size expected_size =
      gfx::ScaleToFlooredSize(kThumbnailSize, scale_factor);
  const float desired_aspect = GetAspect(kThumbnailSize);
  const gfx::Size wider_than_tall_source(400, 210);
  const gfx::Size much_wider_than_tall_source(600, 200);
  const gfx::Size taller_than_wide_source(300, 600);
  const gfx::Size small_source(200, 100);

  CanvasCopyInfo result =
      GetCanvasCopyInfo(wider_than_tall_source, scale_factor, kThumbnailSize);
  EXPECT_EQ(ClipResult::kSourceWiderThanTall, result.clip_result);
  EXPECT_EQ(expected_size, result.target_size);
  EXPECT_NEAR(desired_aspect, GetAspect(result.copy_rect.size()), 0.01);

  result = GetCanvasCopyInfo(much_wider_than_tall_source, scale_factor,
                             kThumbnailSize);
  EXPECT_EQ(ClipResult::kSourceMuchWiderThanTall, result.clip_result);
  EXPECT_EQ(expected_size, result.target_size);
  EXPECT_NEAR(desired_aspect, GetAspect(result.copy_rect.size()), 0.01);

  result =
      GetCanvasCopyInfo(taller_than_wide_source, scale_factor, kThumbnailSize);
  EXPECT_EQ(ClipResult::kSourceTallerThanWide, result.clip_result);
  EXPECT_EQ(expected_size, result.target_size);
  EXPECT_NEAR(desired_aspect, GetAspect(result.copy_rect.size()), 0.01);

  result = GetCanvasCopyInfo(small_source, scale_factor, kThumbnailSize);
  EXPECT_EQ(ClipResult::kSourceSmallerThanTarget, result.clip_result);
  EXPECT_EQ(expected_size, result.target_size);
}

}  // namespace thumbnails
