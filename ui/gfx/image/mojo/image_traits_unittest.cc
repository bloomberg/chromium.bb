// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/mojo/image_traits_test_service.mojom.h"

namespace gfx {

namespace {

// A test ImageSkiaSource that creates an ImageSkiaRep for any scale.
class TestImageSkiaSource : public ImageSkiaSource {
 public:
  explicit TestImageSkiaSource(const gfx::Size& dip_size)
      : dip_size_(dip_size) {}
  ~TestImageSkiaSource() override = default;

  // ImageSkiaSource:
  ImageSkiaRep GetImageForScale(float scale) override {
    return ImageSkiaRep(ScaleToCeiledSize(dip_size_, scale), scale);
  }

 private:
  const gfx::Size dip_size_;

  DISALLOW_COPY_AND_ASSIGN(TestImageSkiaSource);
};

class ImageTraitsTest : public testing::Test,
                        public mojom::ImageTraitsTestService {
 public:
  ImageTraitsTest() = default;

  // testing::Test:
  void SetUp() override {
    bindings_.AddBinding(this, mojo::MakeRequest(&service_));
  }

  mojom::ImageTraitsTestServicePtr& service() { return service_; }

 private:
  // mojom::ImageTraitsTestService:
  void EchoImageSkiaRep(const ImageSkiaRep& in,
                        EchoImageSkiaRepCallback callback) override {
    std::move(callback).Run(in);
  }
  void EchoImageSkia(const ImageSkia& in,
                     EchoImageSkiaCallback callback) override {
    std::move(callback).Run(in);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<ImageTraitsTestService> bindings_;
  mojom::ImageTraitsTestServicePtr service_;

  DISALLOW_COPY_AND_ASSIGN(ImageTraitsTest);
};

}  // namespace

TEST_F(ImageTraitsTest, NullImageSkiaRep) {
  ImageSkiaRep null_rep;
  ASSERT_TRUE(null_rep.is_null());

  ImageSkiaRep output(gfx::Size(1, 1), 1.0f);
  ASSERT_FALSE(output.is_null());
  service()->EchoImageSkiaRep(null_rep, &output);
  EXPECT_TRUE(output.is_null());
}

TEST_F(ImageTraitsTest, EmptyImageSkiaRep) {
  SkBitmap empty_bitmap;
  empty_bitmap.allocN32Pixels(0, 0);
  // Empty SkBitmap is not null.
  EXPECT_FALSE(empty_bitmap.isNull());
  EXPECT_TRUE(empty_bitmap.drawsNothing());

  ImageSkiaRep empty_rep(empty_bitmap, 1.0f);
  // ImageSkiaRep with empty bitmap is not null.
  ASSERT_FALSE(empty_rep.is_null());

  ImageSkiaRep output(gfx::Size(1, 1), 1.0f);
  ASSERT_FALSE(output.is_null());
  service()->EchoImageSkiaRep(empty_rep, &output);
  EXPECT_TRUE(empty_rep.sk_bitmap().drawsNothing());
  EXPECT_TRUE(test::AreBitmapsEqual(empty_rep.sk_bitmap(), output.sk_bitmap()));
}

TEST_F(ImageTraitsTest, ImageSkiaRep) {
  ImageSkiaRep image_rep(gfx::Size(2, 4), 2.0f);

  ImageSkiaRep output;
  service()->EchoImageSkiaRep(image_rep, &output);

  EXPECT_FALSE(output.is_null());
  EXPECT_EQ(image_rep.scale(), output.scale());
  EXPECT_TRUE(test::AreBitmapsEqual(image_rep.sk_bitmap(), output.sk_bitmap()));
}

TEST_F(ImageTraitsTest, UnscaledImageSkiaRep) {
  ImageSkiaRep image_rep(gfx::Size(2, 4), 0.0f);
  ASSERT_TRUE(image_rep.unscaled());

  ImageSkiaRep output(gfx::Size(1, 1), 1.0f);
  EXPECT_FALSE(output.unscaled());
  service()->EchoImageSkiaRep(image_rep, &output);
  EXPECT_TRUE(output.unscaled());
  EXPECT_TRUE(test::AreBitmapsEqual(image_rep.sk_bitmap(), output.sk_bitmap()));
}

TEST_F(ImageTraitsTest, NullImageSkia) {
  ImageSkia null_image;
  ASSERT_TRUE(null_image.isNull());

  ImageSkia output(ImageSkiaRep(gfx::Size(1, 1), 1.0f));
  ASSERT_FALSE(output.isNull());
  service()->EchoImageSkia(null_image, &output);
  EXPECT_TRUE(output.isNull());
}

TEST_F(ImageTraitsTest, ImageSkiaWithNoRepsTreatedAsNull) {
  const gfx::Size kSize(1, 2);
  ImageSkia image(new TestImageSkiaSource(kSize), kSize);
  ASSERT_FALSE(image.isNull());

  ImageSkia output(ImageSkiaRep(gfx::Size(1, 1), 1.0f));
  ASSERT_FALSE(output.isNull());
  service()->EchoImageSkia(image, &output);
  EXPECT_TRUE(output.isNull());
}

TEST_F(ImageTraitsTest, ImageSkia) {
  const gfx::Size kSize(1, 2);
  ImageSkia image(new TestImageSkiaSource(kSize), kSize);
  image.GetRepresentation(1.0f);
  image.GetRepresentation(2.0f);

  ImageSkia output;
  service()->EchoImageSkia(image, &output);

  EXPECT_TRUE(test::AreImagesEqual(Image(output), Image(image)));
}

TEST_F(ImageTraitsTest, EmptyRepPreserved) {
  const gfx::Size kSize(1, 2);
  ImageSkia image(new TestImageSkiaSource(kSize), kSize);
  image.GetRepresentation(1.0f);

  SkBitmap empty_bitmap;
  empty_bitmap.allocN32Pixels(0, 0);
  image.AddRepresentation(ImageSkiaRep(empty_bitmap, 2.0f));

  ImageSkia output;
  service()->EchoImageSkia(image, &output);

  EXPECT_TRUE(test::AreImagesEqual(Image(output), Image(image)));
}

TEST_F(ImageTraitsTest, ImageSkiaWithOperations) {
  const gfx::Size kSize(32, 32);
  ImageSkia image(new TestImageSkiaSource(kSize), kSize);

  const gfx::Size kNewSize(16, 16);
  ImageSkia resized = ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, kNewSize);
  resized.GetRepresentation(1.0f);
  resized.GetRepresentation(2.0f);

  ImageSkia output;
  service()->EchoImageSkia(resized, &output);

  EXPECT_TRUE(test::AreImagesEqual(Image(output), Image(resized)));
}

}  // namespace gfx
