// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragmentBuilder_h
#define NGTextFragmentBuilder_h

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_text_end_effect.h"
#include "core/layout/ng/ng_base_fragment_builder.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutObject;
class NGPhysicalTextFragment;
class ShapeResult;
struct NGInlineItemResult;

class CORE_EXPORT NGTextFragmentBuilder final : public NGBaseFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGTextFragmentBuilder(NGInlineNode, WritingMode);

  // NOTE: Takes ownership of the shape result within the item result.
  void SetItem(NGInlineItemResult*, LayoutUnit line_height);
  void SetAtomicInline(scoped_refptr<const ComputedStyle>, NGLogicalSize size);
  void SetText(scoped_refptr<const ComputedStyle>,
               scoped_refptr<const ShapeResult>,
               NGLogicalSize size);

  // Creates the fragment. Can only be called once.
  scoped_refptr<NGPhysicalTextFragment> ToTextFragment(unsigned index,
                                                       unsigned start_offset,
                                                       unsigned end_offset);

 private:
  NGInlineNode inline_node_;
  NGLogicalSize size_;
  scoped_refptr<const ShapeResult> shape_result_;
  NGTextEndEffect end_effect_ = NGTextEndEffect::kNone;

  // TODO(eae): Replace with Node pointer.
  LayoutObject* layout_object_ = nullptr;

  // Not used in NG paint, only to copy to InlineTextBox::SetExpansion().
  int expansion_ = 0;
};

}  // namespace blink

#endif  // NGTextFragmentBuilder
