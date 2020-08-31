// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/utils/block_parameters_holder.h"

#include <algorithm>

#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/logging.h"
#include "src/utils/parameter_tree.h"
#include "src/utils/types.h"

namespace libgav1 {

namespace {

// Returns the number of super block rows/columns for |value4x4| where value4x4
// is either rows4x4 or columns4x4.
int RowsOrColumns4x4ToSuperBlocks(int value4x4, bool use_128x128_superblock) {
  return use_128x128_superblock ? DivideBy128(MultiplyBy4(value4x4) + 127)
                                : DivideBy64(MultiplyBy4(value4x4) + 63);
}

}  // namespace

BlockParametersHolder::BlockParametersHolder(int rows4x4, int columns4x4,
                                             bool use_128x128_superblock)
    : rows4x4_(rows4x4),
      columns4x4_(columns4x4),
      use_128x128_superblock_(use_128x128_superblock) {}

bool BlockParametersHolder::Init() {
  if (!block_parameters_cache_.Reset(rows4x4_, columns4x4_)) {
    LIBGAV1_DLOG(ERROR, "block_parameters_cache_.Reset() failed.");
    return false;
  }
  const int rows =
      RowsOrColumns4x4ToSuperBlocks(rows4x4_, use_128x128_superblock_);
  const int columns =
      RowsOrColumns4x4ToSuperBlocks(columns4x4_, use_128x128_superblock_);
  const BlockSize sb_size =
      use_128x128_superblock_ ? kBlock128x128 : kBlock64x64;
  const int multiplier = kNum4x4BlocksWide[sb_size];
  if (!trees_.Reset(rows, columns)) {
    LIBGAV1_DLOG(ERROR, "trees_.Reset() failed.");
    return false;
  }
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      trees_[i][j] =
          ParameterTree::Create(i * multiplier, j * multiplier, sb_size);
      if (trees_[i][j] == nullptr) {
        LIBGAV1_DLOG(ERROR, "Allocation of trees_[%d][%d] failed.", i, j);
        return false;
      }
    }
  }
  return true;
}

void BlockParametersHolder::FillCache(int row4x4, int column4x4,
                                      BlockSize block_size,
                                      BlockParameters* const bp) {
  int rows = std::min(static_cast<int>(kNum4x4BlocksHigh[block_size]),
                      rows4x4_ - row4x4);
  const int columns = std::min(static_cast<int>(kNum4x4BlocksWide[block_size]),
                               columns4x4_ - column4x4);
  auto* bp_dst = &block_parameters_cache_[row4x4][column4x4];
  // Specialize columns cases (values in kNum4x4BlocksWide[]) for better
  // performance.
  if (columns == 1) {
    SetBlock<BlockParameters*>(rows, 1, bp, bp_dst, columns4x4_);
  } else if (columns == 2) {
    SetBlock<BlockParameters*>(rows, 2, bp, bp_dst, columns4x4_);
  } else if (columns == 4) {
    SetBlock<BlockParameters*>(rows, 4, bp, bp_dst, columns4x4_);
  } else if (columns == 8) {
    SetBlock<BlockParameters*>(rows, 8, bp, bp_dst, columns4x4_);
  } else if (columns == 16) {
    SetBlock<BlockParameters*>(rows, 16, bp, bp_dst, columns4x4_);
  } else if (columns == 32) {
    SetBlock<BlockParameters*>(rows, 32, bp, bp_dst, columns4x4_);
  } else {
    do {
      // The following loop has better performance than using std::fill().
      // std::fill() has some overhead in checking zero loop count.
      int x = columns;
      auto* d = bp_dst;
      do {
        *d++ = bp;
      } while (--x != 0);
      bp_dst += columns4x4_;
    } while (--rows != 0);
  }
}

}  // namespace libgav1
