// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockNode_h
#define NGBlockNode_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGConstraintSpace;
class NGFragmentBase;
class NGLayoutAlgorithm;
class NGLayoutCoordinator;
class NGPhysicalFragment;
struct MinAndMaxContentSizes;

// Represents a node to be laid out.
class CORE_EXPORT NGBlockNode final : public NGLayoutInputNode {
  friend NGLayoutInputNode;

 public:
  explicit NGBlockNode(LayoutObject*);

  // TODO(layout-ng): make it private and declare a friend class to use in tests
  explicit NGBlockNode(ComputedStyle*);

  bool Layout(const NGConstraintSpace*, NGFragmentBase**) override;
  NGBlockNode* NextSibling() override;

  // Computes the value of min-content and max-content for this box.
  // The return value has the same meaning as for Layout.
  // If the underlying layout algorithm returns NotImplemented from
  // ComputeMinAndMaxContentSizes, this function will synthesize these sizes
  // using Layout with special constraint spaces.
  // It is not legal to interleave a pending Layout() with a pending
  // ComputeOrSynthesizeMinAndMaxContentSizes (i.e. you have to call Layout
  // often enough that it returns true before calling
  // ComputeOrSynthesizeMinAndMaxContentSizes)
  bool ComputeMinAndMaxContentSizes(MinAndMaxContentSizes*);

  const ComputedStyle* Style() const;
  ComputedStyle* MutableStyle();

  NGLayoutInputNode* FirstChild();

  void SetNextSibling(NGBlockNode*);
  void SetFirstChild(NGLayoutInputNode*);

  DECLARE_VIRTUAL_TRACE();

 private:
  // This is necessary for interop between old and new trees -- after our parent
  // positions us, it calls this function so we can store the position on the
  // underlying LayoutBox.
  void PositionUpdated();

  bool CanUseNewLayout();
  bool HasInlineChildren();

  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&);

  // Runs layout on layout_box_ and creates a fragment for the resulting
  // geometry.
  NGPhysicalFragment* RunOldLayout(const NGConstraintSpace&);

  // We can either wrap a layout_box_ or a style_/next_sibling_/first_child_
  // combination.
  LayoutBox* layout_box_;
  RefPtr<ComputedStyle> style_;
  Member<NGBlockNode> next_sibling_;
  Member<NGLayoutInputNode> first_child_;
  Member<NGLayoutCoordinator> layout_coordinator_;
  Member<NGLayoutAlgorithm> minmax_algorithm_;
  Member<NGPhysicalFragment> fragment_;
};

DEFINE_TYPE_CASTS(NGBlockNode,
                  NGLayoutInputNode,
                  node,
                  node->Type() == NGLayoutInputNode::LegacyBlock,
                  node.Type() == NGLayoutInputNode::LegacyBlock);

}  // namespace blink

#endif  // NGBlockNode
