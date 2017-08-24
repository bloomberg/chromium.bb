// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockNode_h
#define NGBlockNode_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "core/layout/ng/ng_layout_input_node.h"

namespace blink {

class LayoutBox;
class NGBreakToken;
class NGConstraintSpace;
class NGFragmentBuilder;
class NGLayoutResult;
class NGPhysicalFragment;
struct MinMaxSize;
struct NGBaselineRequest;
struct NGLogicalOffset;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode final : public NGLayoutInputNode {
  friend NGLayoutInputNode;
 public:
  explicit NGBlockNode(LayoutBox*);

  RefPtr<NGLayoutResult> Layout(const NGConstraintSpace& constraint_space,
                                NGBreakToken* break_token = nullptr);
  NGLayoutInputNode NextSibling() const;

  // Computes the value of min-content and max-content for this box.
  // If the underlying layout algorithm's ComputeMinMaxSize returns
  // no value, this function will synthesize these sizes using Layout with
  // special constraint spaces -- infinite available size for max content, zero
  // available size for min content, and percentage resolution size zero for
  // both.
  MinMaxSize ComputeMinMaxSize();

  NGLayoutInputNode FirstChild();

  // Runs layout on the underlying LayoutObject and creates a fragment for the
  // resulting geometry.
  RefPtr<NGLayoutResult> RunOldLayout(const NGConstraintSpace&);

  // Called if this is an out-of-flow block which needs to be
  // positioned with legacy layout.
  void UseOldOutOfFlowPositioning();

  // Save static position for legacy AbsPos layout.
  void SaveStaticOffsetForLegacy(const NGLogicalOffset&);

  bool CanUseNewLayout() const;

  String ToString() const;

 private:
  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&, NGLayoutResult*);
  void CopyChildFragmentPosition(
      const NGPhysicalFragment& fragment,
      const NGPhysicalOffset& additional_offset = NGPhysicalOffset());

  void CopyBaselinesFromOldLayout(const NGConstraintSpace&, NGFragmentBuilder*);
  void AddAtomicInlineBaselineFromOldLayout(const NGBaselineRequest&,
                                            bool,
                                            NGFragmentBuilder*);
};

DEFINE_TYPE_CASTS(NGBlockNode,
                  NGLayoutInputNode,
                  node,
                  node->IsBlock(),
                  node.IsBlock());

}  // namespace blink

#endif  // NGBlockNode
