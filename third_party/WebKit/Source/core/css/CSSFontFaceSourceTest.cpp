// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontFaceSource.h"

#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DummyFontFaceSource : public CSSFontFaceSource {
 public:
  PassRefPtr<SimpleFontData> CreateFontData(const FontDescription&) override {
    return SimpleFontData::Create(
        FontPlatformData(SkTypeface::MakeDefault(), "", 0, false, false));
  }

  DummyFontFaceSource() {}

  PassRefPtr<SimpleFontData> GetFontDataForSize(float size) {
    FontDescription font_description;
    font_description.SetSizeAdjust(size);
    font_description.SetAdjustedSize(size);
    return GetFontData(font_description);
  }
};

namespace {

unsigned SimulateHashCalculation(float size) {
  FontDescription font_description;
  font_description.SetSizeAdjust(size);
  font_description.SetAdjustedSize(size);
  return font_description.CacheKey(FontFaceCreationParams()).GetHash();
}
}

TEST(CSSFontFaceSourceTest, HashCollision) {
  DummyFontFaceSource font_face_source;
  // Even if the hash value collide, fontface cache should return different
  // value for different fonts.
  EXPECT_EQ(SimulateHashCalculation(8775), SimulateHashCalculation(418));
  EXPECT_NE(font_face_source.GetFontDataForSize(8775),
            font_face_source.GetFontDataForSize(418));
}

}  // namespace blink
