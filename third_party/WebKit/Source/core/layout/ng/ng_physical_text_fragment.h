// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalTextFragment_h
#define NGPhysicalTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT NGPhysicalTextFragment final : public NGPhysicalFragment {
 public:
  NGPhysicalTextFragment(LayoutObject* layout_object,
                         const NGInlineNode* node,
                         unsigned item_index,
                         unsigned start_offset,
                         unsigned end_offset,
                         NGPhysicalSize size,
                         NGPhysicalSize overflow)
      : NGPhysicalFragment(layout_object, size, overflow, kFragmentText),
        node_(node),
        item_index_(item_index),
        start_offset_(start_offset),
        end_offset_(end_offset) {}

  const NGInlineNode* Node() const { return node_; }

  // The range of NGLayoutInlineItem.
  unsigned ItemIndex() const { return item_index_; }
  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }

 private:
  // TODO(kojii): NGInlineNode is to access text content and NGLayoutInlineItem.
  // Review if it's better to point them.
  Persistent<const NGInlineNode> node_;
  unsigned item_index_;
  unsigned start_offset_;
  unsigned end_offset_;
};

DEFINE_TYPE_CASTS(NGPhysicalTextFragment,
                  NGPhysicalFragment,
                  text,
                  text->Type() == NGPhysicalFragment::kFragmentText,
                  text.Type() == NGPhysicalFragment::kFragmentText);

}  // namespace blink

#endif  // NGPhysicalTextFragment_h
