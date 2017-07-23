// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBreaker_h
#define NGLineBreaker_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class NGInlineBreakToken;
class NGInlineItem;
class NGInlineNode;
class NGFragmentBuilder;

// Represents a line breaker.
//
// This class measures each NGInlineItem and determines items to form a line,
// so that NGInlineLayoutAlgorithm can build a line box from the output.
class CORE_EXPORT NGLineBreaker {
 public:
  NGLineBreaker(NGInlineNode,
                NGConstraintSpace*,
                NGFragmentBuilder*,
                Vector<RefPtr<NGUnpositionedFloat>>*,
                const NGInlineBreakToken* = nullptr);
  ~NGLineBreaker() {}
  STACK_ALLOCATED();

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  bool NextLine(NGLineInfo*, const NGLogicalOffset&);

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  RefPtr<NGInlineBreakToken> CreateBreakToken() const;

 private:
  // This struct holds information for the current line.
  struct LineData {
    STACK_ALLOCATED();

    // The current position from inline_start. Unlike NGInlineLayoutAlgorithm
    // that computes position in visual order, this position in logical order.
    LayoutUnit position;

    // The current opportunity.
    WTF::Optional<NGLayoutOpportunity> opportunity;

    // We don't create "certain zero-height line boxes".
    // https://drafts.csswg.org/css2/visuren.html#phantom-line-box
    // Such line boxes do not prevent two margins being "adjoining", and thus
    // collapsing.
    // https://drafts.csswg.org/css2/box.html#collapsing-margins
    bool should_create_line_box = false;

    // Set when the line ended with a forced break. Used to setup the states for
    // the next line.
    bool is_after_forced_break = false;

    bool HasAvailableWidth() const { return opportunity.has_value(); }
    LayoutUnit AvailableWidth() const { return opportunity->InlineSize(); }
    bool CanFit() const { return position <= AvailableWidth(); }
    bool CanFit(LayoutUnit extra) const {
      return position + extra <= AvailableWidth();
    }
  };

  void BreakLine(NGLineInfo*);

  void PrepareNextLine(NGLineInfo*);

  void UpdateAvailableWidth();

  void ComputeLineLocation(NGLineInfo*) const;

  enum class LineBreakState {
    // The current position is not breakable.
    kNotBreakable,
    // The current position is breakable.
    kIsBreakable,
    // Break by including trailing items (CloseTag).
    kBreakAfterTrailings,
    // Break immediately.
    kForcedBreak
  };

  LineBreakState HandleText(const NGInlineItemResults&,
                            const NGInlineItem&,
                            NGInlineItemResult*);
  void BreakText(NGInlineItemResult*,
                 const NGInlineItem&,
                 LayoutUnit available_width);

  LineBreakState HandleControlItem(const NGInlineItem&, NGInlineItemResult*);
  LineBreakState HandleAtomicInline(const NGInlineItem&,
                                    NGInlineItemResult*,
                                    const NGLineInfo&);
  void HandleFloat(const NGInlineItem&,
                   NGInlineItemResults*);

  void HandleOpenTag(const NGInlineItem&, NGInlineItemResult*);
  void HandleCloseTag(const NGInlineItem&, NGInlineItemResult*);

  void HandleOverflow(NGLineInfo*);
  void Rewind(NGLineInfo*, unsigned new_end);

  void SetCurrentStyle(const ComputedStyle&);
  bool IsFirstBreakOpportunity(unsigned, const NGInlineItemResults&) const;

  void MoveToNextOf(const NGInlineItem&);
  void MoveToNextOf(const NGInlineItemResult&);
  void SkipCollapsibleWhitespaces();

  bool IsFirstFormattedLine() const;

  LineData line_;
  NGInlineNode node_;
  NGConstraintSpace* constraint_space_;
  NGFragmentBuilder* container_builder_;
  Vector<RefPtr<NGUnpositionedFloat>>* unpositioned_floats_;
  unsigned item_index_ = 0;
  unsigned offset_ = 0;
  NGLogicalOffset content_offset_;
  LazyLineBreakIterator break_iterator_;
  HarfBuzzShaper shaper_;
  ShapeResultSpacing<String> spacing_;

  // True when current box allows line wrapping.
  bool auto_wrap_ = false;

  // True when current box has 'word-break/word-wrap: break-word'.
  bool break_if_overflow_ = false;
};

}  // namespace blink

#endif  // NGLineBreaker_h
