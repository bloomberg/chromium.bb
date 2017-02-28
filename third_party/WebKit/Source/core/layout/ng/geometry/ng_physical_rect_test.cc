// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_rect.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(NGGeometryUnitsTest, SnapToDevicePixels) {
  NGPhysicalRect rect(NGPhysicalLocation(LayoutUnit(4.5), LayoutUnit(9.5)),
                      NGPhysicalSize(LayoutUnit(10.5), LayoutUnit(11.5)));
  NGPixelSnappedPhysicalRect snappedRect = rect.SnapToDevicePixels();
  EXPECT_EQ(5, snappedRect.left);
  EXPECT_EQ(10, snappedRect.top);
  EXPECT_EQ(10, snappedRect.width);
  EXPECT_EQ(11, snappedRect.height);

  rect = NGPhysicalRect(NGPhysicalLocation(LayoutUnit(4), LayoutUnit(9)),
                        NGPhysicalSize(LayoutUnit(10.5), LayoutUnit(11.5)));
  snappedRect = rect.SnapToDevicePixels();
  EXPECT_EQ(4, snappedRect.left);
  EXPECT_EQ(9, snappedRect.top);
  EXPECT_EQ(11, snappedRect.width);
  EXPECT_EQ(12, snappedRect.height);

  rect = NGPhysicalRect(NGPhysicalLocation(LayoutUnit(1.3), LayoutUnit(1.6)),
                        NGPhysicalSize(LayoutUnit(5.8), LayoutUnit(4.3)));
  snappedRect = rect.SnapToDevicePixels();
  EXPECT_EQ(1, snappedRect.left);
  EXPECT_EQ(2, snappedRect.top);
  EXPECT_EQ(6, snappedRect.width);
  EXPECT_EQ(4, snappedRect.height);

  rect = NGPhysicalRect(NGPhysicalLocation(LayoutUnit(1.6), LayoutUnit(1.3)),
                        NGPhysicalSize(LayoutUnit(5.8), LayoutUnit(4.3)));
  snappedRect = rect.SnapToDevicePixels();
  EXPECT_EQ(2, snappedRect.left);
  EXPECT_EQ(1, snappedRect.top);
  EXPECT_EQ(5, snappedRect.width);
  EXPECT_EQ(5, snappedRect.height);
}

}  // namespace

}  // namespace blink
