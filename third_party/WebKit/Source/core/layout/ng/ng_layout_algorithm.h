// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutAlgorithm_h
#define NGLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/MinMaxSize.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ComputedStyle;
class NGLayoutResult;

// Base class for all LayoutNG algorithms.
template <typename NGInputNodeType, typename NGBreakTokenType>
class CORE_EXPORT NGLayoutAlgorithm {
  STACK_ALLOCATED();
 public:
  NGLayoutAlgorithm(NGInputNodeType node,
                    RefPtr<const ComputedStyle> style,
                    NGConstraintSpace* space,
                    TextDirection direction,
                    NGBreakTokenType* break_token)
      : node_(node),
        constraint_space_(space),
        break_token_(break_token),
        container_builder_(node, style, space->WritingMode(), direction) {}

  NGLayoutAlgorithm(NGInputNodeType node,
                    NGConstraintSpace* space,
                    NGBreakTokenType* break_token)
      : NGLayoutAlgorithm(node,
                          &node.Style(),
                          space,
                          space->Direction(),
                          break_token) {}

  virtual ~NGLayoutAlgorithm() {}

  // Actual layout function. Lays out the children and descendants within the
  // constraints given by the NGConstraintSpace. Returns a layout result with
  // the resulting layout information.
  // TODO(layout-dev): attempt to make this function const.
  virtual RefPtr<NGLayoutResult> Layout() = 0;

  // Computes the min-content and max-content intrinsic sizes for the given box.
  // The result will not take any min-width, max-width or width properties into
  // account. If the return value is empty, the caller is expected to synthesize
  // this value from the overflow rect returned from Layout called with an
  // available width of 0 and LayoutUnit::max(), respectively.
  virtual Optional<MinMaxSize> ComputeMinMaxSize() const {
    return WTF::nullopt;
  }

 protected:
  const NGConstraintSpace& ConstraintSpace() const {
    DCHECK(constraint_space_);
    return *constraint_space_;
  }
  NGConstraintSpace* MutableConstraintSpace() { return constraint_space_; }

  const ComputedStyle& Style() const { return node_.Style(); }

  NGLogicalOffset ContainerBfcOffset() const {
    DCHECK(container_builder_.BfcOffset().has_value());
    return container_builder_.BfcOffset().value();
  }

  virtual NGInputNodeType Node() const { return node_; }

  NGBreakTokenType* BreakToken() const { return break_token_; }

  NGInputNodeType node_;
  NGConstraintSpace* constraint_space_;

  // The break token from which we are currently resuming layout.
  NGBreakTokenType* break_token_;

  NGFragmentBuilder container_builder_;
};

}  // namespace blink

#endif  // NGLayoutAlgorithm_h
