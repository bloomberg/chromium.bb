// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBuilder_h
#define NGLineBuilder_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class ComputedStyle;
class FontMetrics;
class NGConstraintSpace;
class NGFragmentBuilder;
class NGInlineNode;
class NGLayoutInlineItem;

// NGLineBuilder creates the fragment tree for a line.
// NGLineBuilder manages the current line as a range, |start| and |end|.
// |end| can be extended multiple times before creating a line, usually until
// |!CanFitOnLine()|.
// |SetBreakOpportunity| can mark the last confirmed offset that can fit.
class CORE_EXPORT NGLineBuilder final {
  STACK_ALLOCATED();

 public:
  NGLineBuilder(NGInlineNode*, const NGConstraintSpace*);

  const NGConstraintSpace& ConstraintSpace() const {
    return *constraint_space_;
  }

  LayoutUnit MaxInlineSize() const { return max_inline_size_; }

  // Returns if the current items fit on a line.
  bool CanFitOnLine() const;

  // Returns if there were any items.
  bool HasItems() const;

  // Set the start offset.
  // Set the end as well, and therefore empties the current line.
  void SetStart(unsigned index, unsigned offset);

  // Set the end offset.
  void SetEnd(unsigned end_offset);

  // Set the end offset, if caller knows the index and inline size since the
  // current end offset.
  void SetEnd(unsigned last_index,
              unsigned end_offset,
              LayoutUnit inline_size_since_current_end);

  // Create a line up to the end offset.
  // Then set the start to the end offset, and thus empty the current line.
  void CreateLine();

  // Returns if a break opportunity was set on the current line.
  bool HasBreakOpportunity() const;

  // Returns if there were items after the last break opportunity.
  bool HasItemsAfterLastBreakOpportunity() const;

  // Set the break opportunity at the current end offset.
  void SetBreakOpportunity();

  // Create a line up to the last break opportunity.
  // Items after that are sent to the next line.
  void CreateLineUpToLastBreakOpportunity();

  // Set the start offset of hangables; e.g., spaces or hanging punctuations.
  // Hangable characters can go beyond the right margin, and are ignored for
  // center/right alignment.
  void SetStartOfHangables(unsigned offset);

  // Create fragments for all lines created so far.
  void CreateFragments(NGFragmentBuilder*);

  // Copy fragment data of all lines created by this NGLineBuilder to
  // LayoutBlockFlow.
  // This must run after |CreateFragments()|, and after the fragments it created
  // are placed.
  void CopyFragmentDataToLayoutBlockFlow();

 private:
  struct LineItemChunk {
    unsigned index;
    unsigned start_offset;
    unsigned end_offset;
    LayoutUnit inline_size;
  };

  void BidiReorder(Vector<LineItemChunk, 32>*);

  // Represents block-direction metrics for an |NGLayoutInlineItem|.
  struct InlineItemMetrics {
    float ascent;
    float descent;
    float ascent_and_leading;
    float descent_and_leading;

    // Use the leading from the 'line-height' property, or the font metrics of
    // the primary font if 'line-height: normal'.
    InlineItemMetrics(const ComputedStyle&, FontBaseline);

    // Use the leading from the font metrics.
    InlineItemMetrics(const FontMetrics&, FontBaseline);

   private:
    void Initialize(const FontMetrics&, FontBaseline, float line_height);
  };

  // LineBoxData is a set of data for a line box that are computed in early
  // phases, such as in |CreateLine()|, and will be used in later phases.
  // TODO(kojii): Not sure if all these data are needed in fragment tree. If
  // they are, we can create a linebox fragment, store them there, and this
  // isn't needed. For now, we're trying to minimize data in fragments.
  struct LineBoxData {
    unsigned fragment_end;
    LayoutUnit inline_size;
    LayoutUnit top_with_leading;
    float max_ascent = 0;
    float max_descent = 0;
    float max_ascent_and_leading = 0;
    float max_descent_and_leading = 0;

    // Include |InlineItemMetrics| into the metrics for this line box.
    void UpdateMaxAscentAndDescent(const InlineItemMetrics&);
  };

  void PlaceItems(const Vector<LineItemChunk, 32>&);
  void AccumulateUsedFonts(const NGLayoutInlineItem&,
                           const LineItemChunk&,
                           LineBoxData*);

  Persistent<NGInlineNode> inline_box_;
  const NGConstraintSpace* constraint_space_;  // Not owned as STACK_ALLOCATED.
  Vector<RefPtr<NGPhysicalFragment>, 32> fragments_;
  Vector<NGLogicalOffset, 32> offsets_;
  Vector<LineBoxData, 32> line_box_data_list_;
  unsigned start_index_ = 0;
  unsigned start_offset_ = 0;
  unsigned last_index_ = 0;
  unsigned end_offset_ = 0;
  unsigned last_break_opportunity_index_ = 0;
  unsigned last_break_opportunity_offset_ = 0;
  LayoutUnit end_position_;
  LayoutUnit last_break_opportunity_position_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  FontBaseline baseline_type_;

#if DCHECK_IS_ON()
  unsigned is_bidi_reordered_ : 1;
#endif
};

}  // namespace blink

#endif  // NGLineBuilder_h
