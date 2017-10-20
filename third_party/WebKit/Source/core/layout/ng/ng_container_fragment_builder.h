// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGContainerFragmentBuilder_h
#define NGContainerFragmentBuilder_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_bfc_offset.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/geometry/ng_margin_strut.h"
#include "core/layout/ng/ng_base_fragment_builder.h"
#include "core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGExclusionSpace;
class NGLayoutResult;
class NGPhysicalFragment;
struct NGUnpositionedFloat;

class CORE_EXPORT NGContainerFragmentBuilder : public NGBaseFragmentBuilder {
  STACK_ALLOCATED();

 public:
  ~NGContainerFragmentBuilder() override;

  LayoutUnit InlineSize() const { return inline_size_; }
  NGContainerFragmentBuilder& SetInlineSize(LayoutUnit);

  virtual NGLogicalSize Size() const = 0;

  // The NGBfcOffset is where this fragment was positioned within the BFC. If
  // it is not set, this fragment may be placed anywhere within the BFC.
  const WTF::Optional<NGBfcOffset>& BfcOffset() const { return bfc_offset_; }
  NGContainerFragmentBuilder& SetBfcOffset(const NGBfcOffset&);

  NGContainerFragmentBuilder& SetEndMarginStrut(const NGMarginStrut&);

  NGContainerFragmentBuilder& SetExclusionSpace(
      std::unique_ptr<const NGExclusionSpace> exclusion_space);

  NGContainerFragmentBuilder& SwapUnpositionedFloats(
      Vector<scoped_refptr<NGUnpositionedFloat>>*);

  virtual NGContainerFragmentBuilder& AddChild(scoped_refptr<NGLayoutResult>,
                                               const NGLogicalOffset&);
  virtual NGContainerFragmentBuilder& AddChild(
      scoped_refptr<NGPhysicalFragment>,
      const NGLogicalOffset&);

  const Vector<scoped_refptr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  NGContainerFragmentBuilder& AddOutOfFlowDescendant(
      NGOutOfFlowPositionedDescendant);

 protected:
  // An out-of-flow positioned-candidate is a temporary data structure used
  // within the NGFragmentBuilder.
  //
  // A positioned-candidate can be:
  // 1. A direct out-of-flow positioned child. The child_offset is (0,0).
  // 2. A fragment containing an out-of-flow positioned-descendant. The
  //    child_offset in this case is the containing fragment's offset.
  //
  // The child_offset is stored as a NGLogicalOffset as the physical offset
  // cannot be computed until we know the current fragment's size.
  //
  // When returning the positioned-candidates (from
  // GetAndClearOutOfFlowDescendantCandidates), the NGFragmentBuilder will
  // convert the positioned-candidate to a positioned-descendant using the
  // physical size the fragment builder.
  struct NGOutOfFlowPositionedCandidate {
    NGOutOfFlowPositionedDescendant descendant;
    NGLogicalOffset child_offset;
  };

  NGContainerFragmentBuilder(scoped_refptr<const ComputedStyle>,
                             NGWritingMode,
                             TextDirection);

  LayoutUnit inline_size_;
  WTF::Optional<NGBfcOffset> bfc_offset_;
  NGMarginStrut end_margin_strut_;
  std::unique_ptr<const NGExclusionSpace> exclusion_space_;

  // Floats that need to be positioned by the next in-flow fragment that can
  // determine its block position in space.
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats_;

  Vector<NGOutOfFlowPositionedCandidate> oof_positioned_candidates_;
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants_;

  Vector<scoped_refptr<NGPhysicalFragment>> children_;
  Vector<NGLogicalOffset> offsets_;
};

}  // namespace blink

#endif  // NGContainerFragmentBuilder
