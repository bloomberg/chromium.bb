// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/path_win.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/win/scoped_gdi_object.h"
#include "skia/ext/skia_utils_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace gfx {
namespace {

std::vector<SkIRect> GetRectsFromHRGN(HRGN region) {
  // Determine the size of output buffer required to receive the region.
  DWORD bytes_size = GetRegionData(region, 0, NULL);
  CHECK(bytes_size != 0);

  // Fetch the Windows RECTs that comprise the region.
  std::vector<char> buffer(bytes_size);;
  LPRGNDATA region_data = reinterpret_cast<LPRGNDATA>(buffer.data());
  DWORD result = GetRegionData(region, bytes_size, region_data);
  CHECK(result == bytes_size);

  // Pull out the rectangles into a SkIRect vector to pass to setRects().
  const LPRECT rects = reinterpret_cast<LPRECT>(&region_data->Buffer[0]);
  std::vector<SkIRect> sk_rects(region_data->rdh.nCount);
  std::transform(rects, rects + region_data->rdh.nCount,
                 sk_rects.begin(), skia::RECTToSkIRect);

  return sk_rects;
}

TEST(PathWinTest, BasicSkPathToHRGN) {
  SkPath path;
  path.moveTo(0.0f, 0.0f);
  path.rLineTo(100.0f, 0.0f);
  path.rLineTo(0.0f, 100.0f);
  path.rLineTo(-100.0f, 0.0f);
  path.rLineTo(0.0f, -100.0f);
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  std::vector<SkIRect> result = GetRectsFromHRGN(region);
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ(0, result[0].left());
  EXPECT_EQ(0, result[0].top());
  EXPECT_EQ(100, result[0].right());
  EXPECT_EQ(100, result[0].bottom());
}

TEST(PathWinTest, NonContiguousSkPathToHRGN) {
  SkPath path;
  path.moveTo(0.0f, 0.0f);
  path.rLineTo(100.0f, 0.0f);
  path.rLineTo(0.0f, 100.0f);
  path.rLineTo(-100.0f, 0.0f);
  path.rLineTo(0.0f, -100.0f);
  path.moveTo(150.0f, 0.0f);
  path.rLineTo(100.0f, 0.0f);
  path.rLineTo(0.0f, 100.0f);
  path.rLineTo(-100.0f, 0.0f);
  path.rLineTo(0.0f, -100.0f);
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  std::vector<SkIRect> result = GetRectsFromHRGN(region);
  EXPECT_EQ(2U, result.size());
  EXPECT_EQ(0, result[0].left());
  EXPECT_EQ(0, result[0].top());
  EXPECT_EQ(100, result[0].right());
  EXPECT_EQ(100, result[0].bottom());
  EXPECT_EQ(150, result[1].left());
  EXPECT_EQ(0, result[1].top());
  EXPECT_EQ(250, result[1].right());
  EXPECT_EQ(100, result[1].bottom());
}

}  // namespace
}  // namespace gfx
