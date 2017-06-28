// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineItemsBuilder_h
#define NGInlineItemsBuilder_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGInlineItem;

// NGInlineItemsBuilder builds a string and a list of NGInlineItem from inlines.
//
// When appending, spaces are collapsed according to CSS Text, The white space
// processing rules
// https://drafts.csswg.org/css-text-3/#white-space-rules
//
// By calling EnterInline/ExitInline, it inserts bidirectional control
// characters as defined in:
// https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
class CORE_EXPORT NGInlineItemsBuilder {
  STACK_ALLOCATED();

 public:
  explicit NGInlineItemsBuilder(Vector<NGInlineItem>* items) : items_(items) {}
  ~NGInlineItemsBuilder();

  String ToString();

  void SetIsSVGText(bool value) { is_svgtext_ = value; }

  // Returns whether the items contain any Bidi controls.
  bool HasBidiControls() const { return has_bidi_controls_; }

  // Append a string.
  // When appending, spaces are collapsed according to CSS Text, The white space
  // processing rules
  // https://drafts.csswg.org/css-text-3/#white-space-rules
  // @param style The style for the string.
  // If a nullptr, it should skip shaping. Atomic inlines and bidi controls use
  // this.
  // @param LayoutObject The LayoutObject for the string.
  // If a nullptr, it does not generate BidiRun. Bidi controls use this.
  void Append(const String&, const ComputedStyle*, LayoutObject* = nullptr);

  // Append a character.
  // Currently this function is for adding control characters such as
  // objectReplacementCharacter, and does not support all space collapsing logic
  // as its String version does.
  // See the String version for using nullptr for ComputedStyle and
  // LayoutObject.
  void Append(NGInlineItem::NGInlineItemType,
              UChar,
              const ComputedStyle* = nullptr,
              LayoutObject* = nullptr);

  // Append a character.
  // The character is opaque to space collapsing; i.e., spaces before this
  // character and after this character can collapse as if this character does
  // not exist.
  void AppendOpaque(NGInlineItem::NGInlineItemType, UChar);

  // Append a non-character item that is opaque to space collapsing.
  void AppendOpaque(NGInlineItem::NGInlineItemType,
                    const ComputedStyle* = nullptr,
                    LayoutObject* = nullptr);

  // Append a Bidi control character, for LTR or RTL depends on the style.
  void AppendBidiControl(const ComputedStyle*, UChar ltr, UChar rtl);

  void EnterBlock(const ComputedStyle*);
  void ExitBlock();
  void EnterInline(LayoutObject*);
  void ExitInline(LayoutObject*);

 private:
  Vector<NGInlineItem>* items_;
  StringBuilder text_;

  typedef struct OnExitNode {
    LayoutObject* node;
    UChar character;
  } OnExitNode;
  Vector<OnExitNode> exits_;

  enum class CollapsibleSpace { kNone, kSpace, kNewline };

  CollapsibleSpace last_collapsible_space_ = CollapsibleSpace::kSpace;
  bool is_svgtext_ = false;
  bool has_bidi_controls_ = false;

  void AppendWithWhiteSpaceCollapsing(const String&,
                                      unsigned start,
                                      unsigned end,
                                      const ComputedStyle*,
                                      LayoutObject*);
  void AppendWithoutWhiteSpaceCollapsing(const String&,
                                         const ComputedStyle*,
                                         LayoutObject*);
  void AppendWithPreservingNewlines(const String&,
                                    const ComputedStyle*,
                                    LayoutObject*);

  void AppendForcedBreak(const ComputedStyle*, LayoutObject*);

  void RemoveTrailingCollapsibleSpaceIfExists();
  void RemoveTrailingCollapsibleSpace(unsigned);
  void RemoveTrailingCollapsibleNewlineIfNeeded(const String&,
                                                unsigned,
                                                const ComputedStyle*);

  void Enter(LayoutObject*, UChar);
  void Exit(LayoutObject*);
};

}  // namespace blink

#endif  // NGInlineItemsBuilder_h
