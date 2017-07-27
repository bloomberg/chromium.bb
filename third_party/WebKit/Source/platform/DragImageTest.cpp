/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/DragImage.h"

#include <memory>
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontTraits.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class TestImage : public Image {
 public:
  static PassRefPtr<TestImage> Create(sk_sp<SkImage> image) {
    return AdoptRef(new TestImage(image));
  }

  static PassRefPtr<TestImage> Create(const IntSize& size) {
    return AdoptRef(new TestImage(size));
  }

  IntSize Size() const override {
    DCHECK(image_);

    return IntSize(image_->width(), image_->height());
  }

  sk_sp<SkImage> ImageForCurrentFrame() override { return image_; }

  bool CurrentFrameKnownToBeOpaque(
      MetadataMode = kUseCurrentMetadata) override {
    return false;
  }

  void DestroyDecodedData() override {
    // Image pure virtual stub.
  }

  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            RespectImageOrientationEnum,
            ImageClampingMode) override {
    // Image pure virtual stub.
  }

 private:
  explicit TestImage(sk_sp<SkImage> image) : image_(image) {}

  explicit TestImage(IntSize size) : image_(nullptr) {
    sk_sp<SkSurface> surface = CreateSkSurface(size);
    if (!surface)
      return;

    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    image_ = surface->makeImageSnapshot();
  }

  static sk_sp<SkSurface> CreateSkSurface(IntSize size) {
    return SkSurface::MakeRaster(
        SkImageInfo::MakeN32(size.Width(), size.Height(), kPremul_SkAlphaType));
  }

  sk_sp<SkImage> image_;
};

TEST(DragImageTest, NullHandling) {
  EXPECT_FALSE(DragImage::Create(0));

  RefPtr<TestImage> null_test_image(TestImage::Create(IntSize()));
  EXPECT_FALSE(DragImage::Create(null_test_image.Get()));
}

TEST(DragImageTest, NonNullHandling) {
  RefPtr<TestImage> test_image(TestImage::Create(IntSize(2, 2)));
  std::unique_ptr<DragImage> drag_image = DragImage::Create(test_image.Get());
  ASSERT_TRUE(drag_image);

  drag_image->Scale(0.5, 0.5);
  IntSize size = drag_image->Size();
  EXPECT_EQ(1, size.Width());
  EXPECT_EQ(1, size.Height());
}

TEST(DragImageTest, CreateDragImage) {
  // Tests that the DrageImage implementation doesn't choke on null values
  // of imageForCurrentFrame().
  // FIXME: how is this test any different from test NullHandling?
  RefPtr<TestImage> test_image(TestImage::Create(IntSize()));
  EXPECT_FALSE(DragImage::Create(test_image.Get()));
}

TEST(DragImageTest, TrimWhitespace) {
  KURL url(kParsedURLString, "http://www.example.com/");
  String test_label = "          Example Example Example      \n    ";
  String expected_label = "Example Example Example";
  float device_scale_factor = 1.0f;

  FontDescription font_description;
  font_description.FirstFamily().SetFamily("Arial");
  font_description.SetSpecifiedSize(16);
  font_description.SetIsAbsoluteSize(true);
  font_description.SetGenericFamily(FontDescription::kNoFamily);
  font_description.SetWeight(kFontWeightNormal);
  font_description.SetStyle(kFontStyleNormal);

  std::unique_ptr<DragImage> test_image =
      DragImage::Create(url, test_label, font_description, device_scale_factor);
  std::unique_ptr<DragImage> expected_image = DragImage::Create(
      url, expected_label, font_description, device_scale_factor);

  EXPECT_EQ(test_image->Size().Width(), expected_image->Size().Width());
}

TEST(DragImageTest, InterpolationNone) {
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(4, 4);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 2, 2), 0xFFFFFFFF);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(0, 2, 2, 2), 0xFF000000);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(2, 0, 2, 2), 0xFF000000);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(2, 2, 2, 2), 0xFFFFFFFF);

  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(2, 2);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 1, 1), 0xFFFFFFFF);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(0, 1, 1, 1), 0xFF000000);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(1, 0, 1, 1), 0xFF000000);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(1, 1, 1, 1), 0xFFFFFFFF);

  RefPtr<TestImage> test_image =
      TestImage::Create(SkImage::MakeFromBitmap(test_bitmap));
  std::unique_ptr<DragImage> drag_image = DragImage::Create(
      test_image.Get(), kDoNotRespectImageOrientation, 1, kInterpolationNone);
  ASSERT_TRUE(drag_image);
  drag_image->Scale(2, 2);
  const SkBitmap& drag_bitmap = drag_image->Bitmap();
  for (int x = 0; x < drag_bitmap.width(); ++x)
    for (int y = 0; y < drag_bitmap.height(); ++y)
      EXPECT_EQ(expected_bitmap.getColor(x, y), drag_bitmap.getColor(x, y));
}

}  // namespace blink
