// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInlineItemsBuilder_h
#define NGLayoutInlineItemsBuilder_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGLayoutInlineItem;

// NGLayoutInlineItemsBuilder builds a string and a list of NGLayoutInlineItem
// from inlines.
//
// When appending, spaces are collapsed according to CSS Text, The white space
// processing rules
// https://drafts.csswg.org/css-text-3/#white-space-rules
//
// By calling EnterInline/ExitInline, it inserts bidirectional control
// characters as defined in:
// https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
class CORE_EXPORT NGLayoutInlineItemsBuilder {
  STACK_ALLOCATED();

 public:
  explicit NGLayoutInlineItemsBuilder(Vector<NGLayoutInlineItem>* items)
      : items_(items) {}
  ~NGLayoutInlineItemsBuilder();

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
  void Append(UChar, const ComputedStyle* = nullptr, LayoutObject* = nullptr);

  // Append a character.
  // The character is opaque to space collapsing that spaces before this
  // character and after this character can collapse as if this character does
  // not exist.
  void AppendAsOpaqueToSpaceCollapsing(UChar);
  void AppendBidiControl(const ComputedStyle*, UChar ltr, UChar rtl);

  void EnterBlock(const ComputedStyle*);
  void ExitBlock();
  void EnterInline(LayoutObject*);
  void ExitInline(LayoutObject*);

 private:
  Vector<NGLayoutInlineItem>* items_;
  StringBuilder text_;

  typedef struct OnExitNode {
    LayoutObject* node;
    UChar character;
  } OnExitNode;
  Vector<OnExitNode> exits_;

  bool is_last_collapsible_space_ = true;
  bool has_pending_newline_ = false;
  bool is_svgtext_ = false;
  bool has_bidi_controls_ = false;

  // Because newlines may be removed depends on following characters, newlines
  // at the end of input string is not added to |text_| but instead
  // |has_pending_newline_| flag is set.
  // This function determines whether to add the newline or ignore.
  void ProcessPendingNewline(const String&, const ComputedStyle*);

  // Removes the collapsible space at the end of |text_| if exists.
  void RemoveTrailingCollapsibleSpace(unsigned*);

  void Enter(LayoutObject*, UChar);
  void Exit(LayoutObject*);
};

}  // namespace blink

#endif  // NGLayoutInlineItemsBuilder_h
