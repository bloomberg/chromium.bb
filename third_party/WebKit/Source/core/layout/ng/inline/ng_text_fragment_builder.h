// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragmentBuilder_h
#define NGTextFragmentBuilder_h

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_base_fragment_builder.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class NGPhysicalTextFragment;
class ShapeResult;

class CORE_EXPORT NGTextFragmentBuilder final : public NGBaseFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGTextFragmentBuilder(NGInlineNode,
                        RefPtr<const ComputedStyle>,
                        NGWritingMode);
  NGTextFragmentBuilder(NGInlineNode, NGWritingMode);

  NGTextFragmentBuilder& SetSize(const NGLogicalSize&);

  NGTextFragmentBuilder& SetShapeResult(RefPtr<const ShapeResult>);

  // Creates the fragment. Can only be called once.
  RefPtr<NGPhysicalTextFragment> ToTextFragment(unsigned index,
                                                unsigned start_offset,
                                                unsigned end_offset);

 private:
  NGInlineNode node_;

  NGLogicalSize size_;

  RefPtr<const ShapeResult> shape_result_;

};

}  // namespace blink

#endif  // NGTextFragmentBuilder
