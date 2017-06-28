// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineNode_h
#define NGInlineNode_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_item.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

template <typename OffsetMappingBuilder>
class NGInlineItemsBuilderTemplate;

class EmptyOffsetMappingBuilder;
class LayoutBlockFlow;
class LayoutNGBlockFlow;
class LayoutObject;
struct MinMaxContentSize;
class NGConstraintSpace;
class NGInlineItem;
class NGInlineItemRange;
using NGInlineItemsBuilder =
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
class NGLayoutResult;

// Represents an anonymous block box to be laid out, that contains consecutive
// inline nodes and their descendants.
class CORE_EXPORT NGInlineNode : public NGLayoutInputNode {
 public:
  NGInlineNode(LayoutNGBlockFlow*, LayoutObject*);

  LayoutBlockFlow* GetLayoutBlockFlow() const {
    return ToLayoutBlockFlow(box_);
  }
  NGLayoutInputNode NextSibling();

  RefPtr<NGLayoutResult> Layout(NGConstraintSpace*, NGBreakToken* = nullptr);

  // Computes the value of min-content and max-content for this anonymous block
  // box. min-content is the inline size when lines wrap at every break
  // opportunity, and max-content is when lines do not wrap at all.
  MinMaxContentSize ComputeMinMaxContentSize();

  // Copy fragment data of all lines to LayoutBlockFlow.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&, NGLayoutResult*);

  // Instruct to re-compute |PrepareLayout| on the next layout.
  void InvalidatePrepareLayout();

  const String& Text() const { return Data().text_content_; }
  StringView Text(unsigned start_offset, unsigned end_offset) const {
    return StringView(Data().text_content_, start_offset,
                      end_offset - start_offset);
  }

  const Vector<NGInlineItem>& Items() const { return Data().items_; }
  NGInlineItemRange Items(unsigned start_index, unsigned end_index);

  void GetLayoutTextOffsets(Vector<unsigned, 32>*);

  bool IsBidiEnabled() const { return Data().is_bidi_enabled_; }
  TextDirection BaseDirection() const { return Data().BaseDirection(); }

  void AssertOffset(unsigned index, unsigned offset) const;
  void AssertEndOffset(unsigned index, unsigned offset) const;
  void CheckConsistency() const;

  String ToString() const;

 protected:
  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayout();
  bool IsPrepareLayoutFinished() const { return !Text().IsNull(); }

  void CollectInlines(LayoutObject* start, LayoutBlockFlow*);
  LayoutObject* CollectInlines(LayoutObject* start,
                               LayoutBlockFlow*,
                               NGInlineItemsBuilder*);
  void SegmentText();
  void ShapeText();

  NGInlineNodeData& MutableData() {
    return ToLayoutNGBlockFlow(box_)->GetNGInlineNodeData();
  }
  const NGInlineNodeData& Data() const {
    return ToLayoutNGBlockFlow(box_)->GetNGInlineNodeData();
  }

  friend class NGLineBreakerTest;
};

inline void NGInlineNode::AssertOffset(unsigned index, unsigned offset) const {
  Data().items_[index].AssertOffset(offset);
}

inline void NGInlineNode::AssertEndOffset(unsigned index,
                                          unsigned offset) const {
  Data().items_[index].AssertEndOffset(offset);
}

DEFINE_TYPE_CASTS(NGInlineNode,
                  NGLayoutInputNode,
                  node,
                  node->IsInline(),
                  node.IsInline());

}  // namespace blink

#endif  // NGInlineNode_h
