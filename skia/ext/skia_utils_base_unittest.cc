// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "third_party/skia/include/effects/SkPaintImageFilter.h"

namespace skia {
namespace {

class FakeImageGenerator : public SkImageGenerator {
 public:
  FakeImageGenerator() : SkImageGenerator(SkImageInfo::MakeN32Premul(10, 10)) {}
  ~FakeImageGenerator() override = default;

  SkData* onRefEncodedData() override {
    // Use a generator that lets the caller encode it.
    return SkData::MakeWithCString("evilimage").release();
  }

  bool onGetPixels(const SkImageInfo&, void*, size_t, const Options&) override {
    NOTREACHED();
    return false;
  }
};

#if GTEST_HAS_DEATH_TEST
TEST(SkiaUtilsBaseTest, ImageSerializationFails) {
  auto image =
      SkImage::MakeFromGenerator(base::MakeUnique<FakeImageGenerator>());
  auto filter = SkImageSource::Make(image);
  EXPECT_DEATH(ValidatingSerializeFlattenable(filter.get()), "");

  SkPaint paint;
  paint.setShader(
      image->makeShader(SkShader::kClamp_TileMode, SkShader::kClamp_TileMode));
  filter = SkPaintImageFilter::Make(paint);
  EXPECT_DEATH(ValidatingSerializeFlattenable(filter.get()), "");
}
#endif

}  // namespace
}  // namespace skia
