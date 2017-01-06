// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalTextFragment_h
#define NGPhysicalTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT NGPhysicalTextFragment final : public NGPhysicalFragment {
 public:
  NGPhysicalTextFragment(
      const NGInlineNode* node,
      unsigned start_index,
      unsigned end_index,
      NGPhysicalSize size,
      NGPhysicalSize overflow,
      HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
      Vector<NGStaticPosition> out_of_flow_positions)
      : NGPhysicalFragment(size,
                           overflow,
                           kFragmentText,
                           out_of_flow_descendants,
                           out_of_flow_positions),
        node_(node),
        start_index_(start_index),
        end_index_(end_index) {}

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    visitor->trace(node_);
    NGPhysicalFragment::traceAfterDispatch(visitor);
  }

  const NGInlineNode* Node() const { return node_; }

  // The range of NGLayoutInlineItem.
  // |StartIndex| shows the lower logical index, so the visual order iteration
  // for RTL should be done from |EndIndex - 1| to |StartIndex|.
  unsigned StartIndex() const { return start_index_; }
  unsigned EndIndex() const { return end_index_; }

 private:
  // TODO(kojii): NGInlineNode is to access text content and NGLayoutInlineItem.
  // Review if it's better to point them.
  Member<const NGInlineNode> node_;
  unsigned start_index_;
  unsigned end_index_;
};

DEFINE_TYPE_CASTS(NGPhysicalTextFragment,
                  NGPhysicalFragment,
                  text,
                  text->Type() == NGPhysicalFragment::kFragmentText,
                  text.Type() == NGPhysicalFragment::kFragmentText);

}  // namespace blink

#endif  // NGPhysicalTextFragment_h
