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
                const NGInlineBreakToken* = nullptr);
  ~NGLineBreaker() {}
  STACK_ALLOCATED();

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  bool NextLine(NGLineInfo*, const NGLogicalOffset&);

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  RefPtr<NGInlineBreakToken> CreateBreakToken() const;

 private:
  void BreakLine(NGLineInfo*);

  bool HasAvailableWidth() const { return opportunity_.has_value(); }
  LayoutUnit AvailableWidth() const {
    return opportunity_.value().InlineSize();
  }
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

  LineBreakState HandleText(const NGInlineItem&,
                            NGInlineItemResult*);
  void BreakText(NGInlineItemResult*,
                 const NGInlineItem&,
                 LayoutUnit available_width);

  LineBreakState HandleControlItem(const NGInlineItem&, NGInlineItemResult*);
  LineBreakState HandleAtomicInline(const NGInlineItem&, NGInlineItemResult*);
  void HandleFloat(const NGInlineItem&,
                   NGInlineItemResults*);

  void HandleOpenTag(const NGInlineItem&, NGInlineItemResult*);
  void HandleCloseTag(const NGInlineItem&, NGInlineItemResult*);

  void HandleOverflow(NGLineInfo*);
  void Rewind(NGLineInfo*, unsigned new_end);

  void SetShouldCreateLineBox();

  void SetCurrentStyle(const ComputedStyle&);

  void MoveToNextOf(const NGInlineItem&);
  void MoveToNextOf(const NGInlineItemResult&);
  void SkipCollapsibleWhitespaces();

  NGInlineNode node_;
  NGConstraintSpace* constraint_space_;
  NGFragmentBuilder* container_builder_;
  const AtomicString locale_;
  unsigned item_index_;
  unsigned offset_;
  LayoutUnit position_;
  WTF::Optional<NGLayoutOpportunity> opportunity_;
  NGLogicalOffset content_offset_;
  LazyLineBreakIterator break_iterator_;
  HarfBuzzShaper shaper_;
  ShapeResultSpacing<String> spacing_;

  unsigned auto_wrap_ : 1;
  unsigned should_create_line_box_ : 1;
};

}  // namespace blink

#endif  // NGLineBreaker_h
