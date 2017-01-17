// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBuilder_h
#define NGLineBuilder_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class NGConstraintSpace;
class NGFragment;
class NGFragmentBuilder;
class NGInlineNode;

// NGLineBuilder creates the fragment tree for a line.
class CORE_EXPORT NGLineBuilder final
    : public GarbageCollectedFinalized<NGLineBuilder> {
 public:
  NGLineBuilder(NGInlineNode*, const NGConstraintSpace*);

  // Add a range of NGLayoutInlineItem to the line in the logical order.
  void Add(unsigned start_index, unsigned end_index, LayoutUnit inline_size);

  // Create a line from items added by |Add()|.
  void CreateLine();

  // Create fragments for lines created by |CreateLine()|.
  void CreateFragments(NGFragmentBuilder*);

  // Copy fragment data of all lines created by this NGLineBuilder to
  // LayoutBlockFlow.
  // This must run after |CreateFragments()|, and after the fragments it created
  // are placed.
  void CopyFragmentDataToLayoutBlockFlow();

  DECLARE_VIRTUAL_TRACE();

 private:
  void BidiReorder();

  struct LineItemChunk {
    unsigned start_index;
    unsigned end_index;
    LayoutUnit inline_size;
  };

  // LineBoxData is a set of data for a line box that are computed in early
  // phases, such as in |CreateLine()|, and will be used in later phases.
  // TODO(kojii): Not sure if all these data are needed in fragment tree. If
  // they are, we can create a linebox fragment, store them there, and this
  // isn't needed. For now, we're trying to minimize data in fragments.
  struct LineBoxData {
    unsigned fragment_end;
    LayoutUnit inline_size;
  };

  Member<NGInlineNode> inline_box_;
  Member<const NGConstraintSpace> constraint_space_;
  HeapVector<Member<NGFragment>, 32> fragments_;
  Vector<NGLogicalOffset, 32> offsets_;
  Vector<LineItemChunk, 32> line_item_chunks_;
  Vector<LineBoxData, 32> line_box_data_list_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;

#if DCHECK_IS_ON()
  unsigned is_bidi_reordered_ : 1;
#endif
};

}  // namespace blink

#endif  // NGLineBuilder_h
