// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_TRUNCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_TRUNCATOR_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class NGInlineLayoutStateStack;
class NGLineInfo;

// A class to truncate lines and place ellipsis, invoked by the CSS
// 'text-overflow: ellipsis' property.
// https://drafts.csswg.org/css-ui/#overflow-ellipsis
class CORE_EXPORT NGLineTruncator final {
  STACK_ALLOCATED();

 public:
  NGLineTruncator(const NGLineInfo& line_info);

  // Truncate |line_box| and place ellipsis. Returns the new inline-size of the
  // |line_box|.
  //
  // |line_box| should be after bidi reorder, but before box fragments are
  // created.
  LayoutUnit TruncateLine(LayoutUnit line_width,
                          NGLineBoxFragmentBuilder::ChildList* line_box,
                          NGInlineLayoutStateStack* box_states);

  LayoutUnit TruncateLineInTheMiddle(
      LayoutUnit line_width,
      NGLineBoxFragmentBuilder::ChildList* line_box,
      NGInlineLayoutStateStack* box_states);

 private:
  const ComputedStyle& EllipsisStyle() const;

  // Initialize four ellipsis_*_ data members.
  void SetupEllipsis();

  // Add a child for ellipsis next to |ellipsized_child|.
  LayoutUnit PlaceEllipsisNextTo(
      NGLineBoxFragmentBuilder::ChildList* line_box,
      NGLineBoxFragmentBuilder::Child* ellipsized_child);

  static constexpr wtf_size_t kDidNotAddChild = WTF::kNotFound;
  // Add a child with truncated text of (*line_box)[source_index].
  // This function returns the index of the new child.
  // If the truncated text is empty, kDidNotAddChild is returned.
  //
  // |leave_one_character| - Force to leave at least one character regardless of
  //                         |position|.
  // |position| and |edge| - Indicate truncation point and direction.
  //                         If |edge| is TextDirection::kLtr, the left side of
  //                         |position| will be copied to the new child.
  //                         Otherwise, the right side of |position| will be
  //                         copied.
  wtf_size_t AddTruncatedChild(wtf_size_t source_index,
                               bool leave_one_character,
                               LayoutUnit position,
                               TextDirection edge,
                               NGLineBoxFragmentBuilder::ChildList* line_box,
                               NGInlineLayoutStateStack* box_states);
  bool EllipsizeChild(
      LayoutUnit line_width,
      LayoutUnit ellipsis_width,
      bool is_first_child,
      NGLineBoxFragmentBuilder::Child*,
      scoped_refptr<const NGPhysicalTextFragment>* truncated_fragment);
  bool TruncateChild(
      LayoutUnit space_for_this_child,
      bool is_first_child,
      const NGLineBoxFragmentBuilder::Child& child,
      scoped_refptr<const NGPhysicalTextFragment>* truncated_fragment);
  void HideChild(NGLineBoxFragmentBuilder::Child* child);

  scoped_refptr<const ComputedStyle> line_style_;
  LayoutUnit available_width_;
  TextDirection line_direction_;

  // The following 3 data members are available after SetupEllipsis().
  const SimpleFontData* ellipsis_font_data_;
  String ellipsis_text_;
  LayoutUnit ellipsis_width_;

  // This data member is available between SetupEllipsis() and
  // PlaceEllipsisNextTo().
  scoped_refptr<ShapeResultView> ellipsis_shape_result_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_TRUNCATOR_H_
