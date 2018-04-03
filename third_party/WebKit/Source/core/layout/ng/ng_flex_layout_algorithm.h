// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_flex_layout_algorithm_h
#define ng_flex_layout_algorithm_h

#include "core/layout/ng/ng_layout_algorithm.h"

#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGFlexLayoutAlgorithm(NGBlockNode, const NGConstraintSpace&, NGBreakToken*);

  scoped_refptr<NGLayoutResult> Layout() override;

  Optional<MinMaxSize> ComputeMinMaxSize(const MinMaxSizeInput&) const override;
};

}  // namespace blink

#endif  // ng_flex_layout_algorithm_h
