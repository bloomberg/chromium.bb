// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DIFFER_H_
#define REMOTING_HOST_DIFFER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "remoting/base/types.h"
#include "ui/gfx/rect.h"

namespace remoting {

typedef uint8 DiffInfo;

// Size (in pixels) of each square block used for diffing.
// This must be a multiple of sizeof(uint64).
static const int kBlockSize = 32;

class Differ {
 public:
  // Create a differ that operates on bitmaps with the specified width, height
  // and bytes_per_pixel.
  Differ(int width, int height, int bytes_per_pixel, int stride);
  ~Differ();

  // Given the previous and current screen buffer, calculate the set of
  // rectangles that enclose all the changed pixels in the new screen.
  void CalcDirtyRects(const void* prev_buffer, const void* curr_buffer,
                      InvalidRects* rects);

  // Identify all of the blocks that contain changed pixels.
  void MarkDirtyBlocks(const void* prev_buffer, const void* curr_buffer);

  // Diff a small block of image and return non-zero if there is a diff.
  // This checks only the part of the block specified by the width and
  // height parameters.
  // This is much slower than DiffBlock() since it cannot assume that the
  // full block is being checked.
  // If we force the capturer to always return images whose width/height are
  // multiples of kBlockSize, then this will never be called.
  DiffInfo DiffPartialBlock(const uint8* prev_buffer, const uint8* curr_buffer,
                            int stride, int width, int height);

  // After the dirty blocks have been identified, this routine merges adjacent
  // blocks into larger rectangular units.
  // The goal is to minimize the number of rects that cover the dirty blocks,
  // although it is not required to calc the absolute minimum of rects.
  void MergeBlocks(InvalidRects* rects);

  // Allow tests to access our private parts.
  friend class DifferTest;

 private:
  // Dimensions of screen.
  int width_;
  int height_;

  // Number of bytes for each pixel in source and dest bitmap.
  // (Yes, they must match.)
  int bytes_per_pixel_;

  // Number of bytes in each row of the image (AKA: stride).
  int bytes_per_row_;

  // Diff information for each block in the image.
  scoped_array<DiffInfo> diff_info_;

  // Dimensions and total size of diff info array.
  int diff_info_width_;
  int diff_info_height_;
  int diff_info_size_;

  DISALLOW_COPY_AND_ASSIGN(Differ);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DIFFER_H_
