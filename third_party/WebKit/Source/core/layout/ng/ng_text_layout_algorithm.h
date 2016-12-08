// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineLayoutAlgorithm_h
#define NGInlineLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class NGBreakToken;
class NGConstraintSpace;
class NGInlineNode;

// A class for text layout. This takes a NGInlineNode which consists only
// non-atomic inlines and produces NGTextFragments.
//
// Unlike other layout algorithms this takes a NGInlineNode as its input instead
// of the ComputedStyle as it operates over multiple inlines with different
// style.
class CORE_EXPORT NGTextLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param inline_box The inline box to produce fragments from.
  // @param space The constraint space which the algorithm should generate a
  //              fragments within.
  NGTextLayoutAlgorithm(NGInlineNode* inline_box,
                        NGConstraintSpace* space,
                        NGBreakToken* break_token = nullptr);

  NGLayoutStatus Layout(NGFragmentBase*,
                        NGPhysicalFragmentBase**,
                        NGLayoutAlgorithm**) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<NGInlineNode> inline_box_;
  Member<NGConstraintSpace> constraint_space_;
  Member<NGBreakToken> break_token_;
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
