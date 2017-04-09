/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/style/StyleRareInheritedData.h"

#include "core/style/AppliedTextDecoration.h"
#include "core/style/CursorData.h"
#include "core/style/DataEquivalency.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleImage.h"
#include "core/style/StyleInheritedVariables.h"

namespace blink {

struct SameSizeAsStyleRareInheritedData
    : public RefCounted<SameSizeAsStyleRareInheritedData> {
  void* style_image;
  Color first_color;
  float first_float;
  Color colors[7];
  void* own_ptrs[1];
  AtomicString atomic_strings[3];
  void* ref_ptrs[1];
  Persistent<void*> persistent_handles[2];
  Length lengths[1];
  float second_float;
  unsigned bitfields_[2];
  short paged_media_shorts[2];
  short hyphenation_shorts[3];
  uint8_t line_height_step;

  Color touch_colors;
  TabSize tab_size;
  void* variables[1];
  TextSizeAdjust text_size_adjust;
};

static_assert(sizeof(StyleRareInheritedData) <=
                  sizeof(SameSizeAsStyleRareInheritedData),
              "StyleRareInheritedData should stay small");

StyleRareInheritedData::StyleRareInheritedData()
    : list_style_image(ComputedStyle::InitialListStyleImage()),
      text_stroke_width(ComputedStyle::InitialTextStrokeWidth()),
      indent(ComputedStyle::InitialTextIndent()),
      effective_zoom_(ComputedStyle::InitialZoom()),
      widows(ComputedStyle::InitialWidows()),
      orphans(ComputedStyle::InitialOrphans()),
      text_stroke_color_is_current_color_(true),
      text_fill_color_is_current_color_(true),
      text_emphasis_color_is_current_color_(true),
      caret_color_is_current_color_(false),
      caret_color_is_auto_(true),
      visited_link_text_stroke_color_is_current_color_(true),
      visited_link_text_fill_color_is_current_color_(true),
      visited_link_text_emphasis_color_is_current_color_(true),
      visited_link_caret_color_is_current_color_(false),
      visited_link_caret_color_is_auto_(true),
      text_security(ComputedStyle::InitialTextSecurity()),
      user_modify(READ_ONLY),
      word_break(ComputedStyle::InitialWordBreak()),
      overflow_wrap(ComputedStyle::InitialOverflowWrap()),
      line_break(kLineBreakAuto),
      user_select(ComputedStyle::InitialUserSelect()),
      speak(kSpeakNormal),
      hyphens(kHyphensManual),
      text_emphasis_fill(kTextEmphasisFillFilled),
      text_emphasis_mark(kTextEmphasisMarkNone),
      text_emphasis_position(kTextEmphasisPositionOver),
      text_align_last_(ComputedStyle::InitialTextAlignLast()),
      text_justify_(ComputedStyle::InitialTextJustify()),
      text_orientation_(kTextOrientationMixed),
      text_combine_(ComputedStyle::InitialTextCombine()),
      text_indent_line_(ComputedStyle::InitialTextIndentLine()),
      text_indent_type_(ComputedStyle::InitialTextIndentLine()),
      image_rendering_(ComputedStyle::InitialImageRendering()),
      text_underline_position_(ComputedStyle::InitialTextUnderlinePosition()),
      text_decoration_skip_(ComputedStyle::InitialTextDecorationSkip()),
      ruby_position_(ComputedStyle::InitialRubyPosition()),
      subtree_will_change_contents_(false),
      self_or_ancestor_has_dir_auto_attribute_(false),
      respect_image_orientation_(false),
      hyphenation_limit_before(-1),
      hyphenation_limit_after(-1),
      hyphenation_limit_lines(-1),
      line_height_step_(0),
      tap_highlight_color(ComputedStyle::InitialTapHighlightColor()),
      tab_size_(ComputedStyle::InitialTabSize()),
      text_size_adjust_(ComputedStyle::InitialTextSizeAdjust()) {}

StyleRareInheritedData::StyleRareInheritedData(const StyleRareInheritedData& o)
    : RefCounted<StyleRareInheritedData>(),
      list_style_image(o.list_style_image),
      text_stroke_color_(o.text_stroke_color_),
      text_stroke_width(o.text_stroke_width),
      text_fill_color_(o.text_fill_color_),
      text_emphasis_color_(o.text_emphasis_color_),
      caret_color_(o.caret_color_),
      visited_link_text_stroke_color_(o.visited_link_text_stroke_color_),
      visited_link_text_fill_color_(o.visited_link_text_fill_color_),
      visited_link_text_emphasis_color_(o.visited_link_text_emphasis_color_),
      visited_link_caret_color_(o.visited_link_caret_color_),
      text_shadow(o.text_shadow),
      highlight(o.highlight),
      cursor_data(o.cursor_data),
      indent(o.indent),
      effective_zoom_(o.effective_zoom_),
      widows(o.widows),
      orphans(o.orphans),
      text_stroke_color_is_current_color_(
          o.text_stroke_color_is_current_color_),
      text_fill_color_is_current_color_(o.text_fill_color_is_current_color_),
      text_emphasis_color_is_current_color_(
          o.text_emphasis_color_is_current_color_),
      caret_color_is_current_color_(o.caret_color_is_current_color_),
      caret_color_is_auto_(o.caret_color_is_auto_),
      visited_link_text_stroke_color_is_current_color_(
          o.visited_link_text_stroke_color_is_current_color_),
      visited_link_text_fill_color_is_current_color_(
          o.visited_link_text_fill_color_is_current_color_),
      visited_link_text_emphasis_color_is_current_color_(
          o.visited_link_text_emphasis_color_is_current_color_),
      visited_link_caret_color_is_current_color_(
          o.visited_link_caret_color_is_current_color_),
      visited_link_caret_color_is_auto_(o.visited_link_caret_color_is_auto_),
      text_security(o.text_security),
      user_modify(o.user_modify),
      word_break(o.word_break),
      overflow_wrap(o.overflow_wrap),
      line_break(o.line_break),
      user_select(o.user_select),
      speak(o.speak),
      hyphens(o.hyphens),
      text_emphasis_fill(o.text_emphasis_fill),
      text_emphasis_mark(o.text_emphasis_mark),
      text_emphasis_position(o.text_emphasis_position),
      text_align_last_(o.text_align_last_),
      text_justify_(o.text_justify_),
      text_orientation_(o.text_orientation_),
      text_combine_(o.text_combine_),
      text_indent_line_(o.text_indent_line_),
      text_indent_type_(o.text_indent_type_),
      image_rendering_(o.image_rendering_),
      text_underline_position_(o.text_underline_position_),
      text_decoration_skip_(o.text_decoration_skip_),
      ruby_position_(o.ruby_position_),
      subtree_will_change_contents_(o.subtree_will_change_contents_),
      self_or_ancestor_has_dir_auto_attribute_(
          o.self_or_ancestor_has_dir_auto_attribute_),
      respect_image_orientation_(o.respect_image_orientation_),
      hyphenation_string(o.hyphenation_string),
      hyphenation_limit_before(o.hyphenation_limit_before),
      hyphenation_limit_after(o.hyphenation_limit_after),
      hyphenation_limit_lines(o.hyphenation_limit_lines),
      line_height_step_(o.line_height_step_),
      text_emphasis_custom_mark(o.text_emphasis_custom_mark),
      tap_highlight_color(o.tap_highlight_color),
      applied_text_decorations(o.applied_text_decorations),
      tab_size_(o.tab_size_),
      variables(o.variables),
      text_size_adjust_(o.text_size_adjust_) {}

StyleRareInheritedData::~StyleRareInheritedData() {}

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const {
  return text_stroke_color_ == o.text_stroke_color_ &&
         text_stroke_width == o.text_stroke_width &&
         text_fill_color_ == o.text_fill_color_ &&
         text_emphasis_color_ == o.text_emphasis_color_ &&
         caret_color_ == o.caret_color_ &&
         visited_link_text_stroke_color_ == o.visited_link_text_stroke_color_ &&
         visited_link_text_fill_color_ == o.visited_link_text_fill_color_ &&
         visited_link_text_emphasis_color_ ==
             o.visited_link_text_emphasis_color_ &&
         visited_link_caret_color_ == o.visited_link_caret_color_ &&
         tap_highlight_color == o.tap_highlight_color &&
         ShadowDataEquivalent(o) && highlight == o.highlight &&
         DataEquivalent(cursor_data.Get(), o.cursor_data.Get()) &&
         indent == o.indent && effective_zoom_ == o.effective_zoom_ &&
         widows == o.widows && orphans == o.orphans &&
         text_stroke_color_is_current_color_ ==
             o.text_stroke_color_is_current_color_ &&
         text_fill_color_is_current_color_ ==
             o.text_fill_color_is_current_color_ &&
         text_emphasis_color_is_current_color_ ==
             o.text_emphasis_color_is_current_color_ &&
         caret_color_is_current_color_ == o.caret_color_is_current_color_ &&
         caret_color_is_auto_ == o.caret_color_is_auto_ &&
         visited_link_text_stroke_color_is_current_color_ ==
             o.visited_link_text_stroke_color_is_current_color_ &&
         visited_link_text_fill_color_is_current_color_ ==
             o.visited_link_text_fill_color_is_current_color_ &&
         visited_link_text_emphasis_color_is_current_color_ ==
             o.visited_link_text_emphasis_color_is_current_color_ &&
         visited_link_caret_color_is_current_color_ ==
             o.visited_link_caret_color_is_current_color_ &&
         visited_link_caret_color_is_auto_ ==
             o.visited_link_caret_color_is_auto_ &&
         text_security == o.text_security && user_modify == o.user_modify &&
         word_break == o.word_break && overflow_wrap == o.overflow_wrap &&
         line_break == o.line_break && user_select == o.user_select &&
         speak == o.speak && hyphens == o.hyphens &&
         hyphenation_limit_before == o.hyphenation_limit_before &&
         hyphenation_limit_after == o.hyphenation_limit_after &&
         hyphenation_limit_lines == o.hyphenation_limit_lines &&
         text_emphasis_fill == o.text_emphasis_fill &&
         text_emphasis_mark == o.text_emphasis_mark &&
         text_emphasis_position == o.text_emphasis_position &&
         text_align_last_ == o.text_align_last_ &&
         text_justify_ == o.text_justify_ &&
         text_orientation_ == o.text_orientation_ &&
         text_combine_ == o.text_combine_ &&
         text_indent_line_ == o.text_indent_line_ &&
         text_indent_type_ == o.text_indent_type_ &&
         subtree_will_change_contents_ == o.subtree_will_change_contents_ &&
         self_or_ancestor_has_dir_auto_attribute_ ==
             o.self_or_ancestor_has_dir_auto_attribute_ &&
         respect_image_orientation_ == o.respect_image_orientation_ &&
         hyphenation_string == o.hyphenation_string &&
         line_height_step_ == o.line_height_step_ &&
         text_emphasis_custom_mark == o.text_emphasis_custom_mark &&
         QuotesDataEquivalent(o) && tab_size_ == o.tab_size_ &&
         image_rendering_ == o.image_rendering_ &&
         text_underline_position_ == o.text_underline_position_ &&
         text_decoration_skip_ == o.text_decoration_skip_ &&
         ruby_position_ == o.ruby_position_ &&
         DataEquivalent(list_style_image.Get(), o.list_style_image.Get()) &&
         DataEquivalent(applied_text_decorations, o.applied_text_decorations) &&
         DataEquivalent(variables, o.variables) &&
         text_size_adjust_ == o.text_size_adjust_;
}

bool StyleRareInheritedData::ShadowDataEquivalent(
    const StyleRareInheritedData& o) const {
  return DataEquivalent(text_shadow.Get(), o.text_shadow.Get());
}

bool StyleRareInheritedData::QuotesDataEquivalent(
    const StyleRareInheritedData& o) const {
  return DataEquivalent(quotes, o.quotes);
}

}  // namespace blink
