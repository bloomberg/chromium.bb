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
#include "platform/text/TextBreakIterator.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Hyphenation;
class NGInlineBreakToken;
class NGInlineItem;
class NGFragmentBuilder;

// Represents a line breaker.
//
// This class measures each NGInlineItem and determines items to form a line,
// so that NGInlineLayoutAlgorithm can build a line box from the output.
class CORE_EXPORT NGLineBreaker {
  STACK_ALLOCATED();

 public:
  NGLineBreaker(NGInlineNode,
                const NGConstraintSpace&,
                NGFragmentBuilder*,
                Vector<scoped_refptr<NGUnpositionedFloat>>*,
                const NGInlineBreakToken* = nullptr);
  ~NGLineBreaker() {}

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  bool NextLine(const NGLogicalOffset& content_offset,
                const NGExclusionSpace&,
                NGLineInfo*);

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  scoped_refptr<NGInlineBreakToken> CreateBreakToken() const;

  NGExclusionSpace* ExclusionSpace() { return line_.exclusion_space.get(); }

 private:
  // This struct holds information for the current line.
  struct LineData {
    STACK_ALLOCATED();

    // The current position from inline_start. Unlike NGInlineLayoutAlgorithm
    // that computes position in visual order, this position in logical order.
    LayoutUnit position;

    // The current opportunity.
    WTF::Optional<NGLayoutOpportunity> opportunity;

    std::unique_ptr<NGExclusionSpace> exclusion_space;

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

  void PrepareNextLine(const NGExclusionSpace&, NGLineInfo*);

  bool HasFloatsAffectingCurrentLine() const;
  void FindNextLayoutOpportunity();
  void FindNextLayoutOpportunityWithMinimumInlineSize(LayoutUnit);

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

  LineBreakState HandleText(NGLineInfo*,
                            const NGInlineItem&,
                            NGInlineItemResult*);
  void BreakText(NGInlineItemResult*,
                 const NGInlineItem&,
                 LayoutUnit available_width,
                 NGLineInfo*);
  static void AppendHyphen(const ComputedStyle&, NGLineInfo*);

  LineBreakState HandleControlItem(const NGInlineItem&, NGInlineItemResult*);
  LineBreakState HandleAtomicInline(const NGInlineItem&,
                                    NGInlineItemResult*,
                                    const NGLineInfo&);
  LineBreakState HandleFloat(const NGInlineItem&, NGInlineItemResult*);

  void HandleOpenTag(const NGInlineItem&, NGInlineItemResult*);
  LineBreakState HandleCloseTag(const NGInlineItem&, NGInlineItemResults*);

  void HandleOverflow(NGLineInfo*);
  void HandleOverflow(NGLineInfo*,
                      LayoutUnit available_width,
                      bool force_break_anywhere);
  void Rewind(NGLineInfo*, unsigned new_end);

  void TruncateOverflowingText(NGLineInfo*);

  void SetCurrentStyle(const ComputedStyle&);
  bool IsFirstBreakOpportunity(unsigned, const NGLineInfo&) const;
  LineBreakState ComputeIsBreakableAfter(NGInlineItemResult*) const;

  void MoveToNextOf(const NGInlineItem&);
  void MoveToNextOf(const NGInlineItemResult&);
  void SkipCollapsibleWhitespaces();

  bool IsFirstFormattedLine() const;
  void ComputeBaseDirection();

  LineData line_;
  NGInlineNode node_;
  const NGConstraintSpace& constraint_space_;
  NGFragmentBuilder* container_builder_;
  Vector<scoped_refptr<NGUnpositionedFloat>>* unpositioned_floats_;
  unsigned item_index_ = 0;
  unsigned offset_ = 0;
  bool previous_line_had_forced_break_ = false;
  NGLogicalOffset content_offset_;
  LazyLineBreakIterator break_iterator_;
  HarfBuzzShaper shaper_;
  ShapeResultSpacing<String> spacing_;
  const Hyphenation* hyphenation_ = nullptr;

  // Keep track of handled float items. See HandleFloat().
  unsigned handled_floats_end_item_index_ = 0;

  // The current base direction for the bidi algorithm.
  // This is copied from NGInlineNode, then updated after each forced line break
  // if 'unicode-bidi: plaintext'.
  TextDirection base_direction_;

  // True when current box allows line wrapping.
  bool auto_wrap_ = false;

  // True when current box has 'word-break/word-wrap: break-word'.
  bool break_if_overflow_ = false;

  // True when breaking at soft hyphens (U+00AD) is allowed.
  bool enable_soft_hyphen_ = true;
};

}  // namespace blink

#endif  // NGLineBreaker_h
