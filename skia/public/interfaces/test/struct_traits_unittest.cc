// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "skia/public/interfaces/test/traits_test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "ui/gfx/skia_util.h"

namespace skia {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    mojom::TraitsTestServicePtr proxy;
    traits_test_bindings_.AddBinding(this, mojo::MakeRequest(&proxy));
    return proxy;
  }

 private:
  // TraitsTestService:
  void EchoBitmap(const SkBitmap& b, EchoBitmapCallback callback) override {
    std::move(callback).Run(b);
  }

  void EchoBlurImageFilterTileMode(
      SkBlurImageFilter::TileMode t,
      EchoBlurImageFilterTileModeCallback callback) override {
    std::move(callback).Run(t);
  }

  void EchoImageFilter(const sk_sp<SkImageFilter>& i,
                       EchoImageFilterCallback callback) override {
    std::move(callback).Run(i);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

static sk_sp<SkImageFilter> make_scale(float amount,
                                       sk_sp<SkImageFilter> input) {
  SkScalar s = amount;
  SkScalar matrix[20] = {s, 0, 0, 0, 0, 0, s, 0, 0, 0,
                         0, 0, s, 0, 0, 0, 0, 0, s, 0};
  sk_sp<SkColorFilter> filter(
      SkColorFilter::MakeMatrixFilterRowMajor255(matrix));
  return SkColorFilterImageFilter::Make(std::move(filter), std::move(input));
}

static bool colorspace_srgb_gamma(SkColorSpace* cs) {
  return cs && cs->gammaCloseToSRGB();
}

}  // namespace

TEST_F(StructTraitsTest, Bitmap) {
  SkBitmap input;
  input.allocN32Pixels(10, 5);
  input.eraseColor(SK_ColorYELLOW);
  input.erase(SK_ColorTRANSPARENT, SkIRect::MakeXYWH(0, 1, 2, 3));
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SkBitmap output;
  proxy->EchoBitmap(input, &output);
  EXPECT_EQ(input.colorType(), output.colorType());
  EXPECT_EQ(input.alphaType(), output.alphaType());
  EXPECT_EQ(colorspace_srgb_gamma(input.colorSpace()),
            colorspace_srgb_gamma(output.colorSpace()));
  EXPECT_EQ(input.width(), output.width());
  EXPECT_EQ(input.height(), output.height());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST_F(StructTraitsTest, BitmapWithExtraRowBytes) {
  SkBitmap input;
  // Ensure traits work with bitmaps containing additional bytes between rows.
  SkImageInfo info = SkImageInfo::MakeN32(8, 5, kPremul_SkAlphaType);
  input.allocPixels(info, info.minRowBytes() + 2);
  input.eraseColor(SK_ColorRED);
  input.erase(SK_ColorTRANSPARENT, SkIRect::MakeXYWH(0, 1, 2, 3));
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SkBitmap output;
  proxy->EchoBitmap(input, &output);
  EXPECT_EQ(input.colorType(), output.colorType());
  EXPECT_EQ(input.alphaType(), output.alphaType());
  EXPECT_EQ(colorspace_srgb_gamma(input.colorSpace()),
            colorspace_srgb_gamma(output.colorSpace()));
  EXPECT_EQ(input.width(), output.width());
  EXPECT_EQ(input.height(), output.height());
  EXPECT_EQ(input.rowBytes(), output.rowBytes());
  EXPECT_TRUE(gfx::BitmapsAreEqual(input, output));
}

TEST_F(StructTraitsTest, ImageFilter) {
  sk_sp<SkImageFilter> input(make_scale(0.5f, nullptr));
  SkString input_str;
  input->toString(&input_str);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  sk_sp<SkImageFilter> output;
  proxy->EchoImageFilter(input, &output);
  SkString output_str;
  output->toString(&output_str);
  EXPECT_EQ(input_str, output_str);
}

TEST_F(StructTraitsTest, DropShadowImageFilter) {
  sk_sp<SkImageFilter> input(SkDropShadowImageFilter::Make(
      SkIntToScalar(3), SkIntToScalar(8), SkIntToScalar(4), SkIntToScalar(9),
      SK_ColorBLACK,
      SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode, nullptr));
  SkString input_str;
  input->toString(&input_str);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  sk_sp<SkImageFilter> output;
  proxy->EchoImageFilter(input, &output);
  SkString output_str;
  output->toString(&output_str);
  EXPECT_EQ(input_str, output_str);
}

TEST_F(StructTraitsTest, BlurImageFilterTileMode) {
  SkBlurImageFilter::TileMode input(SkBlurImageFilter::kClamp_TileMode);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SkBlurImageFilter::TileMode output;
  proxy->EchoBlurImageFilterTileMode(input, &output);
  EXPECT_EQ(input, output);
}

}  // namespace skia
