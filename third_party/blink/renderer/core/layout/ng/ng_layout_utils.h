// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

class NGConstraintSpace;
class NGLayoutResult;

// Returns true if for a given |new_space|, the |node| will provide the same
// |NGLayoutResult| as |cached_layout_result|, and therefore might be able to
// skip layout.
bool MaySkipLayout(const NGBlockNode node,
                   const NGLayoutResult& cached_layout_result,
                   const NGConstraintSpace& new_space);

// Return true if layout is considered complete. In some cases we require more
// than one layout pass.
// This function never considers intermediate layouts with
// |NGConstraintSpace::IsIntermediateLayout| to be complete.
bool IsBlockLayoutComplete(const NGConstraintSpace&, const NGLayoutResult&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_UTILS_H_
