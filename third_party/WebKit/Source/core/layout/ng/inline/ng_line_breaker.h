// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBreaker_h
#define NGLineBreaker_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class LazyLineBreakIterator;
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
  NGLineBreaker(NGInlineNode*,
                const NGConstraintSpace*,
                NGInlineBreakToken* = nullptr);
  ~NGLineBreaker() {}
  STACK_ALLOCATED();

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  // TODO(kojii): NGInlineLayoutAlgorithm is needed because floats require
  // not only constraint space but also container builder. Consider refactor
  // not to require algorithm.
  void NextLine(NGInlineItemResults*, NGInlineLayoutAlgorithm*);

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  RefPtr<NGInlineBreakToken> CreateBreakToken() const;

 private:
  void BreakLine(NGInlineItemResults*, NGInlineLayoutAlgorithm*);

  bool HandleControlItem(const NGInlineItem&,
                         const String& text,
                         NGInlineItemResult*,
                         LayoutUnit position);
  void LayoutAtomicInline(const NGInlineItem&, NGInlineItemResult*);

  void HandleOverflow(NGInlineItemResults*, const LazyLineBreakIterator&);

  void MoveToNextOf(const NGInlineItem&);
  void SkipCollapsibleWhitespaces();

  void AppendCloseTags(NGInlineItemResults*);

  Persistent<NGInlineNode> node_;
  const NGConstraintSpace* constraint_space_;
  const AtomicString locale_;
  unsigned item_index_;
  unsigned offset_;
};

}  // namespace blink

#endif  // NGLineBreaker_h
