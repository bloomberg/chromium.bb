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

#include "src/utils/parameter_tree.h"

#include <cassert>
#include <memory>
#include <new>

#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/logging.h"
#include "src/utils/types.h"

namespace libgav1 {

// static
std::unique_ptr<ParameterTree> ParameterTree::Create(int row4x4, int column4x4,
                                                     BlockSize block_size,
                                                     bool is_leaf) {
  std::unique_ptr<ParameterTree> tree(
      new (std::nothrow) ParameterTree(row4x4, column4x4, block_size));
  if (tree != nullptr && is_leaf && !tree->SetPartitionType(kPartitionNone)) {
    tree = nullptr;
  }
  return tree;
}

bool ParameterTree::SetPartitionType(Partition partition) {
  assert(!partition_type_set_);
  partition_ = partition;
  partition_type_set_ = true;
  const int block_width4x4 = kNum4x4BlocksWide[block_size_];
  const int half_block4x4 = block_width4x4 >> 1;
  const int quarter_block4x4 = half_block4x4 >> 1;
  const BlockSize sub_size = kSubSize[partition][block_size_];
  const BlockSize split_size = kSubSize[kPartitionSplit][block_size_];
  assert(partition == kPartitionNone || sub_size != kBlockInvalid);
  switch (partition) {
    case kPartitionNone:
      parameters_.reset(new (std::nothrow) BlockParameters());
      return parameters_ != nullptr;
    case kPartitionHorizontal:
      children_[0] = ParameterTree::Create(row4x4_, column4x4_, sub_size, true);
      children_[1] = ParameterTree::Create(row4x4_ + half_block4x4, column4x4_,
                                           sub_size, true);
      return children_[0] != nullptr && children_[1] != nullptr;
    case kPartitionVertical:
      children_[0] = ParameterTree::Create(row4x4_, column4x4_, sub_size, true);
      children_[1] = ParameterTree::Create(row4x4_, column4x4_ + half_block4x4,
                                           sub_size, true);
      return children_[0] != nullptr && children_[1] != nullptr;
    case kPartitionSplit:
      children_[0] =
          ParameterTree::Create(row4x4_, column4x4_, sub_size, false);
      children_[1] = ParameterTree::Create(row4x4_, column4x4_ + half_block4x4,
                                           sub_size, false);
      children_[2] = ParameterTree::Create(row4x4_ + half_block4x4, column4x4_,
                                           sub_size, false);
      children_[3] = ParameterTree::Create(
          row4x4_ + half_block4x4, column4x4_ + half_block4x4, sub_size, false);
      return children_[0] != nullptr && children_[1] != nullptr &&
             children_[2] != nullptr && children_[3] != nullptr;
    case kPartitionHorizontalWithTopSplit:
      assert(split_size != kBlockInvalid);
      children_[0] =
          ParameterTree::Create(row4x4_, column4x4_, split_size, true);
      children_[1] = ParameterTree::Create(row4x4_, column4x4_ + half_block4x4,
                                           split_size, true);
      children_[2] = ParameterTree::Create(row4x4_ + half_block4x4, column4x4_,
                                           sub_size, true);
      return children_[0] != nullptr && children_[1] != nullptr &&
             children_[2] != nullptr;
    case kPartitionHorizontalWithBottomSplit:
      assert(split_size != kBlockInvalid);
      children_[0] = ParameterTree::Create(row4x4_, column4x4_, sub_size, true);
      children_[1] = ParameterTree::Create(row4x4_ + half_block4x4, column4x4_,
                                           split_size, true);
      children_[2] =
          ParameterTree::Create(row4x4_ + half_block4x4,
                                column4x4_ + half_block4x4, split_size, true);
      return children_[0] != nullptr && children_[1] != nullptr &&
             children_[2] != nullptr;
    case kPartitionVerticalWithLeftSplit:
      assert(split_size != kBlockInvalid);
      children_[0] =
          ParameterTree::Create(row4x4_, column4x4_, split_size, true);
      children_[1] = ParameterTree::Create(row4x4_ + half_block4x4, column4x4_,
                                           split_size, true);
      children_[2] = ParameterTree::Create(row4x4_, column4x4_ + half_block4x4,
                                           sub_size, true);
      return children_[0] != nullptr && children_[1] != nullptr &&
             children_[2] != nullptr;
    case kPartitionVerticalWithRightSplit:
      assert(split_size != kBlockInvalid);
      children_[0] = ParameterTree::Create(row4x4_, column4x4_, sub_size, true);
      children_[1] = ParameterTree::Create(row4x4_, column4x4_ + half_block4x4,
                                           split_size, true);
      children_[2] =
          ParameterTree::Create(row4x4_ + half_block4x4,
                                column4x4_ + half_block4x4, split_size, true);
      return children_[0] != nullptr && children_[1] != nullptr &&
             children_[2] != nullptr;
    case kPartitionHorizontal4:
      for (int i = 0; i < 4; ++i) {
        children_[i] = ParameterTree::Create(row4x4_ + i * quarter_block4x4,
                                             column4x4_, sub_size, true);
        if (children_[i] == nullptr) return false;
      }
      return true;
    default:
      assert(partition == kPartitionVertical4);
      for (int i = 0; i < 4; ++i) {
        children_[i] = ParameterTree::Create(
            row4x4_, column4x4_ + i * quarter_block4x4, sub_size, true);
        if (children_[i] == nullptr) return false;
      }
      return true;
  }
}

}  // namespace libgav1
