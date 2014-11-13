// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/path_win.h"

#include <algorithm>
#include <vector>

#include "base/win/scoped_gdi_object.h"
#include "skia/ext/skia_utils_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/path.h"

namespace gfx {

namespace {

// Get rectangles from the |region| and convert them to SkIRect.
std::vector<SkIRect> GetRectsFromHRGN(HRGN region) {
  // Determine the size of output buffer required to receive the region.
  DWORD bytes_size = GetRegionData(region, 0, NULL);
  CHECK_NE((DWORD)0, bytes_size);

  // Fetch the Windows RECTs that comprise the region.
  std::vector<char> buffer(bytes_size);
  LPRGNDATA region_data = reinterpret_cast<LPRGNDATA>(buffer.data());
  DWORD result = GetRegionData(region, bytes_size, region_data);
  CHECK_EQ(bytes_size, result);

  // Pull out the rectangles into a SkIRect vector to return to caller.
  const LPRECT rects = reinterpret_cast<LPRECT>(&region_data->Buffer[0]);
  std::vector<SkIRect> sk_rects(region_data->rdh.nCount);
  std::transform(rects, rects + region_data->rdh.nCount,
                 sk_rects.begin(), skia::RECTToSkIRect);

  return sk_rects;
}

}  // namespace

// Test that rectangle with round corners stil has round corners after
// converting from SkPath to the HRGN.
TEST(CreateHRGNFromSkPathTest, RoundCornerTest) {
  const SkIRect rects[] = {
      { 17, 0, 33, 1},
      { 12, 1, 38, 2},
      { 11, 2, 39, 3},
      { 9, 3, 41, 4},
      { 8, 4, 42, 5},
      { 6, 5, 44, 6},
      { 5, 6, 45, 8},
      { 4, 8, 46, 9},
      { 3, 9, 47, 11},
      { 2, 11, 48, 12},
      { 1, 12, 49, 17},
      { 0, 17, 50, 33},
      { 1, 33, 49, 38},
      { 2, 38, 48, 39},
      { 3, 39, 47, 41},
      { 4, 41, 46, 42},
      { 5, 42, 45, 44},
      { 6, 44, 44, 45},
      { 8, 45, 42, 46},
      { 9, 46, 41, 47},
      { 11, 47, 39, 48},
      { 12, 48, 38, 49},
      { 17, 49, 33, 50},
  };

  Path path;
  SkRRect rrect;
  rrect.setRectXY(SkRect::MakeWH(50, 50), 20, 20);
  path.addRRect(rrect);
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  const std::vector<SkIRect>& region_rects = GetRectsFromHRGN(region);
  EXPECT_EQ(arraysize(rects), region_rects.size());
  for (size_t i = 0; i < arraysize(rects) && i < region_rects.size(); ++i)
    EXPECT_EQ(rects[i], region_rects[i]);
}

// Check that a path enclosing two non-adjacent areas is correctly translated
// to a non-contiguous region.
TEST(CreateHRGNFromSkPathTest, NonContiguousPath) {
  const SkIRect rects[] = {
      { 0, 0, 50, 50},
      { 100, 100, 150, 150},
  };

  Path path;
  for (const SkIRect& rect : rects) {
    path.addRect(SkRect::Make(rect));
  }
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  const std::vector<SkIRect>& region_rects = GetRectsFromHRGN(region);
  ASSERT_EQ(arraysize(rects), region_rects.size());
  for (size_t i = 0; i < arraysize(rects); ++i)
    EXPECT_EQ(rects[i], region_rects[i]);
}

// Check that empty region is returned for empty path.
TEST(CreateHRGNFromSkPathTest, EmptyPath) {
  Path path;
  base::win::ScopedRegion empty_region(::CreateRectRgn(0, 0, 0, 0));
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  EXPECT_TRUE(::EqualRgn(empty_region, region));
}

}  // namespace gfx

