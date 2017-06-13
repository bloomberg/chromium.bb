// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBreaker_h
#define NGLineBreaker_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class NGInlineBreakToken;
class NGInlineItem;
class NGInlineNode;
class NGInlineLayoutAlgorithm;

// Represents a line breaker.
//
// This class measures each NGInlineItem and determines items to form a line,
// so that NGInlineLayoutAlgorithm can build a line box from the output.
class CORE_EXPORT NGLineBreaker {
 public:
  NGLineBreaker(NGInlineNode,
                const NGConstraintSpace*,
                NGInlineBreakToken* = nullptr);
  ~NGLineBreaker() {}
  STACK_ALLOCATED();

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  // TODO(kojii): NGInlineLayoutAlgorithm is needed because floats require
  // not only constraint space but also container builder. Consider refactor
  // not to require algorithm.
  bool NextLine(NGLineInfo*, NGInlineLayoutAlgorithm*);

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  RefPtr<NGInlineBreakToken> CreateBreakToken() const;

 private:
  void BreakLine(NGLineInfo*, NGInlineLayoutAlgorithm*);

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

  LineBreakState HandleText(const NGInlineItem&, NGInlineItemResult*);
  void BreakText(NGInlineItemResult*,
                 const NGInlineItem&,
                 LayoutUnit available_width);

  LineBreakState HandleControlItem(const NGInlineItem&, NGInlineItemResult*);
  LineBreakState HandleAtomicInline(const NGInlineItem&, NGInlineItemResult*);
  void HandleFloat(const NGInlineItem&,
                   NGInlineItemResults*,
                   NGInlineLayoutAlgorithm*);

  void HandleOpenTag(const NGInlineItem&, NGInlineItemResult*);
  void HandleCloseTag(const NGInlineItem&, NGInlineItemResult*);

  void HandleOverflow(NGLineInfo*);
  void Rewind(NGLineInfo*, unsigned new_end);

  void UpdateBreakIterator(const ComputedStyle&);

  void MoveToNextOf(const NGInlineItem&);
  void MoveToNextOf(const NGInlineItemResult&);
  void SkipCollapsibleWhitespaces();

  NGInlineNode node_;
  const NGConstraintSpace* constraint_space_;
  const AtomicString locale_;
  unsigned item_index_;
  unsigned offset_;
  LayoutUnit available_width_;
  LayoutUnit position_;
  LazyLineBreakIterator break_iterator_;
  HarfBuzzShaper shaper_;

  unsigned auto_wrap_ : 1;
};

}  // namespace blink

#endif  // NGLineBreaker_h
