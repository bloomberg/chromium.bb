// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "webkit/glue/webcursor.h"

using WebKit::WebCursorInfo;

TEST(WebCursorTest, OKCursorSerialization) {
  WebCursor custom_cursor;
  // This is a valid custom cursor.
  Pickle ok_custom_pickle;
  // Type and hotspots.
  ok_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  ok_custom_pickle.WriteInt(0);
  ok_custom_pickle.WriteInt(0);
  // X & Y
  ok_custom_pickle.WriteInt(1);
  ok_custom_pickle.WriteInt(1);
  // Scale
  ok_custom_pickle.WriteFloat(1.0);
  // Data len including enough data for a 1x1 image.
  ok_custom_pickle.WriteInt(4);
  ok_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  PickleIterator iter(ok_custom_pickle);
  EXPECT_TRUE(custom_cursor.Deserialize(&iter));

#if defined(TOOLKIT_GTK)
  // On GTK+ using platforms, we should get a real native GdkCursor object back
  // (and the memory used should automatically be freed by the WebCursor object
  // for valgrind tests).
  EXPECT_TRUE(custom_cursor.GetCustomCursor());
#endif
}

TEST(WebCursorTest, BrokenCursorSerialization) {
  WebCursor custom_cursor;
  // This custom cursor has not been send with enough data.
  Pickle short_custom_pickle;
  // Type and hotspots.
  short_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  short_custom_pickle.WriteInt(0);
  short_custom_pickle.WriteInt(0);
  // X & Y
  short_custom_pickle.WriteInt(1);
  short_custom_pickle.WriteInt(1);
  // Scale
  short_custom_pickle.WriteFloat(1.0);
  // Data len not including enough data for a 1x1 image.
  short_custom_pickle.WriteInt(3);
  short_custom_pickle.WriteUInt32(0);
  PickleIterator iter(short_custom_pickle);
  EXPECT_FALSE(custom_cursor.Deserialize(&iter));

  // This custom cursor has enough data but is too big.
  Pickle large_custom_pickle;
  // Type and hotspots.
  large_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  large_custom_pickle.WriteInt(0);
  large_custom_pickle.WriteInt(0);
  // X & Y
  static const int kTooBigSize = 4096 + 1;
  large_custom_pickle.WriteInt(kTooBigSize);
  large_custom_pickle.WriteInt(1);
  // Scale
  large_custom_pickle.WriteFloat(1.0);
  // Data len including enough data for a 4097x1 image.
  large_custom_pickle.WriteInt(kTooBigSize * 4);
  for (int i = 0; i < kTooBigSize; ++i)
    large_custom_pickle.WriteUInt32(0);
  iter = PickleIterator(large_custom_pickle);
  EXPECT_FALSE(custom_cursor.Deserialize(&iter));

  // This custom cursor uses negative lengths.
  Pickle neg_custom_pickle;
  // Type and hotspots.
  neg_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  neg_custom_pickle.WriteInt(0);
  neg_custom_pickle.WriteInt(0);
  // X & Y
  neg_custom_pickle.WriteInt(-1);
  neg_custom_pickle.WriteInt(-1);
  // Scale
  neg_custom_pickle.WriteFloat(1.0);
  // Data len including enough data for a 1x1 image.
  neg_custom_pickle.WriteInt(4);
  neg_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  neg_custom_pickle.WriteUInt32(0);
  iter = PickleIterator(neg_custom_pickle);
  EXPECT_FALSE(custom_cursor.Deserialize(&iter));

  // This custom cursor uses zero scale.
  Pickle scale_zero_custom_pickle;
  // Type and hotspots.
  scale_zero_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  scale_zero_custom_pickle.WriteInt(0);
  scale_zero_custom_pickle.WriteInt(0);
  // X & Y
  scale_zero_custom_pickle.WriteInt(1);
  scale_zero_custom_pickle.WriteInt(1);
  // Scale
  scale_zero_custom_pickle.WriteFloat(0);
  // Data len including enough data for a 1x1 image.
  scale_zero_custom_pickle.WriteInt(4);
  scale_zero_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  scale_zero_custom_pickle.WriteUInt32(0);
  iter = PickleIterator(scale_zero_custom_pickle);
  EXPECT_FALSE(custom_cursor.Deserialize(&iter));

  // This custom cursor uses tiny scale.
  Pickle scale_tiny_custom_pickle;
  // Type and hotspots.
  scale_tiny_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  scale_tiny_custom_pickle.WriteInt(0);
  scale_tiny_custom_pickle.WriteInt(0);
  // X & Y
  scale_tiny_custom_pickle.WriteInt(1);
  scale_tiny_custom_pickle.WriteInt(1);
  // Scale
  scale_tiny_custom_pickle.WriteFloat(0.001f);
  // Data len including enough data for a 1x1 image.
  scale_tiny_custom_pickle.WriteInt(4);
  scale_tiny_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  scale_tiny_custom_pickle.WriteUInt32(0);
  iter = PickleIterator(scale_tiny_custom_pickle);
  EXPECT_FALSE(custom_cursor.Deserialize(&iter));
}

#if defined(OS_WIN) && !defined(USE_AURA)
TEST(WebCursorTest, WindowsCursorConversion) {
  WebCursor custom_cursor;
  Pickle win32_custom_pickle;
  WebCursor win32_custom_cursor;
  win32_custom_cursor.InitFromExternalCursor(
      reinterpret_cast<HCURSOR>(1000));
  EXPECT_TRUE(win32_custom_cursor.Serialize(&win32_custom_pickle));
  PickleIterator iter(win32_custom_pickle);
  EXPECT_TRUE(custom_cursor.Deserialize(&iter));
  EXPECT_EQ(reinterpret_cast<HCURSOR>(1000), custom_cursor.GetCursor(NULL));
}
#endif  // OS_WIN

TEST(WebCursorTest, ClampHotspot) {
  WebCursor custom_cursor;
  // This is a valid custom cursor.
  Pickle ok_custom_pickle;
  // Type and hotspots.
  ok_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  // Hotspot is invalid --- outside the bounds of the image.
  ok_custom_pickle.WriteInt(5);
  ok_custom_pickle.WriteInt(5);
  // X & Y
  ok_custom_pickle.WriteInt(2);
  ok_custom_pickle.WriteInt(2);
  // Scale
  ok_custom_pickle.WriteFloat(1.0);
  // Data len including enough data for a 2x2 image.
  ok_custom_pickle.WriteInt(4 * 4);
  for (size_t i = 0; i < 4; i++)
    ok_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  PickleIterator iter(ok_custom_pickle);
  ASSERT_TRUE(custom_cursor.Deserialize(&iter));

  // Convert to WebCursorInfo, make sure the hotspot got clamped.
  WebCursorInfo info;
  custom_cursor.GetCursorInfo(&info);
  EXPECT_EQ(gfx::Point(1, 1), gfx::Point(info.hotSpot));

  // Set hotspot to an invalid point again, pipe back through WebCursor,
  // and make sure the hotspot got clamped again.
  info.hotSpot = gfx::Point(-1, -1);
  custom_cursor.InitFromCursorInfo(info);
  custom_cursor.GetCursorInfo(&info);
  EXPECT_EQ(gfx::Point(0, 0), gfx::Point(info.hotSpot));
}

TEST(WebCursorTest, EmptyImage) {
  WebCursor custom_cursor;
  Pickle broken_cursor_pickle;
  broken_cursor_pickle.WriteInt(WebCursorInfo::TypeCustom);
  // Hotspot is at origin
  broken_cursor_pickle.WriteInt(0);
  broken_cursor_pickle.WriteInt(0);
  // X & Y are empty
  broken_cursor_pickle.WriteInt(0);
  broken_cursor_pickle.WriteInt(0);
  // Scale
  broken_cursor_pickle.WriteFloat(1.0);
  // No data for the image since the size is 0.
  broken_cursor_pickle.WriteInt(0);
  // Custom Windows message.
  broken_cursor_pickle.WriteInt(0);

  // Make sure we can read this on all platforms; it is technicaally a valid
  // cursor.
  PickleIterator iter(broken_cursor_pickle);
  ASSERT_TRUE(custom_cursor.Deserialize(&iter));

#if defined(TOOLKIT_GTK)
  // On GTK+ using platforms, we make sure that we get NULL back from this
  // method; the relevant GDK methods take NULL as a request to use the default
  // cursor.
  EXPECT_EQ(NULL, custom_cursor.GetCustomCursor());
#endif
}

TEST(WebCursorTest, Scale2) {
  WebCursor custom_cursor;
  // This is a valid custom cursor.
  Pickle ok_custom_pickle;
  // Type and hotspots.
  ok_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  ok_custom_pickle.WriteInt(0);
  ok_custom_pickle.WriteInt(0);
  // X & Y
  ok_custom_pickle.WriteInt(1);
  ok_custom_pickle.WriteInt(1);
  // Scale - 2 image pixels per UI pixel.
  ok_custom_pickle.WriteFloat(2.0);
  // Data len including enough data for a 1x1 image.
  ok_custom_pickle.WriteInt(4);
  ok_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  PickleIterator iter(ok_custom_pickle);
  EXPECT_TRUE(custom_cursor.Deserialize(&iter));

#if defined(TOOLKIT_GTK)
  // On GTK+ using platforms, we should get a real native GdkCursor object back
  // (and the memory used should automatically be freed by the WebCursor object
  // for valgrind tests).
  EXPECT_TRUE(custom_cursor.GetCustomCursor());
#endif
}
