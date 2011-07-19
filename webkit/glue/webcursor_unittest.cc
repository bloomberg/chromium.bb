// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "webkit/glue/webcursor.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebCursorInfo;

TEST(WebCursorTest, CursorSerialization) {
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
  // Data len including enough data for a 1x1 image.
  ok_custom_pickle.WriteInt(4);
  ok_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  void* iter = NULL;
  EXPECT_TRUE(custom_cursor.Deserialize(&ok_custom_pickle, &iter));

  // This custom cursor has not been send with enough data.
  Pickle short_custom_pickle;
  // Type and hotspots.
  short_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  short_custom_pickle.WriteInt(0);
  short_custom_pickle.WriteInt(0);
  // X & Y
  short_custom_pickle.WriteInt(1);
  short_custom_pickle.WriteInt(1);
  // Data len not including enough data for a 1x1 image.
  short_custom_pickle.WriteInt(3);
  short_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  iter = NULL;
  EXPECT_FALSE(custom_cursor.Deserialize(&short_custom_pickle, &iter));

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
  // Data len including enough data for a 4097x1 image.
  large_custom_pickle.WriteInt(kTooBigSize * 4);
  for (int i = 0; i < kTooBigSize; ++i)
    large_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  iter = NULL;
  EXPECT_FALSE(custom_cursor.Deserialize(&large_custom_pickle, &iter));

  // This custom cursor uses negative lengths.
  Pickle neg_custom_pickle;
  // Type and hotspots.
  neg_custom_pickle.WriteInt(WebCursorInfo::TypeCustom);
  neg_custom_pickle.WriteInt(0);
  neg_custom_pickle.WriteInt(0);
  // X & Y
  neg_custom_pickle.WriteInt(-1);
  neg_custom_pickle.WriteInt(-1);
  // Data len including enough data for a 1x1 image.
  neg_custom_pickle.WriteInt(4);
  neg_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  neg_custom_pickle.WriteUInt32(0);
  iter = NULL;
  EXPECT_FALSE(custom_cursor.Deserialize(&neg_custom_pickle, &iter));

#if defined(OS_WIN)
  Pickle win32_custom_pickle;
  WebCursor win32_custom_cursor;
  win32_custom_cursor.InitFromExternalCursor(
      reinterpret_cast<HCURSOR>(1000));
  EXPECT_TRUE(win32_custom_cursor.Serialize(&win32_custom_pickle));
  iter = NULL;
  EXPECT_TRUE(custom_cursor.Deserialize(&win32_custom_pickle, &iter));
  EXPECT_EQ(reinterpret_cast<HCURSOR>(1000), custom_cursor.GetCursor(NULL));
#endif  // OS_WIN
}

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
  // Data len including enough data for a 2x2 image.
  ok_custom_pickle.WriteInt(4 * 4);
  for (size_t i = 0; i < 4; i++)
    ok_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteUInt32(0);
  void* iter = NULL;
  ASSERT_TRUE(custom_cursor.Deserialize(&ok_custom_pickle, &iter));

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
