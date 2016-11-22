// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/property_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace mojo {
namespace {

// Tests round-trip serializing and deserializing an SkBitmap.
TEST(PropertyTypeConvertersTest, SkBitmapRoundTrip) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 32);
  bitmap.eraseARGB(255, 11, 22, 33);
  EXPECT_FALSE(bitmap.isNull());
  auto bytes = TypeConverter<std::vector<uint8_t>, SkBitmap>::Convert(bitmap);
  SkBitmap out = TypeConverter<SkBitmap, std::vector<uint8_t>>::Convert(bytes);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, out));
}

}  // namespace
}  // namespace mojo
