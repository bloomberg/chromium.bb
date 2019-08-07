// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/pickle.h"
#include "build/build_config.h"
#include "content/common/cursors/webcursor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace content {
namespace {

// Creates a basic bitmap for testing with the given width and height.
SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

TEST(WebCursorTest, DefaultConstructor) {
  WebCursor cursor;
  EXPECT_EQ(blink::WebCursorInfo::kTypePointer, cursor.info().type);
  EXPECT_TRUE(cursor.info().custom_image.isNull());
  EXPECT_TRUE(cursor.info().hotspot.IsOrigin());
  EXPECT_EQ(1.f, cursor.info().image_scale_factor);
}

TEST(WebCursorTest, CursorInfoConstructor) {
  CursorInfo info(blink::WebCursorInfo::kTypeHand);
  WebCursor cursor(info);
  EXPECT_EQ(info, cursor.info());
}

TEST(WebCursorTest, CursorInfoConstructorCustom) {
  CursorInfo info(blink::WebCursorInfo::kTypeCustom);
  info.custom_image = CreateTestBitmap(32, 32);
  info.hotspot = gfx::Point(10, 20);
  info.image_scale_factor = 1.5f;
  WebCursor cursor(info);
  EXPECT_EQ(info, cursor.info());
}

TEST(WebCursorTest, CopyConstructorType) {
  CursorInfo info(blink::WebCursorInfo::kTypeHand);
  WebCursor cursor(info);
  WebCursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(WebCursorTest, CopyConstructorCustom) {
  CursorInfo info(blink::WebCursorInfo::kTypeCustom);
  info.custom_image = CreateTestBitmap(32, 32);
  info.hotspot = gfx::Point(10, 20);
  info.image_scale_factor = 1.5f;
  WebCursor cursor(info);
  WebCursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(WebCursorTest, SerializationDefault) {
  WebCursor cursor;
  base::Pickle pickle;
  cursor.Serialize(&pickle);

  WebCursor copy;
  base::PickleIterator iter(pickle);
  ASSERT_TRUE(copy.Deserialize(&pickle, &iter));
  EXPECT_EQ(cursor, copy);
}

TEST(WebCursorTest, SerializationCustom) {
  CursorInfo info(blink::WebCursorInfo::kTypeCustom);
  info.custom_image = CreateTestBitmap(32, 32);
  info.hotspot = gfx::Point(10, 20);
  info.image_scale_factor = 1.5f;
  WebCursor cursor(info);
  base::Pickle pickle;
  cursor.Serialize(&pickle);

  WebCursor copy;
  base::PickleIterator iter(pickle);
  ASSERT_TRUE(copy.Deserialize(&pickle, &iter));
  EXPECT_EQ(cursor, copy);
}

TEST(WebCursorTest, DeserializeBadMessage) {
  WebCursor cursor((CursorInfo(blink::WebCursorInfo::kTypeHand)));
  WebCursor unmodified_cursor(cursor);
  WebCursor invalid_cursor;

  // Serialize a cursor with a small scale; deserialization should fail.
  CursorInfo small_scale_info(blink::WebCursorInfo::kTypeCustom);
  small_scale_info.image_scale_factor = 0.001f;
  invalid_cursor.set_info_for_testing(small_scale_info);
  base::Pickle small_scale_pickle;
  invalid_cursor.Serialize(&small_scale_pickle);
  base::PickleIterator iter = base::PickleIterator(small_scale_pickle);
  EXPECT_FALSE(cursor.Deserialize(&small_scale_pickle, &iter));
  EXPECT_EQ(unmodified_cursor, cursor);

  // Serialize a cursor with a big scale; deserialization should fail.
  CursorInfo big_scale_info(blink::WebCursorInfo::kTypeCustom);
  big_scale_info.image_scale_factor = 1000.f;
  invalid_cursor.set_info_for_testing(big_scale_info);
  base::Pickle big_scale_pickle;
  invalid_cursor.Serialize(&big_scale_pickle);
  iter = base::PickleIterator(big_scale_pickle);
  EXPECT_FALSE(cursor.Deserialize(&big_scale_pickle, &iter));
  EXPECT_EQ(unmodified_cursor, cursor);

  // Serialize a cursor with a big image; deserialization should fail.
  CursorInfo big_image_info(blink::WebCursorInfo::kTypeCustom);
  big_image_info.custom_image = CreateTestBitmap(1025, 3);
  invalid_cursor.set_info_for_testing(big_image_info);
  base::Pickle big_image_pickle;
  invalid_cursor.Serialize(&big_image_pickle);
  iter = base::PickleIterator(big_image_pickle);
  EXPECT_FALSE(cursor.Deserialize(&big_image_pickle, &iter));
  EXPECT_EQ(unmodified_cursor, cursor);
}

TEST(WebCursorTest, ClampHotspot) {
  // Initialize a cursor with an invalid hotspot; it should be clamped.
  CursorInfo info(blink::WebCursorInfo::kTypeCustom);
  info.hotspot = gfx::Point(100, 100);
  info.custom_image = CreateTestBitmap(5, 7);
  WebCursor cursor(info);
  EXPECT_EQ(gfx::Point(4, 6), cursor.info().hotspot);

  // Serialize a cursor with an invalid hotspot; it should be clamped.
  cursor.set_info_for_testing(info);
  base::Pickle pickle;
  cursor.Serialize(&pickle);
  base::PickleIterator iter = base::PickleIterator(pickle);
  EXPECT_TRUE(cursor.Deserialize(&pickle, &iter));
  EXPECT_EQ(gfx::Point(4, 6), cursor.info().hotspot);
}

#if defined(USE_AURA)
TEST(WebCursorTest, CursorScaleFactor) {
  CursorInfo info;
  info.type = blink::WebCursorInfo::kTypeCustom;
  info.hotspot = gfx::Point(0, 1);
  info.image_scale_factor = 2.0f;
  info.custom_image = CreateTestBitmap(128, 128);
  WebCursor cursor(info);

  display::Display display;
  display.set_device_scale_factor(4.2f);
  cursor.SetDisplayInfo(display);

#if defined(USE_OZONE)
  // For Ozone cursors, the size of the cursor is capped at 64px, and this is
  // enforce through the calculated scale factor.
  EXPECT_EQ(0.5f, cursor.GetNativeCursor().device_scale_factor());
#else
  EXPECT_EQ(2.1f, cursor.GetNativeCursor().device_scale_factor());
#endif

  // Test that the Display dsf is copied.
  WebCursor copy(cursor);
  EXPECT_EQ(cursor.GetNativeCursor().device_scale_factor(),
            copy.GetNativeCursor().device_scale_factor());
}

TEST(WebCursorTest, UnscaledImageCopy) {
  CursorInfo info;
  info.type = blink::WebCursorInfo::kTypeCustom;
  info.hotspot = gfx::Point(0, 1);
  info.custom_image = CreateTestBitmap(2, 2);
  WebCursor cursor(info);

  SkBitmap copy;
  gfx::Point hotspot;
  float dsf = 0.f;
  cursor.CreateScaledBitmapAndHotspotFromCustomData(&copy, &hotspot, &dsf);
  EXPECT_EQ(1.f, dsf);
  EXPECT_EQ(2, copy.width());
  EXPECT_EQ(2, copy.height());
  EXPECT_EQ(0, hotspot.x());
  EXPECT_EQ(1, hotspot.y());
}
#endif

#if defined(OS_WIN)
void ScaleCursor(float scale, int hotspot_x, int hotspot_y) {
  CursorInfo info;
  info.type = blink::WebCursorInfo::kTypeCustom;
  info.hotspot = gfx::Point(hotspot_x, hotspot_y);
  info.custom_image = CreateTestBitmap(10, 10);
  WebCursor cursor(info);

  display::Display display;
  display.set_device_scale_factor(scale);
  cursor.SetDisplayInfo(display);

  HCURSOR windows_cursor_handle = cursor.GetNativeCursor().platform();
  EXPECT_NE(nullptr, windows_cursor_handle);
  ICONINFO windows_icon_info;
  EXPECT_TRUE(GetIconInfo(windows_cursor_handle, &windows_icon_info));
  EXPECT_FALSE(windows_icon_info.fIcon);
  EXPECT_EQ(static_cast<DWORD>(scale * hotspot_x), windows_icon_info.xHotspot);
  EXPECT_EQ(static_cast<DWORD>(scale * hotspot_y), windows_icon_info.yHotspot);
}

TEST(WebCursorTest, WindowsCursorScaledAtHiDpi) {
  ScaleCursor(2.0f, 4, 6);
  ScaleCursor(1.5f, 2, 8);
  ScaleCursor(1.25f, 3, 7);
}
#endif

}  // namespace
}  // namespace content
