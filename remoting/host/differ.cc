// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/differ.h"

#include "base/logging.h"

namespace remoting {

Differ::Differ(int width, int height, int bpp, int stride) {
  // Dimensions of screen.
  width_ = width;
  height_ = height;
  bytes_per_pixel_ = bpp;
  bytes_per_row_ = stride;

  // Calc number of blocks (full and partial) required to cover entire image.
  // One additional row/column is added as a boundary on the right & bottom.
  diff_info_width_ = (width_ + kBlockSize - 1) / kBlockSize + 1;
  diff_info_height_ = (height_ + kBlockSize - 1) / kBlockSize + 1;
  diff_info_size_ = diff_info_width_ * diff_info_height_ * sizeof(DiffInfo);
  diff_info_.reset(new uint8[diff_info_size_]);
}

Differ::~Differ() {}

void Differ::CalcDirtyRects(const void* prev_buffer, const void* curr_buffer,
                            InvalidRects* rects) {
  if (!rects) {
    return;
  }
  rects->clear();

  if (!prev_buffer || !curr_buffer) {
    return;
  }

  // Identify all the blocks that contain changed pixels.
  MarkDirtyBlocks(prev_buffer, curr_buffer);

  // Now that we've identified the blocks that have changed, merge adjacent
  // blocks to minimize the number of rects that we return.
  MergeBlocks(rects);
}

void Differ::MarkDirtyBlocks(const void* prev_buffer, const void* curr_buffer) {
  memset(diff_info_.get(), 0, diff_info_size_);

  // Calc number of full blocks.
  int x_full_blocks = width_ / kBlockSize;
  int y_full_blocks = height_ / kBlockSize;

  // Calc size of partial blocks which may be present on right and bottom edge.
  int partial_column_width = width_ - (x_full_blocks * kBlockSize);
  int partial_row_height = height_ - (y_full_blocks * kBlockSize);

  // Offset from the start of one block-column to the next.
  int block_x_offset = bytes_per_pixel_ * kBlockSize;
  // Offset from the start of one block-row to the next.
  int block_y_stride = (width_ * bytes_per_pixel_) * kBlockSize;
  // Offset from the start of one diff_info row to the next.
  int diff_info_stride = diff_info_width_ * sizeof(DiffInfo);

  const uint8* prev_block_row_start = static_cast<const uint8*>(prev_buffer);
  const uint8* curr_block_row_start = static_cast<const uint8*>(curr_buffer);
  uint8* diff_info_row_start = static_cast<uint8*>(diff_info_.get());

  for (int y = 0; y < y_full_blocks; y++) {
    const uint8* prev_block = prev_block_row_start;
    const uint8* curr_block = curr_block_row_start;
    uint8* diff_info = diff_info_row_start;

    for (int x = 0; x < x_full_blocks; x++) {
      DiffInfo diff = DiffBlock(prev_block, curr_block, bytes_per_row_);
      if (diff != 0) {
        // Mark this block as being modified so that it gets incorporated into
        // a dirty rect.
        *diff_info = diff;
      }
      prev_block += block_x_offset;
      curr_block += block_x_offset;
      diff_info += sizeof(DiffInfo);
    }

    // If there is a partial column at the end, handle it.
    if (partial_column_width != 0) {
      // TODO(garykac): Handle last partial column.
    }

    // Update pointers for next row.
    prev_block_row_start += block_y_stride;
    curr_block_row_start += block_y_stride;
    diff_info_row_start += diff_info_stride;
  }
  if (partial_row_height != 0) {
    // TODO(garykac): Handle last partial row.
  }
}

DiffInfo Differ::DiffBlock(const uint8* prev_buffer, const uint8* curr_buffer,
                           int stride) {
  const uint8* prev_row_start = prev_buffer;
  const uint8* curr_row_start = curr_buffer;

  // Number of uint64s in each row of the block.
  // This must be an integral number.
  int int64s_per_row = (kBlockSize * bytes_per_pixel_) / sizeof(uint64);
  DCHECK(((kBlockSize * bytes_per_pixel_) % sizeof(uint64)) == 0);

  for (int y = 0; y < kBlockSize; y++) {
    const uint64* prev = reinterpret_cast<const uint64*>(prev_row_start);
    const uint64* curr = reinterpret_cast<const uint64*>(curr_row_start);

    // Check each row in uint64-sized chunks.
    // Note that this check may straddle multiple pixels. This is OK because
    // we're interested in identifying whether or not there was change - we
    // don't care what the actual change is.
    for (int x = 0; x < int64s_per_row; x++) {
      if (*prev++ != *curr++) {
        return 1;
      }
    }
    prev_row_start += stride;
    curr_row_start += stride;
  }
  return 0;
}

DiffInfo Differ::DiffPartialBlock(const uint8* prev_buffer,
                                  const uint8* curr_buffer,
                                  int stride, int width, int height) {
  const uint8* prev_row_start = prev_buffer;
  const uint8* curr_row_start = curr_buffer;

  for (int y = 0; y < height; y++) {
    const uint8* prev = prev_row_start;
    const uint8* curr = curr_row_start;
    for (int x = 0; x < width; x++) {
      for (int b = 0; b < bytes_per_pixel_; b++) {
        if (*prev++ != *curr++) {
          return 1;
        }
      }
    }
    prev_row_start += bytes_per_row_;
    curr_row_start += bytes_per_row_;
  }
  return 0;
}

void Differ::MergeBlocks(InvalidRects* rects) {
  DCHECK(rects);
  rects->clear();

  uint8* diff_info_row_start = static_cast<uint8*>(diff_info_.get());
  int diff_info_stride = diff_info_width_ * sizeof(DiffInfo);

  for (int y = 0; y < diff_info_height_; y++) {
    uint8* diff_info = diff_info_row_start;
    for (int x = 0; x < diff_info_width_; x++) {
      if (*diff_info != 0) {
        // We've found a modified block. Look at blocks to the right and below
        // to group this block with as many others as we can.
        int left = x * kBlockSize;
        int top = y * kBlockSize;
        int width = 1;
        int height = 1;
        *diff_info = 0;

        // Group with blocks to the right.
        // We can keep looking until we find an unchanged block because we
        // have a boundary block which is never marked as having diffs.
        uint8* right = diff_info + 1;
        while (*right) {
          *right++ = 0;
          width++;
        }

        // Group with blocks below.
        // The entire width of blocks that we matched above much match for
        // each row that we add.
        uint8* bottom = diff_info;
        bool found_new_row;
        do {
          found_new_row = true;
          bottom += diff_info_stride;
          right = bottom;
          for (int x2 = 0; x2 < width; x2++) {
            if (*right++ == 0) {
              found_new_row = false;
            }
          }

          if (found_new_row) {
            height++;

            // We need to go back and erase the diff markers so that we don't
            // try to add these blocks a second time.
            right = bottom;
            for (int x2 = 0; x2 < width; x2++) {
              *right++ = 0;
            }
          }
        } while (found_new_row);

        // Add rect to list of dirty rects.
        width *= kBlockSize;
        if (left + width > width_) {
          width = width_ - left;
        }
        height *= kBlockSize;
        if (top + height > height_) {
          height = height_ - top;
        }
        rects->insert(gfx::Rect(left, top, width, height));
      }

      // Increment to next block in this row.
      diff_info++;
    }

    // Go to start of next row.
    diff_info_row_start += diff_info_stride;
  }
}

}  // namespace remoting
