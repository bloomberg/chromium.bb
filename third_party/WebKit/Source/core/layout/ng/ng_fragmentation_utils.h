// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentationUtils_h
#define NGFragmentationUtils_h

#include "platform/LayoutUnit.h"

namespace blink {

class NGConstraintSpace;
class NGPhysicalFragment;

// Return the total amount of block space spent on a node by fragments
// preceding this one (but not including this one).
LayoutUnit PreviouslyUsedBlockSpace(const NGConstraintSpace&,
                                    const NGPhysicalFragment&);

// Return true if the specified fragment is the first generated fragment of
// some node.
bool IsFirstFragment(const NGConstraintSpace&, const NGPhysicalFragment&);

// Return true if the specified fragment is the final fragment of some node.
bool IsLastFragment(const NGPhysicalFragment&);

}  // namespace blink

#endif  // NGFragmentationUtils_h
