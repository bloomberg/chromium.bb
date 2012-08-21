// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size.h"

namespace gfx {

namespace {

class FixedSource : public ImageSkiaSource {
 public:
  FixedSource(const ImageSkiaRep& image) : image_(image) {}

  virtual ~FixedSource() {
  }

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

  virtual ~DynamicSource() {
  }

  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    return gfx::ImageSkiaRep(size_, scale_factor);
  }

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(DynamicSource);
};

class NullSource: public ImageSkiaSource {
 public:
  NullSource() {
  }

  virtual ~NullSource() {
  }

  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    return gfx::ImageSkiaRep();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullSource);
};

}  // namespace

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
  EXPECT_EQ(1U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);
  image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_EQ(1U, image_skia.image_reps().size());
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

#if defined(OS_MACOSX)

// Tests that GetRepresentations returns all of the representations in the
// image when there are multiple representations for a scale factor.
// This currently is the case with ImageLoadingTracker::LoadImages, to
// load the application shortcut icon on Mac in particular.
TEST(ImageSkiaTest, GetRepresentationsManyRepsPerScaleFactor) {
  const int kSmallIcon1x = 16;
  const int kSmallIcon2x = 32;
  const int kLargeIcon1x = 32;

  ImageSkia image(new NullSource(), gfx::Size(kSmallIcon1x, kSmallIcon1x));
  // Simulate a source which loads images on a delay. Upon
  // GetImageForScaleFactor, it immediately returns null and starts loading
  // image reps slowly.
  image.GetRepresentation(ui::SCALE_FACTOR_100P);
  image.GetRepresentation(ui::SCALE_FACTOR_200P);

  // After a lengthy amount of simulated time, finally loaded image reps.
  image.AddRepresentation(ImageSkiaRep(
      gfx::Size(kSmallIcon1x, kSmallIcon1x), ui::SCALE_FACTOR_100P));
  image.AddRepresentation(ImageSkiaRep(
      gfx::Size(kSmallIcon2x, kSmallIcon2x), ui::SCALE_FACTOR_200P));
  image.AddRepresentation(ImageSkiaRep(
      gfx::Size(kLargeIcon1x, kLargeIcon1x), ui::SCALE_FACTOR_100P));

  std::vector<ImageSkiaRep> image_reps = image.GetRepresentations();
  EXPECT_EQ(3u, image_reps.size());

  int num_1x = 0;
  int num_2x = 0;
  for (size_t i = 0; i < image_reps.size(); ++i) {
    if (image_reps[i].scale_factor() == ui::SCALE_FACTOR_100P)
      num_1x++;
    else if (image_reps[i].scale_factor() == ui::SCALE_FACTOR_200P)
      num_2x++;
  }
  EXPECT_EQ(2, num_1x);
  EXPECT_EQ(1, num_2x);
}

#endif  // OS_MACOSX

TEST(ImageSkiaTest, GetBitmap) {
  ImageSkia image_skia(new DynamicSource(Size(100, 200)), Size(100, 200));
  const SkBitmap* bitmap = image_skia.bitmap();
  EXPECT_NE(static_cast<SkBitmap*>(NULL), bitmap);
  EXPECT_FALSE(bitmap->isNull());
}

TEST(ImageSkiaTest, GetBitmapFromEmpty) {
  // Create an image with 1 representation and remove it so the ImageSkiaStorage
  // is left with no representations.
  ImageSkia empty_image(ImageSkiaRep(Size(100, 200), ui::SCALE_FACTOR_100P));
  ImageSkia empty_image_copy(empty_image);
  empty_image.RemoveRepresentation(ui::SCALE_FACTOR_100P);

  // Check that ImageSkia::bitmap() still returns a valid SkBitmap pointer for
  // the image and all its copies.
  const SkBitmap* bitmap = empty_image_copy.bitmap();
  ASSERT_NE(static_cast<SkBitmap*>(NULL), bitmap);
  EXPECT_TRUE(bitmap->isNull());
  EXPECT_TRUE(bitmap->empty());
}

#if defined(OS_MACOSX) || defined(OS_WIN)
TEST(ImageSkiaTest, OperatorBitmapFromSource) {
  ImageSkia image_skia(new DynamicSource(Size(100, 200)), Size(100, 200));
  // ImageSkia should use the source to create the bitmap.
  const SkBitmap& bitmap = image_skia;
  ASSERT_NE(static_cast<SkBitmap*>(NULL), &bitmap);
  EXPECT_FALSE(bitmap.isNull());
}
#endif

TEST(ImageSkiaTest, BackedBySameObjectAs) {
  // Null images should all be backed by the same object (NULL).
  ImageSkia image;
  ImageSkia unrelated;
  EXPECT_TRUE(image.BackedBySameObjectAs(unrelated));

  image.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10),
                                            ui::SCALE_FACTOR_100P));
  ImageSkia copy = image;
  copy.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10),
                                           ui::SCALE_FACTOR_200P));
  unrelated.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10),
                                                ui::SCALE_FACTOR_100P));
  EXPECT_TRUE(image.BackedBySameObjectAs(copy));
  EXPECT_FALSE(image.BackedBySameObjectAs(unrelated));
  EXPECT_FALSE(copy.BackedBySameObjectAs(unrelated));
}

}  // namespace gfx
