// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOutOfFlowLayoutPart_h
#define NGOutOfFlowLayoutPart_h

#include "core/CoreExport.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ComputedStyle;
class NGBlockNode;
class NGFragmentBuilder;
class NGConstraintSpace;
class NGLayoutResult;

// Helper class for positioning of out-of-flow blocks.
// It should be used together with NGFragmentBuilder.
// See NGFragmentBuilder::AddOutOfFlowChildCandidate documentation
// for example of using these classes together.
class CORE_EXPORT NGOutOfFlowLayoutPart {
  STACK_ALLOCATED();

 public:
  NGOutOfFlowLayoutPart(const NGBlockNode container,
                        const NGConstraintSpace& container_space,
                        const ComputedStyle& container_style,
                        NGFragmentBuilder* container_builder);

  // update_legacy will place NG OOF descendants into their Legacy container.
  // It should be false if OOF descendants have already been placed into Legacy.
  void Run(bool update_legacy = true);

 private:
  scoped_refptr<NGLayoutResult> LayoutDescendant(
      NGBlockNode descendant,
      NGStaticPosition static_position,
      NGLogicalOffset* offset);

  bool IsContainingBlockForDescendant(const ComputedStyle& descendant_style);

  scoped_refptr<NGLayoutResult> GenerateFragment(
      NGBlockNode node,
      const Optional<LayoutUnit>& block_estimate,
      const NGAbsolutePhysicalPosition node_position);

  const ComputedStyle& container_style_;
  NGFragmentBuilder* container_builder_;

  bool contains_absolute_;
  bool contains_fixed_;
  NGLogicalOffset content_offset_;
  NGPhysicalOffset content_physical_offset_;
  NGLogicalSize container_size_;
  NGPhysicalSize icb_size_;
};

}  // namespace blink

#endif
