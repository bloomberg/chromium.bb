// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojo/cursor_struct_traits.h"

#include "mojo/public/cpp/bindings/binding_set.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/mojo/cursor.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace ui {

namespace {

bool EchoCursor(const ui::Cursor& in, ui::Cursor* out) {
  return mojom::Cursor::Deserialize(mojom::Cursor::Serialize(&in), out);
}

using CursorStructTraitsTest = testing::Test;

}  // namespace

// Tests numeric cursor ids.
TEST_F(CursorStructTraitsTest, TestBuiltIn) {
  for (int i = 0; i < 43; ++i) {
    ui::CursorType type = static_cast<ui::CursorType>(i);
    ui::Cursor input(type);

    ui::Cursor output;
    ASSERT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(type, output.native_type());
  }
}

// Test that we copy cursor bitmaps and metadata across the wire.
TEST_F(CursorStructTraitsTest, TestBitmapCursor) {
  ui::Cursor input(ui::CursorType::kCustom);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);

  const gfx::Point kHotspot = gfx::Point(5, 2);
  input.set_custom_hotspot(kHotspot);
  input.set_custom_bitmap(bitmap);

  const float kScale = 2.0f;
  input.set_device_scale_factor(kScale);

  ui::Cursor output;
  ASSERT_TRUE(EchoCursor(input, &output));

  EXPECT_EQ(ui::CursorType::kCustom, output.native_type());
  EXPECT_EQ(kScale, output.device_scale_factor());
  EXPECT_EQ(kHotspot, output.GetHotspot());

  // Even if the pixel data is logically the same, expect that it has different
  // generation ids.
  EXPECT_FALSE(output.IsSameAs(input));

  // Make a copy of output. It should be the same as output.
  ui::Cursor copy = output;
  EXPECT_TRUE(copy.IsSameAs(output));

  // But make sure that the pixel data actually is equivalent.
  ASSERT_EQ(input.GetBitmap().width(), output.GetBitmap().width());
  ASSERT_EQ(input.GetBitmap().height(), output.GetBitmap().height());

  for (int x = 0; x < input.GetBitmap().width(); ++x) {
    for (int y = 0; y < input.GetBitmap().height(); ++y) {
      EXPECT_EQ(input.GetBitmap().getColor(x, y),
                output.GetBitmap().getColor(x, y));
    }
  }
}

// Test that we deal with empty bitmaps. (When a cursor resource isn't loaded
// in the renderer, the renderer will send a custom cursor with an empty
// bitmap.)
TEST_F(CursorStructTraitsTest, TestEmptyCursor) {
  const gfx::Point kHotspot = gfx::Point(5, 2);
  const float kScale = 2.0f;

  ui::Cursor input(ui::CursorType::kCustom);
  input.set_custom_hotspot(kHotspot);
  input.set_custom_bitmap(SkBitmap());
  input.set_device_scale_factor(kScale);

  ui::Cursor output;
  ASSERT_TRUE(EchoCursor(input, &output));

  EXPECT_TRUE(output.GetBitmap().empty());
}

}  // namespace ui
