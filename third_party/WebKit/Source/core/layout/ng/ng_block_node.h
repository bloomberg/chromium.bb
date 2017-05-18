// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockNode_h
#define NGBlockNode_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGBreakToken;
class NGConstraintSpace;
class NGLayoutResult;
struct NGLogicalOffset;
struct MinMaxContentSize;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode final : public NGLayoutInputNode {
  friend NGLayoutInputNode;

 public:
  explicit NGBlockNode(LayoutObject*);

  ~NGBlockNode() override;

  RefPtr<NGLayoutResult> Layout(NGConstraintSpace* constraint_space,
                                NGBreakToken* break_token = nullptr) override;
  NGLayoutInputNode* NextSibling() override;
  LayoutObject* GetLayoutObject() const override;

  // Computes the value of min-content and max-content for this box.
  // If the underlying layout algorithm's ComputeMinMaxContentSize returns
  // no value, this function will synthesize these sizes using Layout with
  // special constraint spaces -- infinite available size for max content, zero
  // available size for min content, and percentage resolution size zero for
  // both.
  MinMaxContentSize ComputeMinMaxContentSize() override;

  const ComputedStyle& Style() const override;

  NGLayoutInputNode* FirstChild();

  DECLARE_VIRTUAL_TRACE();

  // Runs layout on layout_box_ and creates a fragment for the resulting
  // geometry.
  RefPtr<NGLayoutResult> RunOldLayout(const NGConstraintSpace&);

  // Called if this is an out-of-flow block which needs to be
  // positioned with legacy layout.
  void UseOldOutOfFlowPositioning();

  // Save static position for legacy AbsPos layout.
  void SaveStaticOffsetForLegacy(const NGLogicalOffset&);

  bool CanUseNewLayout() const;

  String ToString() const override;

 private:
  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&, NGLayoutResult*);

  // We can either wrap a layout_box_ or a next_sibling_/first_child_
  // combination.
  LayoutBox* layout_box_;
  Member<NGLayoutInputNode> next_sibling_;
  Member<NGLayoutInputNode> first_child_;
};

DEFINE_TYPE_CASTS(NGBlockNode,
                  NGLayoutInputNode,
                  node,
                  node->IsBlock(),
                  node.IsBlock());

}  // namespace blink

#endif  // NGBlockNode
