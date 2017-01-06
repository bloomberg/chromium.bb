// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOutOfFlowLayoutPart_h
#define NGOutOfFlowLayoutPart_h

#include "core/CoreExport.h"

#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "platform/heap/Handle.h"
#include "wtf/Optional.h"

namespace blink {

class ComputedStyle;
class NGBlockNode;
class NGFragment;
class NGConstraintSpace;

// Helper class for positioning of out-of-flow blocks.
// It should be used together with NGFragmentBuilder.
// See NGFragmentBuilder::AddOutOfFlowChildCandidate documentation
// for example of using these classes together.
class CORE_EXPORT NGOutOfFlowLayoutPart
    : public GarbageCollectedFinalized<NGOutOfFlowLayoutPart> {
 public:
  NGOutOfFlowLayoutPart(PassRefPtr<const ComputedStyle>, NGLogicalSize);

  // If false, this fragment should be passed up the tree for layout by
  // an ancestor.
  bool StartLayout(NGBlockNode*, const NGStaticPosition&);
  NGLayoutStatus Layout(NGFragment**, NGLogicalOffset*);

  DECLARE_TRACE();

 private:
  bool ComputeInlineSizeEstimate();
  bool ComputeBlockSizeEstimate();
  bool ComputeNodeFragment();

  bool contains_fixed_;
  bool contains_absolute_;

  enum State {
    kComputeInlineEstimate,
    kPartialPosition,
    kComputeBlockEstimate,
    kFullPosition,
    kGenerateFragment,
    kDone
  };
  State state_;

  NGStaticPosition static_position_;
  NGLogicalOffset parent_offset_;
  NGPhysicalOffset parent_physical_offset_;
  Member<NGConstraintSpace> parent_space_;
  Member<NGBlockNode> node_;
  Member<NGConstraintSpace> node_space_;
  Member<NGFragment> node_fragment_;
  NGAbsolutePhysicalPosition node_position_;
  Optional<LayoutUnit> inline_estimate_;
  Optional<LayoutUnit> block_estimate_;
};

}  // namespace blink

#endif
