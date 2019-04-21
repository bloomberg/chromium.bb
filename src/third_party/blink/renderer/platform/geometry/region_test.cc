/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/region.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

#define TEST_INSIDE_RECT(r, x, y, w, h)                    \
  EXPECT_TRUE(r.Contains(IntPoint(x, y)));                 \
  EXPECT_TRUE(r.Contains(IntPoint(x + w - 1, y)));         \
  EXPECT_TRUE(r.Contains(IntPoint(x, y + h - 1)));         \
  EXPECT_TRUE(r.Contains(IntPoint(x + w - 1, y + h - 1))); \
  EXPECT_TRUE(r.Contains(IntPoint(x, y + h / 2)));         \
  EXPECT_TRUE(r.Contains(IntPoint(x + w - 1, y + h / 2))); \
  EXPECT_TRUE(r.Contains(IntPoint(x + w / 2, y)));         \
  EXPECT_TRUE(r.Contains(IntPoint(x + w / 2, y + h - 1))); \
  EXPECT_TRUE(r.Contains(IntPoint(x + w / 2, y + h / 2)));

#define TEST_LEFT_OF_RECT(r, x, y, w, h)        \
  EXPECT_FALSE(r.Contains(IntPoint(x - 1, y))); \
  EXPECT_FALSE(r.Contains(IntPoint(x - 1, y + h - 1)));

#define TEST_RIGHT_OF_RECT(r, x, y, w, h)       \
  EXPECT_FALSE(r.Contains(IntPoint(x + w, y))); \
  EXPECT_FALSE(r.Contains(IntPoint(x + w, y + h - 1)));

#define TEST_TOP_OF_RECT(r, x, y, w, h)         \
  EXPECT_FALSE(r.Contains(IntPoint(x, y - 1))); \
  EXPECT_FALSE(r.Contains(IntPoint(x + w - 1, y - 1)));

#define TEST_BOTTOM_OF_RECT(r, x, y, w, h)      \
  EXPECT_FALSE(r.Contains(IntPoint(x, y + h))); \
  EXPECT_FALSE(r.Contains(IntPoint(x + w - 1, y + h)));

TEST(RegionTest, containsPoint) {
  Region r;

  EXPECT_FALSE(r.Contains(IntPoint(0, 0)));

  r.Unite(IntRect(35, 35, 1, 1));
  TEST_INSIDE_RECT(r, 35, 35, 1, 1);
  TEST_LEFT_OF_RECT(r, 35, 35, 1, 1);
  TEST_RIGHT_OF_RECT(r, 35, 35, 1, 1);
  TEST_TOP_OF_RECT(r, 35, 35, 1, 1);
  TEST_BOTTOM_OF_RECT(r, 35, 35, 1, 1);

  r.Unite(IntRect(30, 30, 10, 10));
  TEST_INSIDE_RECT(r, 30, 30, 10, 10);
  TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
  TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
  TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 30, 30, 10, 10);

  r.Unite(IntRect(31, 40, 10, 10));
  EXPECT_FALSE(r.Contains(IntPoint(30, 40)));
  EXPECT_TRUE(r.Contains(IntPoint(31, 40)));
  EXPECT_FALSE(r.Contains(IntPoint(40, 39)));
  EXPECT_TRUE(r.Contains(IntPoint(40, 40)));

  TEST_INSIDE_RECT(r, 30, 30, 10, 10);
  TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
  TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
  TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
  TEST_INSIDE_RECT(r, 31, 40, 10, 10);
  TEST_LEFT_OF_RECT(r, 31, 40, 10, 10);
  TEST_RIGHT_OF_RECT(r, 31, 40, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 31, 40, 10, 10);

  r.Unite(IntRect(42, 40, 10, 10));

  TEST_INSIDE_RECT(r, 42, 40, 10, 10);
  TEST_LEFT_OF_RECT(r, 42, 40, 10, 10);
  TEST_RIGHT_OF_RECT(r, 42, 40, 10, 10);
  TEST_TOP_OF_RECT(r, 42, 40, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 42, 40, 10, 10);

  TEST_INSIDE_RECT(r, 30, 30, 10, 10);
  TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
  TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
  TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
  TEST_INSIDE_RECT(r, 31, 40, 10, 10);
  TEST_LEFT_OF_RECT(r, 31, 40, 10, 10);
  TEST_RIGHT_OF_RECT(r, 31, 40, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 31, 40, 10, 10);
}

TEST(RegionTest, emptySpan) {
  Region r;
  r.Unite(IntRect(5, 0, 10, 10));
  r.Unite(IntRect(0, 5, 10, 10));
  r.Subtract(IntRect(7, 7, 10, 0));

  Vector<IntRect> rects = r.Rects();
  for (size_t i = 0; i < rects.size(); ++i)
    EXPECT_FALSE(rects[i].IsEmpty());
}

#define TEST_NO_INTERSECT(a, b)      \
  {                                  \
    Region ar = a;                   \
    Region br = b;                   \
    EXPECT_FALSE(ar.Intersects(br)); \
    EXPECT_FALSE(br.Intersects(ar)); \
  }

#define TEST_INTERSECT(a, b)        \
  {                                 \
    Region ar = a;                  \
    Region br = b;                  \
    EXPECT_TRUE(ar.Intersects(br)); \
    EXPECT_TRUE(br.Intersects(ar)); \
  }

TEST(RegionTest, intersectsRegion) {
  Region r;

  TEST_NO_INTERSECT(IntRect(), IntRect());
  TEST_NO_INTERSECT(IntRect(), IntRect(0, 0, 1, 1));
  TEST_NO_INTERSECT(IntRect(), IntRect(1, 1, 1, 1));

  r.Unite(IntRect(0, 0, 1, 1));
  TEST_NO_INTERSECT(r, IntRect());
  TEST_INTERSECT(r, IntRect(0, 0, 1, 1));
  TEST_INTERSECT(r, IntRect(0, 0, 2, 2));
  TEST_INTERSECT(r, IntRect(-1, 0, 2, 2));
  TEST_INTERSECT(r, IntRect(-1, -1, 2, 2));
  TEST_INTERSECT(r, IntRect(0, -1, 2, 2));
  TEST_INTERSECT(r, IntRect(-1, -1, 3, 3));

  r.Unite(IntRect(0, 0, 3, 3));
  r.Unite(IntRect(10, 0, 3, 3));
  r.Unite(IntRect(0, 10, 13, 3));
  TEST_NO_INTERSECT(r, IntRect());
  TEST_INTERSECT(r, IntRect(1, 1, 1, 1));
  TEST_INTERSECT(r, IntRect(0, 0, 2, 2));
  TEST_INTERSECT(r, IntRect(1, 0, 2, 2));
  TEST_INTERSECT(r, IntRect(1, 1, 2, 2));
  TEST_INTERSECT(r, IntRect(0, 1, 2, 2));
  TEST_INTERSECT(r, IntRect(0, 0, 3, 3));
  TEST_INTERSECT(r, IntRect(-1, -1, 2, 2));
  TEST_INTERSECT(r, IntRect(2, -1, 2, 2));
  TEST_INTERSECT(r, IntRect(2, 2, 2, 2));
  TEST_INTERSECT(r, IntRect(-1, 2, 2, 2));

  TEST_INTERSECT(r, IntRect(11, 1, 1, 1));
  TEST_INTERSECT(r, IntRect(10, 0, 2, 2));
  TEST_INTERSECT(r, IntRect(11, 0, 2, 2));
  TEST_INTERSECT(r, IntRect(11, 1, 2, 2));
  TEST_INTERSECT(r, IntRect(10, 1, 2, 2));
  TEST_INTERSECT(r, IntRect(10, 0, 3, 3));
  TEST_INTERSECT(r, IntRect(9, -1, 2, 2));
  TEST_INTERSECT(r, IntRect(12, -1, 2, 2));
  TEST_INTERSECT(r, IntRect(12, 2, 2, 2));
  TEST_INTERSECT(r, IntRect(9, 2, 2, 2));

  TEST_INTERSECT(r, IntRect(0, -1, 13, 5));
  TEST_INTERSECT(r, IntRect(1, -1, 11, 5));
  TEST_INTERSECT(r, IntRect(2, -1, 9, 5));
  TEST_INTERSECT(r, IntRect(2, -1, 8, 5));
  TEST_INTERSECT(r, IntRect(3, -1, 8, 5));
  TEST_NO_INTERSECT(r, IntRect(3, -1, 7, 5));

  TEST_INTERSECT(r, IntRect(0, 1, 13, 1));
  TEST_INTERSECT(r, IntRect(1, 1, 11, 1));
  TEST_INTERSECT(r, IntRect(2, 1, 9, 1));
  TEST_INTERSECT(r, IntRect(2, 1, 8, 1));
  TEST_INTERSECT(r, IntRect(3, 1, 8, 1));
  TEST_NO_INTERSECT(r, IntRect(3, 1, 7, 1));

  TEST_INTERSECT(r, IntRect(0, 0, 13, 13));
  TEST_INTERSECT(r, IntRect(0, 1, 13, 11));
  TEST_INTERSECT(r, IntRect(0, 2, 13, 9));
  TEST_INTERSECT(r, IntRect(0, 2, 13, 8));
  TEST_INTERSECT(r, IntRect(0, 3, 13, 8));
  TEST_NO_INTERSECT(r, IntRect(0, 3, 13, 7));
}

TEST(RegionTest, ReadPastFullSpanVectorInIntersectsTest) {
  Region r;

  // This region has enough spans to fill its allocated Vector exactly.
  r.Unite(IntRect(400, 300, 1, 800));
  r.Unite(IntRect(785, 585, 1, 1));
  r.Unite(IntRect(787, 585, 1, 1));
  r.Unite(IntRect(0, 587, 16, 162));
  r.Unite(IntRect(26, 590, 300, 150));
  r.Unite(IntRect(196, 750, 1, 1));
  r.Unite(IntRect(0, 766, 1, 1));
  r.Unite(IntRect(0, 782, 1, 1));
  r.Unite(IntRect(745, 798, 1, 1));
  r.Unite(IntRect(795, 882, 10, 585));
  r.Unite(IntRect(100, 1499, 586, 1));
  r.Unite(IntRect(100, 1500, 585, 784));
  // This query rect goes past the bottom of the Region, causing the
  // test to reach the last span and try go past it. It should not read
  // memory off the end of the span Vector.
  TEST_NO_INTERSECT(r, IntRect(0, 2184, 1, 150));
}

#define TEST_NO_CONTAINS(a, b)     \
  {                                \
    Region ar = a;                 \
    Region br = b;                 \
    EXPECT_FALSE(ar.Contains(br)); \
  }

#define TEST_CONTAINS(a, b)       \
  {                               \
    Region ar = a;                \
    Region br = b;                \
    EXPECT_TRUE(ar.Contains(br)); \
  }

TEST(RegionTest, containsRegion) {
  TEST_CONTAINS(IntRect(), IntRect());
  TEST_NO_CONTAINS(IntRect(), IntRect(0, 0, 1, 1));
  TEST_NO_CONTAINS(IntRect(), IntRect(1, 1, 1, 1));

  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(11, 10, 1, 1));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(10, 11, 1, 1));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(9, 10, 1, 1));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(10, 9, 1, 1));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(9, 9, 2, 2));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(10, 9, 2, 2));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(9, 10, 2, 2));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(10, 10, 2, 2));
  TEST_NO_CONTAINS(IntRect(10, 10, 1, 1), IntRect(9, 9, 3, 3));

  Region h_lines;
  for (int i = 10; i < 20; i += 2)
    h_lines.Unite(IntRect(i, 10, 1, 10));

  TEST_CONTAINS(IntRect(10, 10, 9, 10), h_lines);
  TEST_NO_CONTAINS(IntRect(10, 10, 9, 9), h_lines);
  TEST_NO_CONTAINS(IntRect(10, 11, 9, 9), h_lines);
  TEST_NO_CONTAINS(IntRect(10, 10, 8, 10), h_lines);
  TEST_NO_CONTAINS(IntRect(11, 10, 8, 10), h_lines);

  Region v_lines;
  for (int i = 10; i < 20; i += 2)
    v_lines.Unite(IntRect(10, i, 10, 1));

  TEST_CONTAINS(IntRect(10, 10, 10, 9), v_lines);
  TEST_NO_CONTAINS(IntRect(10, 10, 9, 9), v_lines);
  TEST_NO_CONTAINS(IntRect(11, 10, 9, 9), v_lines);
  TEST_NO_CONTAINS(IntRect(10, 10, 10, 8), v_lines);
  TEST_NO_CONTAINS(IntRect(10, 11, 10, 8), v_lines);

  Region grid;
  for (int i = 10; i < 20; i += 2)
    for (int j = 10; j < 20; j += 2)
      grid.Unite(IntRect(i, j, 1, 1));

  TEST_CONTAINS(IntRect(10, 10, 9, 9), grid);
  TEST_NO_CONTAINS(IntRect(10, 10, 9, 8), grid);
  TEST_NO_CONTAINS(IntRect(10, 11, 9, 8), grid);
  TEST_NO_CONTAINS(IntRect(10, 10, 8, 9), grid);
  TEST_NO_CONTAINS(IntRect(11, 10, 8, 9), grid);

  TEST_CONTAINS(h_lines, h_lines);
  TEST_CONTAINS(v_lines, v_lines);
  TEST_NO_CONTAINS(v_lines, h_lines);
  TEST_NO_CONTAINS(h_lines, v_lines);
  TEST_CONTAINS(grid, grid);
  TEST_CONTAINS(h_lines, grid);
  TEST_CONTAINS(v_lines, grid);
  TEST_NO_CONTAINS(grid, h_lines);
  TEST_NO_CONTAINS(grid, v_lines);

  for (int i = 10; i < 20; i += 2)
    TEST_CONTAINS(h_lines, IntRect(i, 10, 1, 10));

  for (int i = 10; i < 20; i += 2)
    TEST_CONTAINS(v_lines, IntRect(10, i, 10, 1));

  for (int i = 10; i < 20; i += 2)
    for (int j = 10; j < 20; j += 2)
      TEST_CONTAINS(grid, IntRect(i, j, 1, 1));

  Region container;
  container.Unite(IntRect(0, 0, 40, 20));
  container.Unite(IntRect(0, 20, 41, 20));
  TEST_CONTAINS(container, IntRect(5, 5, 30, 30));

  container = Region();
  container.Unite(IntRect(0, 0, 10, 10));
  container.Unite(IntRect(0, 30, 10, 10));
  container.Unite(IntRect(30, 30, 10, 10));
  container.Unite(IntRect(30, 0, 10, 10));
  TEST_NO_CONTAINS(container, IntRect(5, 5, 30, 30));

  container = Region();
  container.Unite(IntRect(0, 0, 10, 10));
  container.Unite(IntRect(0, 30, 10, 10));
  container.Unite(IntRect(30, 0, 10, 40));
  TEST_NO_CONTAINS(container, IntRect(5, 5, 30, 30));

  container = Region();
  container.Unite(IntRect(30, 0, 10, 10));
  container.Unite(IntRect(30, 30, 10, 10));
  container.Unite(IntRect(0, 0, 10, 40));
  TEST_NO_CONTAINS(container, IntRect(5, 5, 30, 30));

  container = Region();
  container.Unite(IntRect(0, 0, 10, 40));
  container.Unite(IntRect(30, 0, 10, 40));
  TEST_NO_CONTAINS(container, IntRect(5, 5, 30, 30));

  container = Region();
  container.Unite(IntRect(0, 0, 40, 40));
  TEST_NO_CONTAINS(container, IntRect(10, -1, 20, 10));

  container = Region();
  container.Unite(IntRect(0, 0, 40, 40));
  TEST_NO_CONTAINS(container, IntRect(10, 31, 20, 10));

  container = Region();
  container.Unite(IntRect(0, 0, 40, 20));
  container.Unite(IntRect(0, 20, 41, 20));
  TEST_NO_CONTAINS(container, IntRect(-1, 10, 10, 20));

  container = Region();
  container.Unite(IntRect(0, 0, 40, 20));
  container.Unite(IntRect(0, 20, 41, 20));
  TEST_NO_CONTAINS(container, IntRect(31, 10, 10, 20));

  container = Region();
  container.Unite(IntRect(0, 0, 40, 40));
  container.Subtract(IntRect(0, 20, 60, 0));
  TEST_NO_CONTAINS(container, IntRect(31, 10, 10, 20));
}

TEST(RegionTest, unite) {
  Region r;
  Region r2;

  // A rect uniting a contained rect does not change the region.
  r2 = r = IntRect(0, 0, 50, 50);
  r2.Unite(IntRect(20, 20, 10, 10));
  EXPECT_EQ(r, r2);

  // A rect uniting a containing rect gives back the containing rect.
  r = IntRect(0, 0, 50, 50);
  r.Unite(IntRect(0, 0, 100, 100));
  EXPECT_EQ(Region(IntRect(0, 0, 100, 100)), r);

  // A complex region uniting a contained rect does not change the region.
  r = IntRect(0, 0, 50, 50);
  r.Unite(IntRect(100, 0, 50, 50));
  r2 = r;
  r2.Unite(IntRect(20, 20, 10, 10));
  EXPECT_EQ(r, r2);

  // A complex region uniting a containing rect gives back the containing rect.
  r = IntRect(0, 0, 50, 50);
  r.Unite(IntRect(100, 0, 50, 50));
  r.Unite(IntRect(0, 0, 500, 500));
  EXPECT_EQ(Region(IntRect(0, 0, 500, 500)), r);
}

TEST(RegionTest, Area) {
  Region r;
  EXPECT_EQ(0u, r.Area());

  r.Unite(IntRect(10, 20, 30, 10));
  EXPECT_EQ(300u, r.Area());

  r.Unite(IntRect(20, 10, 10, 30));
  EXPECT_EQ(500u, r.Area());

  r.Unite(IntRect(10, 10, 30, 30));
  EXPECT_EQ(900u, r.Area());

  r.Subtract(IntRect(20, 20, 10, 10));
  EXPECT_EQ(800u, r.Area());

  r.Unite(IntRect(0, 0, 50000, 50000));
  EXPECT_EQ(2500000000u, r.Area());
}

}  // namespace blink
