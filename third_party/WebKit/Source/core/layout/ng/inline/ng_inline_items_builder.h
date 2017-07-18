// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineItemsBuilder_h
#define NGInlineItemsBuilder_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/empty_offset_mapping_builder.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_offset_mapping_builder.h"
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
//
// NGInlineItemsBuilder may optionally take an NGOffsetMappingBuilder template
// parameter to construct the white-space collapsed offset mapping, which maps
// offsets in the concatenation of all appended strings and characters to
// offsets in |text_|.
// See https://goo.gl/CJbxky for more details about offset mapping.
template <typename OffsetMappingBuilder>
class CORE_TEMPLATE_CLASS_EXPORT NGInlineItemsBuilderTemplate {
  STACK_ALLOCATED();

 public:
  explicit NGInlineItemsBuilderTemplate(Vector<NGInlineItem>* items)
      : items_(items) {}
  ~NGInlineItemsBuilderTemplate();

  String ToString();

  void SetIsSVGText(bool value) { is_svgtext_ = value; }

  // Returns whether the items contain any Bidi controls.
  bool HasBidiControls() const { return has_bidi_controls_; }

  // Returns if the inline node has no content. For example:
  // <span></span> or <span><float></float></span>.
  bool IsEmptyInline() const { return is_empty_inline_; }

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

  OffsetMappingBuilder& GetOffsetMappingBuilder() { return mapping_builder_; }
  OffsetMappingBuilder& GetConcatenatedOffsetMappingBuilder() {
    return concatenated_mapping_builder_;
  }

 private:
  Vector<NGInlineItem>* items_;
  StringBuilder text_;

  // TODO(xiaochengh): Rename |mapping_builder_| to |collapsed_mapping_builder_|
  // |collapsed_mapping_builder_| builds the whitespace-collapsed offset mapping
  // during inline collection. It is updated whenever |text_| is modified or a
  // white space is collapsed.
  OffsetMappingBuilder mapping_builder_;

  // |concatenated_mapping_builder_| builds the concatenated offset mapping
  // during inline collection. It is updated whenever a non-text character is
  // appended to |text_|. User of NGInlineItemsBuilder should also update
  // |concatenated_mapping_builder_| whenever collecting a text node during
  // inline collection, by appending the text-transformed offset mapping of the
  // text node to |concatenated_mapping_builder_|.
  OffsetMappingBuilder concatenated_mapping_builder_;

  // Indicates whether we are appending a string not, to help updating
  // |concatenated_mapping_builder_|.
  bool is_appending_string_ = false;

  typedef struct OnExitNode {
    LayoutObject* node;
    UChar character;
  } OnExitNode;
  Vector<OnExitNode> exits_;

  enum class CollapsibleSpace { kNone, kSpace, kNewline };

  CollapsibleSpace last_collapsible_space_ = CollapsibleSpace::kSpace;
  bool is_svgtext_ = false;
  bool has_bidi_controls_ = false;
  bool is_empty_inline_ = true;

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

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>;

using NGInlineItemsBuilder =
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
using NGInlineItemsBuilderForOffsetMapping =
    NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>;

}  // namespace blink

#endif  // NGInlineItemsBuilder_h
