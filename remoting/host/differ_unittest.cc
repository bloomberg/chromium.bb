// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "remoting/host/differ.h"
#include "remoting/host/differ_block.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

// 96x96 screen gives a 4x4 grid of blocks.
const int kScreenWidth= 96;
const int kScreenHeight = 96;
const int kBytesPerRow = (kBytesPerPixel * kScreenWidth);

class DifferTest : public testing::Test {
 public:
  DifferTest() {
  }

 protected:
  virtual void SetUp() {
    InitDiffer(kScreenWidth, kScreenHeight, kBytesPerPixel, kBytesPerRow);
  }

  void InitDiffer(int width, int height, int bpp, int stride) {
    width_ = width;
    height_ = height;
    bytes_per_pixel_ = bpp;
    stride_ = stride;
    buffer_size_ = width_ * height_ * bytes_per_pixel_;

    differ_.reset(new Differ(width_, height_, bytes_per_pixel_, stride_));

    prev_.reset(new uint8[buffer_size_]);
    memset(prev_.get(), 0, buffer_size_);

    curr_.reset(new uint8[buffer_size_]);
    memset(curr_.get(), 0, buffer_size_);
  }

  void ClearBuffer(uint8* buffer) {
    memset(buffer, 0, buffer_size_);
  }

  // Convenience wrapper for Differ's DiffBlock that calculates the appropriate
  // offset to the start of the desired block.
  DiffInfo DiffBlock(int block_x, int block_y) {
    // Offset from upper-left of buffer to upper-left of requested block.
    int block_offset = ((block_y * stride_) + (block_x * bytes_per_pixel_))
                        * kBlockSize;
    return BlockDifference(prev_.get() + block_offset,
                           curr_.get() + block_offset,
                           stride_);
  }

  // Write the pixel |value| into the specified block in the |buffer|.
  // This is a convenience wrapper around WritePixel().
  void WriteBlockPixel(uint8* buffer, int block_x, int block_y,
                       int pixel_x, int pixel_y, uint32 value) {
    WritePixel(buffer, (block_x * kBlockSize) + pixel_x,
               (block_y * kBlockSize) + pixel_y, value);
  }

  // Write the test pixel |value| into the |buffer| at the specified |x|,|y|
  // location.
  // Only the low-order bytes from |value| are written (assuming little-endian).
  // So, for |value| = 0xaabbccdd:
  //   If bytes_per_pixel = 4, then ddccbbaa will be written as the pixel value.
  //   If                 = 3,        ddccbb
  //   If                 = 2,          ddcc
  //   If                 = 1,            dd
  void WritePixel(uint8* buffer, int x, int y, uint32 value) {
    uint8* pixel = reinterpret_cast<uint8*>(&value);
    buffer += (y * stride_) + (x * bytes_per_pixel_);
    for (int b = bytes_per_pixel_ - 1; b >= 0; b--) {
      *buffer++ = pixel[b];
    }
  }

  // DiffInfo utility routines.
  // These are here so that we don't have to make each DifferText_Xxx_Test
  // class a friend class to Differ.

  // Clear out the entire |diff_info_| buffer.
  void ClearDiffInfo() {
    memset(differ_->diff_info_.get(), 0, differ_->diff_info_size_);
  }

  // Get the value in the |diff_info_| array at (x,y).
  DiffInfo GetDiffInfo(int x, int y) {
    DiffInfo* diff_info = differ_->diff_info_.get();
    return diff_info[(y * GetDiffInfoWidth()) + x];
  }

  // Width of |diff_info_| array.
  int GetDiffInfoWidth() {
    return differ_->diff_info_width_;
  }

  // Height of |diff_info_| array.
  int GetDiffInfoHeight() {
    return differ_->diff_info_height_;
  }

  // Size of |diff_info_| array.
  int GetDiffInfoSize() {
    return differ_->diff_info_size_;
  }

  void SetDiffInfo(int x, int y, const DiffInfo& value) {
    DiffInfo* diff_info = differ_->diff_info_.get();
    diff_info[(y * GetDiffInfoWidth()) + x] = value;
  }

  // Mark the range of blocks specified.
  void MarkBlocks(int x_origin, int y_origin, int width, int height) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        SetDiffInfo(x_origin + x, y_origin + y, 1);
      }
    }
  }

  // Verify that the given dirty rect matches the expected |x|, |y|, |width|
  // and |height|.
  // |x|, |y|, |width| and |height| are specified in block (not pixel) units.
  void CheckDirtyRect(const InvalidRects& rects, int x, int y,
                      int width, int height) {
    gfx::Rect r(x * kBlockSize, y * kBlockSize,
                width * kBlockSize, height * kBlockSize);
    EXPECT_TRUE(rects.find(r) != rects.end());
  }

  // Mark the range of blocks specified and then verify that they are
  // merged correctly.
  // Only one rectangular region of blocks can be checked with this routine.
  void MarkBlocksAndCheckMerge(int x_origin, int y_origin,
                               int width, int height) {
    ClearDiffInfo();
    MarkBlocks(x_origin, y_origin, width, height);

    scoped_ptr<InvalidRects> dirty(new InvalidRects());
    differ_->MergeBlocks(dirty.get());

    ASSERT_EQ(1UL, dirty->size());
    CheckDirtyRect(*dirty.get(), x_origin, y_origin, width, height);
  }

  // The differ class we're testing.
  scoped_ptr<Differ> differ_;

  // Screen/buffer info.
  int width_;
  int height_;
  int bytes_per_pixel_;
  int stride_;

  // Size of each screen buffer.
  int buffer_size_;

  // Previous and current screen buffers.
  scoped_array<uint8> prev_;
  scoped_array<uint8> curr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DifferTest);
};

TEST_F(DifferTest, Setup) {
  // 96x96 pixels results in 3x3 array. Add 1 to each dimension as boundary.
  // +---+---+---+---+
  // | o | o | o | _ |
  // +---+---+---+---+  o = blocks mapped to screen pixels
  // | o | o | o | _ |
  // +---+---+---+---+  _ = boundary blocks
  // | o | o | o | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  EXPECT_EQ(4, GetDiffInfoWidth());
  EXPECT_EQ(4, GetDiffInfoHeight());
  EXPECT_EQ(16, GetDiffInfoSize());
}

TEST_F(DifferTest, MarkDirtyBlocks_All) {
  ClearDiffInfo();

  // Update a pixel in each block.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      WriteBlockPixel(curr_.get(), x, y, 10, 10, 0xff00ff);
    }
  }

  differ_->MarkDirtyBlocks(prev_.get(), curr_.get());

  // Make sure each block is marked as dirty.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      EXPECT_EQ(1, GetDiffInfo(x, y))
          << "when x = " << x << ", and y = " << y;
    }
  }
}

TEST_F(DifferTest, MarkDirtyBlocks_Sampling) {
  ClearDiffInfo();

  // Update some pixels in image.
  WriteBlockPixel(curr_.get(), 1, 0, 10, 10, 0xff00ff);
  WriteBlockPixel(curr_.get(), 2, 1, 10, 10, 0xff00ff);
  WriteBlockPixel(curr_.get(), 0, 2, 10, 10, 0xff00ff);

  differ_->MarkDirtyBlocks(prev_.get(), curr_.get());

  // Make sure corresponding blocks are updated.
  EXPECT_EQ(0, GetDiffInfo(0, 0));
  EXPECT_EQ(0, GetDiffInfo(0, 1));
  EXPECT_EQ(1, GetDiffInfo(0, 2));
  EXPECT_EQ(1, GetDiffInfo(1, 0));
  EXPECT_EQ(0, GetDiffInfo(1, 1));
  EXPECT_EQ(0, GetDiffInfo(1, 2));
  EXPECT_EQ(0, GetDiffInfo(2, 0));
  EXPECT_EQ(1, GetDiffInfo(2, 1));
  EXPECT_EQ(0, GetDiffInfo(2, 2));
}

TEST_F(DifferTest, DiffBlock) {
  // Verify no differences at start.
  EXPECT_EQ(0, DiffBlock(0, 0));
  EXPECT_EQ(0, DiffBlock(1, 1));

  // Write new data into the 4 corners of the middle block and verify that
  // neighboring blocks are not affected.
  int max = kBlockSize - 1;
  WriteBlockPixel(curr_.get(), 1, 1, 0, 0, 0xffffff);
  WriteBlockPixel(curr_.get(), 1, 1, 0, max, 0xffffff);
  WriteBlockPixel(curr_.get(), 1, 1, max, 0, 0xffffff);
  WriteBlockPixel(curr_.get(), 1, 1, max, max, 0xffffff);
  EXPECT_EQ(0, DiffBlock(0, 0));
  EXPECT_EQ(0, DiffBlock(0, 1));
  EXPECT_EQ(0, DiffBlock(0, 2));
  EXPECT_EQ(0, DiffBlock(1, 0));
  EXPECT_EQ(1, DiffBlock(1, 1));  // Only this block should change.
  EXPECT_EQ(0, DiffBlock(1, 2));
  EXPECT_EQ(0, DiffBlock(2, 0));
  EXPECT_EQ(0, DiffBlock(2, 1));
  EXPECT_EQ(0, DiffBlock(2, 2));
}

TEST_F(DifferTest, DiffPartialBlocks) {
  // TODO(garykac): Add tests for DiffPartialBlock
}


TEST_F(DifferTest, MergeBlocks_Empty) {
  // No blocks marked:
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();

  scoped_ptr<InvalidRects> dirty(new InvalidRects());
  differ_->MergeBlocks(dirty.get());

  EXPECT_EQ(0UL, dirty->size());
}

TEST_F(DifferTest, MergeBlocks_SingleBlock) {
  // Mark a single block and make sure that there is a single merged
  // rect with the correct bounds.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      MarkBlocksAndCheckMerge(x, y, 1, 1);
    }
  }
}

TEST_F(DifferTest, MergeBlocks_BlockRow) {
  // +---+---+---+---+
  // | X | X |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(0, 0, 2, 1);

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(0, 1, 3, 1);

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(1, 2, 2, 1);
}

TEST_F(DifferTest, MergeBlocks_BlockColumn) {
  // +---+---+---+---+
  // | X |   |   | _ |
  // +---+---+---+---+
  // | X |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(0, 0, 1, 2);

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   | X |   | _ |
  // +---+---+---+---+
  // |   | X |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(1, 1, 1, 2);

  // +---+---+---+---+
  // |   |   | X | _ |
  // +---+---+---+---+
  // |   |   | X | _ |
  // +---+---+---+---+
  // |   |   | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(2, 0, 1, 3);
}

TEST_F(DifferTest, MergeBlocks_BlockRect) {
  // +---+---+---+---+
  // | X | X |   | _ |
  // +---+---+---+---+
  // | X | X |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(0, 0, 2, 2);

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(1, 1, 2, 2);

  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(1, 0, 2, 3);

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(0, 1, 3, 2);

  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  MarkBlocksAndCheckMerge(0, 0, 3, 3);
}

// This tests marked regions that require more than 1 single dirty rect.
// The exact rects returned depend on the current implementation, so these
// may need to be updated if we modify how we merge blocks.
TEST_F(DifferTest, MergeBlocks_MultiRect) {
  scoped_ptr<InvalidRects> dirty;

  // +---+---+---+---+      +---+---+---+
  // |   | X |   | _ |      |   | 0 |   |
  // +---+---+---+---+      +---+---+---+
  // | X |   |   | _ |      | 1 |   |   |
  // +---+---+---+---+  =>  +---+---+---+
  // |   |   | X | _ |      |   |   | 2 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(1, 0, 1, 1);
  MarkBlocks(0, 1, 1, 1);
  MarkBlocks(2, 2, 1, 1);

  dirty.reset(new InvalidRects());
  differ_->MergeBlocks(dirty.get());

  ASSERT_EQ(3UL, dirty->size());
  CheckDirtyRect(*dirty.get(), 1, 0, 1, 1);
  CheckDirtyRect(*dirty.get(), 0, 1, 1, 1);
  CheckDirtyRect(*dirty.get(), 2, 2, 1, 1);

  // +---+---+---+---+      +---+---+---+
  // |   |   | X | _ |      |   |   | 0 |
  // +---+---+---+---+      +---+---+   +
  // | X | X | X | _ |      | 1   1 | 0 |
  // +---+---+---+---+  =>  +       +   +
  // | X | X | X | _ |      | 1   1 | 0 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(2, 0, 1, 3);
  MarkBlocks(0, 1, 2, 2);

  dirty.reset(new InvalidRects());
  differ_->MergeBlocks(dirty.get());

  ASSERT_EQ(2UL, dirty->size());
  CheckDirtyRect(*dirty.get(), 2, 0, 1, 3);
  CheckDirtyRect(*dirty.get(), 0, 1, 2, 2);

  // +---+---+---+---+      +---+---+---+
  // |   |   |   | _ |      |   |   |   |
  // +---+---+---+---+      +---+---+---+
  // | X |   | X | _ |      | 0 |   | 1 |
  // +---+---+---+---+  =>  +   +---+   +
  // | X | X | X | _ |      | 0 | 2 | 1 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(0, 1, 1, 1);
  MarkBlocks(2, 1, 1, 1);
  MarkBlocks(0, 2, 3, 1);

  dirty.reset(new InvalidRects());
  differ_->MergeBlocks(dirty.get());

  ASSERT_EQ(3UL, dirty->size());
  CheckDirtyRect(*dirty.get(), 0, 1, 1, 2);
  CheckDirtyRect(*dirty.get(), 2, 1, 1, 2);
  CheckDirtyRect(*dirty.get(), 1, 2, 1, 1);

  // +---+---+---+---+      +---+---+---+
  // | X | X | X | _ |      | 0   0   0 |
  // +---+---+---+---+      +---+---+---+
  // | X |   | X | _ |      | 1 |   | 2 |
  // +---+---+---+---+  =>  +   +---+   +
  // | X | X | X | _ |      | 1 | 3 | 2 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(0, 0, 3, 1);
  MarkBlocks(0, 1, 1, 1);
  MarkBlocks(2, 1, 1, 1);
  MarkBlocks(0, 2, 3, 1);

  dirty.reset(new InvalidRects());
  differ_->MergeBlocks(dirty.get());

  ASSERT_EQ(4UL, dirty->size());
  CheckDirtyRect(*dirty.get(), 0, 0, 3, 1);
  CheckDirtyRect(*dirty.get(), 0, 1, 1, 2);
  CheckDirtyRect(*dirty.get(), 2, 1, 1, 2);
  CheckDirtyRect(*dirty.get(), 1, 2, 1, 1);

  // +---+---+---+---+      +---+---+---+
  // | X | X |   | _ |      | 0   0 |   |
  // +---+---+---+---+      +       +---+
  // | X | X |   | _ |      | 0   0 |   |
  // +---+---+---+---+  =>  +---+---+---+
  // |   | X |   | _ |      |   | 1 |   |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(0, 0, 2, 2);
  MarkBlocks(1, 2, 1, 1);

  dirty.reset(new InvalidRects());
  differ_->MergeBlocks(dirty.get());

  ASSERT_EQ(2UL, dirty->size());
  CheckDirtyRect(*dirty.get(), 0, 0, 2, 2);
  CheckDirtyRect(*dirty.get(), 1, 2, 1, 1);
}

}  // namespace remoting
