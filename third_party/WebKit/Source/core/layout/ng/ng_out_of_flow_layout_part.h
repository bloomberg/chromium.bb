// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOutOfFlowLayoutPart_h
#define NGOutOfFlowLayoutPart_h

#include "core/CoreExport.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "core/layout/ng/ng_units.h"
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

  void Layout(NGBlockNode&, NGStaticPosition, NGFragment**, NGLogicalOffset*);

  DECLARE_TRACE();

 private:
  NGFragment* GenerateFragment(NGBlockNode& node,
                               const Optional<LayoutUnit>& block_estimate,
                               const NGAbsolutePhysicalPosition node_position);

  NGLogicalOffset parent_border_offset_;
  NGPhysicalOffset parent_border_physical_offset_;
  Member<NGConstraintSpace> parent_space_;
};

}  // namespace blink

#endif
