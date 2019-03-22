// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

namespace blink {

namespace {

NGPercentageStorage GetPercentageStorage(LayoutUnit percentage_size,
                                         LayoutUnit available_size) {
  if (percentage_size == available_size)
    return kSameAsAvailable;

  if (percentage_size == NGSizeIndefinite)
    return kIndefinite;

  if (percentage_size == LayoutUnit())
    return kZero;

  return kRareDataPercentage;
}

}  // namespace

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetPercentageResolutionSize(
    NGLogicalSize percentage_resolution_size) {
#if DCHECK_IS_ON()
  DCHECK(is_available_size_set_);
#endif
  if (LIKELY(is_in_parallel_flow_)) {
    space_.bitfields_.percentage_inline_storage =
        GetPercentageStorage(percentage_resolution_size.inline_size,
                             space_.available_size_.inline_size);
    if (UNLIKELY(space_.bitfields_.percentage_inline_storage ==
                 kRareDataPercentage)) {
      space_.EnsureRareData()->percentage_resolution_size.inline_size =
          percentage_resolution_size.inline_size;
    }

    space_.bitfields_.percentage_block_storage =
        GetPercentageStorage(percentage_resolution_size.block_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.percentage_block_storage == kRareDataPercentage) {
      space_.EnsureRareData()->percentage_resolution_size.block_size =
          percentage_resolution_size.block_size;
    }
  } else {
    AdjustInlineSizeIfNeeded(&percentage_resolution_size.block_size);

    space_.bitfields_.percentage_inline_storage =
        GetPercentageStorage(percentage_resolution_size.block_size,
                             space_.available_size_.inline_size);
    if (space_.bitfields_.percentage_inline_storage == kRareDataPercentage) {
      space_.EnsureRareData()->percentage_resolution_size.inline_size =
          percentage_resolution_size.block_size;
    }

    space_.bitfields_.percentage_block_storage =
        GetPercentageStorage(percentage_resolution_size.inline_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.percentage_block_storage == kRareDataPercentage) {
      space_.EnsureRareData()->percentage_resolution_size.block_size =
          percentage_resolution_size.inline_size;
    }
  }

  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetReplacedPercentageResolutionSize(
    NGLogicalSize replaced_percentage_resolution_size) {
#if DCHECK_IS_ON()
  DCHECK(is_available_size_set_);
#endif
  if (LIKELY(is_in_parallel_flow_)) {
    space_.bitfields_.replaced_percentage_inline_storage =
        GetPercentageStorage(replaced_percentage_resolution_size.inline_size,
                             space_.available_size_.inline_size);
    if (UNLIKELY(space_.bitfields_.replaced_percentage_inline_storage ==
                 kRareDataPercentage)) {
      space_.EnsureRareData()->replaced_percentage_resolution_size.inline_size =
          replaced_percentage_resolution_size.inline_size;
    }

    space_.bitfields_.replaced_percentage_block_storage =
        GetPercentageStorage(replaced_percentage_resolution_size.block_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.replaced_percentage_block_storage ==
        kRareDataPercentage) {
      space_.EnsureRareData()->replaced_percentage_resolution_size.block_size =
          replaced_percentage_resolution_size.block_size;
    }
  } else {
    AdjustInlineSizeIfNeeded(&replaced_percentage_resolution_size.block_size);

    space_.bitfields_.replaced_percentage_inline_storage =
        GetPercentageStorage(replaced_percentage_resolution_size.block_size,
                             space_.available_size_.inline_size);
    if (space_.bitfields_.replaced_percentage_inline_storage ==
        kRareDataPercentage) {
      space_.EnsureRareData()->replaced_percentage_resolution_size.inline_size =
          replaced_percentage_resolution_size.block_size;
    }

    space_.bitfields_.replaced_percentage_block_storage =
        GetPercentageStorage(replaced_percentage_resolution_size.inline_size,
                             space_.available_size_.block_size);
    if (space_.bitfields_.replaced_percentage_block_storage ==
        kRareDataPercentage) {
      space_.EnsureRareData()->replaced_percentage_resolution_size.block_size =
          replaced_percentage_resolution_size.inline_size;
    }
  }

  return *this;
}

}  // namespace blink
