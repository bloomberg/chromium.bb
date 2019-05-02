// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/sparse_heap_bitmap.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

constexpr size_t kChunkRange = SparseHeapBitmap::kBitmapChunkRange;
constexpr size_t kUnitPointer = 0x1u
                                << SparseHeapBitmap::kPointerAlignmentInBits;

}  // namespace

TEST(SparseHeapBitmapTest, SparseBitmapBasic) {
  Address base = reinterpret_cast<Address>(0x10000u);
  auto bitmap = std::make_unique<SparseHeapBitmap>(base);

  size_t double_chunk = 2 * kChunkRange;

  // 101010... starting at |base|.
  for (size_t i = 0; i < double_chunk; i += 2 * kUnitPointer)
    bitmap->Add(base + i);

  // Check that hasRange() returns a bitmap subtree, if any, for a given
  // address.
  EXPECT_TRUE(!!bitmap->HasRange(base, 1));
  EXPECT_TRUE(!!bitmap->HasRange(base + kUnitPointer, 1));
  EXPECT_FALSE(!!bitmap->HasRange(base - kUnitPointer, 1));

  // Test implementation details.. that each SparseHeapBitmap node maps
  // |s_bitmapChunkRange| ranges only.
  EXPECT_EQ(bitmap->HasRange(base + kUnitPointer, 1),
            bitmap->HasRange(base + 2 * kUnitPointer, 1));
  // Second range will be just past the first.
  EXPECT_NE(bitmap->HasRange(base, 1), bitmap->HasRange(base + kChunkRange, 1));

  // Iterate a range that will encompass more than one 'chunk'.
  SparseHeapBitmap* start =
      bitmap->HasRange(base + 2 * kUnitPointer, double_chunk);
  EXPECT_TRUE(!!start);
  for (size_t i = 2 * kUnitPointer; i < double_chunk; i += 2 * kUnitPointer) {
    EXPECT_TRUE(start->IsSet(base + i));
    EXPECT_FALSE(start->IsSet(base + i + kUnitPointer));
  }
}

TEST(SparseHeapBitmapTest, SparseBitmapBuild) {
  Address base = reinterpret_cast<Address>(0x10000u);
  auto bitmap = std::make_unique<SparseHeapBitmap>(base);

  size_t double_chunk = 2 * kChunkRange;

  // Create a sparse bitmap containing at least three chunks.
  bitmap->Add(base - double_chunk);
  bitmap->Add(base + double_chunk);

  // This is sanity testing internal implementation details of
  // SparseHeapBitmap; probing |isSet()| outside the bitmap
  // of the range used in |hasRange()|, is not defined.
  //
  // Regardless, the testing here verifies that a |hasRange()| that
  // straddles multiple internal nodes, returns a bitmap that is
  // capable of returning correct |isSet()| results.
  SparseHeapBitmap* start = bitmap->HasRange(
      base - double_chunk - 2 * kUnitPointer, 4 * kUnitPointer);
  EXPECT_TRUE(!!start);
  EXPECT_TRUE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));

  start = bitmap->HasRange(base - double_chunk - 2 * kUnitPointer,
                           2 * double_chunk + 2 * kUnitPointer);
  EXPECT_TRUE(!!start);
  EXPECT_TRUE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));

  start = bitmap->HasRange(base, 20);
  EXPECT_TRUE(!!start);
  // Probing for values outside of hasRange() should be considered
  // undefined, but do it to exercise the (left) tree traversal.
  EXPECT_TRUE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_TRUE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));

  start = bitmap->HasRange(base + kChunkRange + 2 * kUnitPointer, 2048);
  EXPECT_TRUE(!!start);
  // Probing for values outside of hasRange() should be considered
  // undefined, but do it to exercise node traversal.
  EXPECT_FALSE(start->IsSet(base - double_chunk));
  EXPECT_FALSE(start->IsSet(base - double_chunk + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base));
  EXPECT_FALSE(start->IsSet(base + kUnitPointer));
  EXPECT_FALSE(start->IsSet(base + kChunkRange));
  EXPECT_TRUE(start->IsSet(base + double_chunk));
  EXPECT_FALSE(start->IsSet(base + double_chunk + kUnitPointer));
}

TEST(SparseHeapBitmapTest, SparseBitmapLeftExtension) {
  Address base = reinterpret_cast<Address>(0x10000u);
  auto bitmap = std::make_unique<SparseHeapBitmap>(base);

  SparseHeapBitmap* start = bitmap->HasRange(base, 1);
  EXPECT_TRUE(start);

  // Verify that re-adding is a no-op.
  bitmap->Add(base);
  EXPECT_EQ(start, bitmap->HasRange(base, 1));

  // Adding an Address |A| before a single-address SparseHeapBitmap node should
  // cause that node to  be "left extended" to use |A| as its new base.
  bitmap->Add(base - 2 * kUnitPointer);
  EXPECT_EQ(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));

  // Reset.
  bitmap = std::make_unique<SparseHeapBitmap>(base);

  // If attempting same as above, but the Address |A| is outside the
  // chunk size of a node, a new SparseHeapBitmap node needs to be
  // created to the left of |bitmap|.
  bitmap->Add(base - kChunkRange);
  EXPECT_NE(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));

  bitmap = std::make_unique<SparseHeapBitmap>(base);
  bitmap->Add(base - kChunkRange + kUnitPointer);
  // This address is just inside the horizon and shouldn't create a new chunk.
  EXPECT_EQ(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));
  // ..but this one should, like for the sub-test above.
  bitmap->Add(base - kChunkRange);
  EXPECT_EQ(bitmap->HasRange(base, 1),
            bitmap->HasRange(base - 2 * kUnitPointer, 1));
  EXPECT_NE(bitmap->HasRange(base, 1), bitmap->HasRange(base - kChunkRange, 1));
}

}  // namespace blink
