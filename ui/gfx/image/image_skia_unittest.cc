// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size.h"
#include "ui/base/layout.h"

namespace gfx {

namespace {

class FixedSource : public ImageSkiaSource {
 public:
  FixedSource(const ImageSkiaRep& image) : image_(image) {}

  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    return image_;
  }

 private:
  ImageSkiaRep image_;

  DISALLOW_COPY_AND_ASSIGN(FixedSource);
};

class DynamicSource : public ImageSkiaSource {
 public:
  DynamicSource(const gfx::Size& size) : size_(size) {}

  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    return gfx::ImageSkiaRep(size_, scale_factor);
  }

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(DynamicSource);
};

}  // namespace;

typedef testing::Test ImageSkiaTest;

TEST(ImageSkiaTest, FixedSource) {
  ImageSkiaRep image(Size(100, 200), ui::SCALE_FACTOR_100P);
  ImageSkia image_skia(new FixedSource(image), Size(100, 200));
  EXPECT_EQ(0U, image_skia.image_reps().size());

  const ImageSkiaRep& result_100p =
      image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_EQ(100, result_100p.GetWidth());
  EXPECT_EQ(200, result_100p.GetHeight());
  EXPECT_EQ(ui::SCALE_FACTOR_100P, result_100p.scale_factor());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  const ImageSkiaRep& result_200p =
      image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);

  EXPECT_EQ(100, result_200p.GetWidth());
  EXPECT_EQ(200, result_200p.GetHeight());
  EXPECT_EQ(100, result_200p.pixel_width());
  EXPECT_EQ(200, result_200p.pixel_height());
  EXPECT_EQ(ui::SCALE_FACTOR_100P, result_200p.scale_factor());
  EXPECT_EQ(2U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_EQ(2U, image_skia.image_reps().size());
  image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_EQ(2U, image_skia.image_reps().size());
}

TEST(ImageSkiaTest, DynamicSource) {
  ImageSkia image_skia(new DynamicSource(Size(100, 200)), Size(100, 200));
  EXPECT_EQ(0U, image_skia.image_reps().size());
  const ImageSkiaRep& result_100p =
      image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_EQ(100, result_100p.GetWidth());
  EXPECT_EQ(200, result_100p.GetHeight());
  EXPECT_EQ(ui::SCALE_FACTOR_100P, result_100p.scale_factor());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  const ImageSkiaRep& result_200p =
      image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_EQ(100, result_200p.GetWidth());
  EXPECT_EQ(200, result_200p.GetHeight());
  EXPECT_EQ(200, result_200p.pixel_width());
  EXPECT_EQ(400, result_200p.pixel_height());
  EXPECT_EQ(ui::SCALE_FACTOR_200P, result_200p.scale_factor());
  EXPECT_EQ(2U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_EQ(2U, image_skia.image_reps().size());
  image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_EQ(2U, image_skia.image_reps().size());
}

TEST(ImageSkiaTest, GetBitmap) {
  ImageSkia image_skia(new DynamicSource(Size(100, 200)), Size(100, 200));
  const SkBitmap* bitmap = image_skia.bitmap();
  EXPECT_NE(static_cast<SkBitmap*>(NULL), bitmap);
  EXPECT_FALSE(bitmap->isNull());
}

}  // namespace gfx
