// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragmentation_utils.h"

#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

// Return the total amount of block space spent on a node by fragments
// preceding this one (but not including this one).
LayoutUnit PreviouslyUsedBlockSpace(const NGConstraintSpace& constraint_space,
                                    const NGPhysicalFragment& fragment) {
  if (!fragment.IsBox())
    return LayoutUnit();
  const auto* break_token = ToNGBlockBreakToken(fragment.BreakToken());
  if (!break_token)
    return LayoutUnit();
  NGBoxFragment logical_fragment(constraint_space.WritingMode(),
                                 ToNGPhysicalBoxFragment(fragment));
  return break_token->UsedBlockSize() - logical_fragment.BlockSize();
}

// Return true if the specified fragment is the first generated fragment of
// some node.
bool IsFirstFragment(const NGConstraintSpace& constraint_space,
                     const NGPhysicalFragment& fragment) {
  // TODO(mstensho): Figure out how to behave for non-box fragments here. How
  // can we tell whether it's the first one? Looking for previously used block
  // space certainly isn't the answer.
  if (!fragment.IsBox())
    return true;
  return PreviouslyUsedBlockSpace(constraint_space, fragment) <= LayoutUnit();
}

// Return true if the specified fragment is the final fragment of some node.
bool IsLastFragment(const NGPhysicalFragment& fragment) {
  const auto* break_token = fragment.BreakToken();
  return !break_token || break_token->IsFinished();
}

}  // namespace blink
