// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/discardable_image_utils.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/src/core/SkOrderedReadBuffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace skia {

namespace {

class TestImageGenerator : public SkImageGenerator {
 public:
  TestImageGenerator(const SkImageInfo& info) : SkImageGenerator(info) {}
};

skia::RefPtr<SkImage> CreateDiscardableImage(const gfx::Size& size) {
  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(size.width(), size.height());
  return skia::AdoptRef(
      SkImage::NewFromGenerator(new TestImageGenerator(info)));
}

SkCanvas* StartRecording(SkPictureRecorder* recorder, gfx::Rect layer_rect) {
  SkCanvas* canvas =
      recorder->beginRecording(layer_rect.width(), layer_rect.height());

  canvas->save();
  canvas->translate(-layer_rect.x(), -layer_rect.y());
  canvas->clipRect(SkRect::MakeXYWH(layer_rect.x(), layer_rect.y(),
                                    layer_rect.width(), layer_rect.height()));

  return canvas;
}

SkPicture* StopRecording(SkPictureRecorder* recorder, SkCanvas* canvas) {
  canvas->restore();
  return recorder->endRecordingAsPicture();
}

void VerifyScales(SkScalar x_scale,
                  SkScalar y_scale,
                  const SkMatrix& matrix,
                  int source_line) {
  SkSize scales;
  bool success = matrix.decomposeScale(&scales);
  EXPECT_TRUE(success) << "line: " << source_line;
  EXPECT_FLOAT_EQ(x_scale, scales.width()) << "line: " << source_line;
  EXPECT_FLOAT_EQ(y_scale, scales.height()) << "line: " << source_line;
}

}  // namespace

TEST(DiscardableImageUtilsTest, DrawImage) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  skia::RefPtr<SkImage> first = CreateDiscardableImage(gfx::Size(50, 50));
  skia::RefPtr<SkImage> second = CreateDiscardableImage(gfx::Size(50, 50));
  skia::RefPtr<SkImage> third = CreateDiscardableImage(gfx::Size(50, 50));
  skia::RefPtr<SkImage> fourth = CreateDiscardableImage(gfx::Size(50, 1));
  skia::RefPtr<SkImage> fifth = CreateDiscardableImage(gfx::Size(10, 10));
  skia::RefPtr<SkImage> sixth = CreateDiscardableImage(gfx::Size(10, 10));

  canvas->save();

  // At (0, 0).
  canvas->drawImage(first.get(), 0, 0);
  canvas->translate(25, 0);
  // At (25, 0).
  canvas->drawImage(second.get(), 0, 0);
  canvas->translate(0, 50);
  // At (50, 50).
  canvas->drawImage(third.get(), 25, 0);

  canvas->restore();
  canvas->save();

  canvas->translate(1, 0);
  canvas->rotate(90);
  // At (1, 0), rotated 90 degrees
  canvas->drawImage(fourth.get(), 0, 0);

  canvas->restore();
  canvas->save();

  canvas->scale(5.f, 6.f);
  // At (0, 0), scaled by 5 and 6
  canvas->drawImage(fifth.get(), 0, 0);

  canvas->restore();

  canvas->rotate(27);
  canvas->scale(3.3f, 0.4f);

  canvas->drawImage(sixth.get(), 0, 0);

  canvas->restore();

  skia::RefPtr<SkPicture> picture =
      skia::AdoptRef(StopRecording(&recorder, canvas));

  std::vector<skia::DiscardableImageUtils::PositionImage> images;
  skia::DiscardableImageUtils::GatherDiscardableImages(picture.get(), &images);

  EXPECT_EQ(6u, images.size());
  EXPECT_FLOAT_RECT_EQ(SkRect::MakeXYWH(0, 0, 50, 50), images[0].image_rect);
  VerifyScales(1.f, 1.f, images[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(25, 0, 50, 50), images[1].image_rect);
  VerifyScales(1.f, 1.f, images[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 50, 50, 50), images[2].image_rect);
  VerifyScales(1.f, 1.f, images[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[2].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 1, 50), images[3].image_rect);
  VerifyScales(1.f, 1.f, images[3].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[3].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 60), images[4].image_rect);
  VerifyScales(5.f, 6.f, images[4].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[4].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(-1.8159621f, 0, 31.219175f, 18.545712f),
                       images[5].image_rect);
  VerifyScales(3.3f, 0.4f, images[5].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[5].filter_quality);
}

TEST(DiscardableImageUtilsTest, DrawImageRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  SkPictureRecorder recorder;
  SkCanvas* canvas = StartRecording(&recorder, layer_rect);

  skia::RefPtr<SkImage> first = CreateDiscardableImage(gfx::Size(50, 50));
  skia::RefPtr<SkImage> second = CreateDiscardableImage(gfx::Size(50, 50));
  skia::RefPtr<SkImage> third = CreateDiscardableImage(gfx::Size(50, 50));

  canvas->save();

  SkPaint paint;

  // (0, 0, 100, 100).
  canvas->drawImageRect(first.get(), SkRect::MakeWH(100, 100), &paint);
  canvas->translate(25, 0);
  // (75, 50, 10, 10).
  canvas->drawImageRect(second.get(), SkRect::MakeXYWH(50, 50, 10, 10), &paint);
  canvas->translate(5, 50);
  // (0, 30, 100, 100).
  canvas->drawImageRect(third.get(), SkRect::MakeXYWH(-30, -20, 100, 100),
                        &paint);

  canvas->restore();

  skia::RefPtr<SkPicture> picture =
      skia::AdoptRef(StopRecording(&recorder, canvas));

  std::vector<skia::DiscardableImageUtils::PositionImage> images;
  skia::DiscardableImageUtils::GatherDiscardableImages(picture.get(), &images);

  EXPECT_EQ(3u, images.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100), images[0].image_rect);
  VerifyScales(2.f, 2.f, images[0].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[0].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(75, 50, 10, 10), images[1].image_rect);
  VerifyScales(0.2f, 0.2f, images[1].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[1].filter_quality);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 30, 100, 100), images[2].image_rect);
  VerifyScales(2.f, 2.f, images[2].matrix, __LINE__);
  EXPECT_EQ(kNone_SkFilterQuality, images[2].filter_quality);
}

}  // namespace skia
