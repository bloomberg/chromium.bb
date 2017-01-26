// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockNode_h
#define NGBlockNode_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGBreakToken;
class NGConstraintSpace;
struct NGLogicalOffset;
class NGPhysicalFragment;
struct MinAndMaxContentSizes;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode final : public NGLayoutInputNode {
  friend NGLayoutInputNode;

 public:
  explicit NGBlockNode(LayoutObject*);

  // TODO(layout-ng): make it private and declare a friend class to use in tests
  explicit NGBlockNode(ComputedStyle*);

  ~NGBlockNode() override;

  NGPhysicalFragment* Layout(NGConstraintSpace* constraint_space) override;
  NGBlockNode* NextSibling() override;
  LayoutObject* GetLayoutObject() override;

  // Computes the value of min-content and max-content for this box.
  // If the underlying layout algorithm returns NotImplemented from
  // ComputeMinAndMaxContentSizes, this function will synthesize these sizes
  // using Layout with special constraint spaces.
  MinAndMaxContentSizes ComputeMinAndMaxContentSizes();

  const ComputedStyle* Style() const;
  ComputedStyle* MutableStyle();

  NGLayoutInputNode* FirstChild();

  void SetNextSibling(NGBlockNode*);
  void SetFirstChild(NGLayoutInputNode*);

  void SetFragment(NGPhysicalBoxFragment* fragment) { fragment_ = fragment; }
  NGBreakToken* CurrentBreakToken() const;
  bool IsLayoutFinished() const {
    return fragment_ && !fragment_->BreakToken();
  }

  DECLARE_VIRTUAL_TRACE();

  // Runs layout on layout_box_ and creates a fragment for the resulting
  // geometry.
  NGPhysicalBoxFragment* RunOldLayout(const NGConstraintSpace&);

  // Called if this is an out-of-flow block which needs to be
  // positioned with legacy layout.
  void UseOldOutOfFlowPositioning();

  // Save static position for legacy AbsPos layout.
  void SaveStaticOffsetForLegacy(const NGLogicalOffset&);
 private:

  bool CanUseNewLayout();
  bool HasInlineChildren();

  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&);

  // We can either wrap a layout_box_ or a style_/next_sibling_/first_child_
  // combination.
  LayoutBox* layout_box_;
  RefPtr<ComputedStyle> style_;
  Member<NGBlockNode> next_sibling_;
  Member<NGLayoutInputNode> first_child_;
  // TODO(mstensho): An input node may produce multiple fragments, so this
  // should probably be renamed to last_fragment_ or something like that, since
  // the last fragment is all we care about when resuming layout.
  Member<NGPhysicalBoxFragment> fragment_;
};

DEFINE_TYPE_CASTS(NGBlockNode,
                  NGLayoutInputNode,
                  node,
                  node->Type() == NGLayoutInputNode::kLegacyBlock,
                  node.Type() == NGLayoutInputNode::kLegacyBlock);

}  // namespace blink

#endif  // NGBlockNode
