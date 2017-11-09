// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineNode_h
#define NGInlineNode_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/inline/ng_inline_item.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

template <typename OffsetMappingBuilder>
class NGInlineItemsBuilderTemplate;

class EmptyOffsetMappingBuilder;
class LayoutBlockFlow;
struct MinMaxSize;
class NGConstraintSpace;
class NGInlineItem;
class NGInlineItemRange;
using NGInlineItemsBuilder =
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
struct NGInlineNodeData;
class NGLayoutResult;
class NGOffsetMapping;

// Represents an anonymous block box to be laid out, that contains consecutive
// inline nodes and their descendants.
class CORE_EXPORT NGInlineNode : public NGLayoutInputNode {
 public:
  NGInlineNode(LayoutBlockFlow*);

  LayoutBlockFlow* GetLayoutBlockFlow() const {
    return ToLayoutBlockFlow(box_);
  }
  NGLayoutInputNode NextSibling() { return nullptr; }

  // True in quirks mode or limited-quirks mode, which require line-height
  // quirks.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  bool InLineHeightQuirksMode() const;

  scoped_refptr<NGLayoutResult> Layout(const NGConstraintSpace&,
                                       NGBreakToken* = nullptr);

  // Computes the value of min-content and max-content for this anonymous block
  // box. min-content is the inline size when lines wrap at every break
  // opportunity, and max-content is when lines do not wrap at all.
  MinMaxSize ComputeMinMaxSize();

  // Copy fragment data of all lines to LayoutBlockFlow.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&,
                                   const NGLayoutResult&);

  // Instruct to re-compute |PrepareLayout| on the next layout.
  void InvalidatePrepareLayout();

  const String& Text() const { return Data().text_content_; }
  StringView Text(unsigned start_offset, unsigned end_offset) const {
    return StringView(Data().text_content_, start_offset,
                      end_offset - start_offset);
  }

  const Vector<NGInlineItem>& Items(bool is_first_line = false) const;
  NGInlineItemRange Items(unsigned start_index, unsigned end_index);

  void GetLayoutTextOffsets(Vector<unsigned, 32>*);

  // Returns the DOM to text content offset mapping of this block. If it is not
  // computed before, compute and store it in NGInlineNodeData.
  // This funciton must be called with clean layout.
  const NGOffsetMapping* ComputeOffsetMappingIfNeeded();

  bool IsBidiEnabled() const { return Data().is_bidi_enabled_; }
  TextDirection BaseDirection() const { return Data().BaseDirection(); }

  bool IsEmptyInline() { return EnsureData().is_empty_inline_; }

  void AssertOffset(unsigned index, unsigned offset) const;
  void AssertEndOffset(unsigned index, unsigned offset) const;
  void CheckConsistency() const;

  String ToString() const;

 protected:
  bool IsPrepareLayoutFinished() const;

  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayoutIfNeeded();

  void CollectInlines(NGInlineNodeData*);
  void SegmentText(NGInlineNodeData*);
  void ShapeText(NGInlineNodeData*);
  void ShapeText(const String&, Vector<NGInlineItem>*);
  void ShapeTextForFirstLineIfNeeded(NGInlineNodeData*);

  NGInlineNodeData* MutableData();
  const NGInlineNodeData& Data() const;
  const NGInlineNodeData& EnsureData();

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
