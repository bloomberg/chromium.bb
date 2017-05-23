/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef StyleRareInheritedData_h
#define StyleRareInheritedData_h

#include "core/CoreExport.h"
#include "core/css/StyleAutoColor.h"
#include "core/css/StyleColor.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/AppliedTextDecorationList.h"
#include "core/style/CursorData.h"
#include "core/style/CursorList.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleImage.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/TextSizeAdjust.h"
#include "platform/Length.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/text/TabSize.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefVector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

// This struct is for rarely used inherited CSS3, CSS2, and WebKit-specific
// properties.  By grouping them together, we save space, and only allocate this
// object when someone actually uses one of these properties.
// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class CORE_EXPORT StyleRareInheritedData
    : public RefCountedCopyable<StyleRareInheritedData> {
 public:
  static PassRefPtr<StyleRareInheritedData> Create() {
    return AdoptRef(new StyleRareInheritedData);
  }
  PassRefPtr<StyleRareInheritedData> Copy() const {
    return AdoptRef(new StyleRareInheritedData(*this));
  }

  bool operator==(const StyleRareInheritedData& other) const {
    return text_stroke_color_ == other.text_stroke_color_ &&
           text_stroke_width_ == other.text_stroke_width_ &&
           text_fill_color_ == other.text_fill_color_ &&
           text_emphasis_color_ == other.text_emphasis_color_ &&
           caret_color_ == other.caret_color_ &&
           visited_link_text_stroke_color_ ==
               other.visited_link_text_stroke_color_ &&
           visited_link_text_fill_color_ ==
               other.visited_link_text_fill_color_ &&
           visited_link_text_emphasis_color_ ==
               other.visited_link_text_emphasis_color_ &&
           visited_link_caret_color_ == other.visited_link_caret_color_ &&
           tap_highlight_color_ == other.tap_highlight_color_ &&
           DataEquivalent(text_shadow_, other.text_shadow_) &&
           highlight_ == other.highlight_ &&
           DataEquivalent(cursor_data_, other.cursor_data_) &&
           text_indent_ == other.text_indent_ &&
           effective_zoom_ == other.effective_zoom_ &&
           widows_ == other.widows_ && orphans_ == other.orphans_ &&
           text_stroke_color_is_current_color_ ==
               other.text_stroke_color_is_current_color_ &&
           text_fill_color_is_current_color_ ==
               other.text_fill_color_is_current_color_ &&
           text_emphasis_color_is_current_color_ ==
               other.text_emphasis_color_is_current_color_ &&
           caret_color_is_current_color_ ==
               other.caret_color_is_current_color_ &&
           caret_color_is_auto_ == other.caret_color_is_auto_ &&
           visited_link_text_stroke_color_is_current_color_ ==
               other.visited_link_text_stroke_color_is_current_color_ &&
           visited_link_text_fill_color_is_current_color_ ==
               other.visited_link_text_fill_color_is_current_color_ &&
           visited_link_text_emphasis_color_is_current_color_ ==
               other.visited_link_text_emphasis_color_is_current_color_ &&
           visited_link_caret_color_is_current_color_ ==
               other.visited_link_caret_color_is_current_color_ &&
           visited_link_caret_color_is_auto_ ==
               other.visited_link_caret_color_is_auto_ &&
           text_security_ == other.text_security_ &&
           user_modify_ == other.user_modify_ &&
           word_break_ == other.word_break_ &&
           overflow_wrap_ == other.overflow_wrap_ &&
           line_break_ == other.line_break_ &&
           user_select_ == other.user_select_ && speak_ == other.speak_ &&
           hyphens_ == other.hyphens_ &&
           hyphenation_limit_before_ == other.hyphenation_limit_before_ &&
           hyphenation_limit_after_ == other.hyphenation_limit_after_ &&
           hyphenation_limit_lines_ == other.hyphenation_limit_lines_ &&
           text_emphasis_fill_ == other.text_emphasis_fill_ &&
           text_emphasis_mark_ == other.text_emphasis_mark_ &&
           text_emphasis_position_ == other.text_emphasis_position_ &&
           text_align_last_ == other.text_align_last_ &&
           text_justify_ == other.text_justify_ &&
           text_orientation_ == other.text_orientation_ &&
           text_combine_ == other.text_combine_ &&
           text_indent_line_ == other.text_indent_line_ &&
           text_indent_type_ == other.text_indent_type_ &&
           subtree_will_change_contents_ ==
               other.subtree_will_change_contents_ &&
           self_or_ancestor_has_dir_auto_attribute_ ==
               other.self_or_ancestor_has_dir_auto_attribute_ &&
           respect_image_orientation_ == other.respect_image_orientation_ &&
           subtree_is_sticky_ == other.subtree_is_sticky_ &&
           hyphenation_string_ == other.hyphenation_string_ &&
           line_height_step_ == other.line_height_step_ &&
           text_emphasis_custom_mark_ == other.text_emphasis_custom_mark_ &&
           DataEquivalent(quotes_, other.quotes_) &&
           tab_size_ == other.tab_size_ &&
           image_rendering_ == other.image_rendering_ &&
           text_underline_position_ == other.text_underline_position_ &&
           text_decoration_skip_ == other.text_decoration_skip_ &&
           ruby_position_ == other.ruby_position_ &&
           DataEquivalent(list_style_image_, other.list_style_image_) &&
           DataEquivalent(applied_text_decorations_,
                          other.applied_text_decorations_) &&
           DataEquivalent(variables_, other.variables_) &&
           text_size_adjust_ == other.text_size_adjust_;
  }
  bool operator!=(const StyleRareInheritedData& o) const {
    return !(*this == o);
  }

  Persistent<StyleImage> list_style_image_;

  Color text_stroke_color_;
  float text_stroke_width_;
  Color text_fill_color_;
  Color text_emphasis_color_;
  Color caret_color_;

  Color visited_link_text_stroke_color_;
  Color visited_link_text_fill_color_;
  Color visited_link_text_emphasis_color_;
  Color visited_link_caret_color_;

  RefPtr<ShadowList>
      text_shadow_;  // Our text shadow information for shadowed text drawing.
  AtomicString
      highlight_;  // Apple-specific extension for custom highlight rendering.

  Persistent<CursorList> cursor_data_;

  Length text_indent_;
  float effective_zoom_;

  // Paged media properties.
  short widows_;
  short orphans_;

  unsigned text_stroke_color_is_current_color_ : 1;
  unsigned text_fill_color_is_current_color_ : 1;
  unsigned text_emphasis_color_is_current_color_ : 1;
  unsigned caret_color_is_current_color_ : 1;
  unsigned caret_color_is_auto_ : 1;
  unsigned visited_link_text_stroke_color_is_current_color_ : 1;
  unsigned visited_link_text_fill_color_is_current_color_ : 1;
  unsigned visited_link_text_emphasis_color_is_current_color_ : 1;
  unsigned visited_link_caret_color_is_current_color_ : 1;
  unsigned visited_link_caret_color_is_auto_ : 1;

  unsigned text_security_ : 2;           // ETextSecurity
  unsigned user_modify_ : 2;             // EUserModify (editing)
  unsigned word_break_ : 2;              // EWordBreak
  unsigned overflow_wrap_ : 1;           // EOverflowWrap
  unsigned line_break_ : 3;              // LineBreak
  unsigned user_select_ : 2;             // EUserSelect
  unsigned speak_ : 3;                   // ESpeak
  unsigned hyphens_ : 2;                 // Hyphens
  unsigned text_emphasis_fill_ : 1;      // TextEmphasisFill
  unsigned text_emphasis_mark_ : 3;      // TextEmphasisMark
  unsigned text_emphasis_position_ : 1;  // TextEmphasisPosition
  unsigned text_align_last_ : 3;        // TextAlignLast
  unsigned text_justify_ : 2;           // TextJustify
  unsigned text_orientation_ : 2;       // TextOrientation
  unsigned text_combine_ : 1;           // CSS3 text-combine-upright properties
  unsigned text_indent_line_ : 1;       // TextIndentEachLine
  unsigned text_indent_type_ : 1;       // TextIndentHanging
  // CSS Image Values Level 3
  unsigned image_rendering_ : 3;          // EImageRendering
  unsigned text_underline_position_ : 1;  // TextUnderlinePosition
  unsigned text_decoration_skip_ : 3;     // TextDecorationSkip
  unsigned ruby_position_ : 1;            // RubyPosition

  // Though will-change is not itself an inherited property, the intent
  // expressed by 'will-change: contents' includes descendants.
  unsigned subtree_will_change_contents_ : 1;

  unsigned self_or_ancestor_has_dir_auto_attribute_ : 1;

  unsigned respect_image_orientation_ : 1;

  // Though position: sticky is not itself an inherited property, being a
  // descendent of a sticky element changes some document lifecycle logic.
  unsigned subtree_is_sticky_ : 1;

  AtomicString hyphenation_string_;
  short hyphenation_limit_before_;
  short hyphenation_limit_after_;
  short hyphenation_limit_lines_;

  uint8_t line_height_step_;

  AtomicString text_emphasis_custom_mark_;
  RefPtr<QuotesData> quotes_;

  Color tap_highlight_color_;

  RefPtr<AppliedTextDecorationList> applied_text_decorations_;
  TabSize tab_size_;

  RefPtr<StyleInheritedVariables> variables_;
  TextSizeAdjust text_size_adjust_;

 private:
  StyleRareInheritedData()
      : list_style_image_(nullptr),
        text_stroke_width_(0),
        text_indent_(Length(kFixed)),
        effective_zoom_(1.0),
        widows_(2),
        orphans_(2),
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
        text_security_(static_cast<unsigned>(ETextSecurity::kNone)),
        user_modify_(static_cast<unsigned>(EUserModify::kReadOnly)),
        word_break_(static_cast<unsigned>(EWordBreak::kNormal)),
        overflow_wrap_(static_cast<unsigned>(EOverflowWrap::kNormal)),
        line_break_(static_cast<unsigned>(LineBreak::kAuto)),
        user_select_(static_cast<unsigned>(EUserSelect::kText)),
        speak_(static_cast<unsigned>(ESpeak::kNormal)),
        hyphens_(static_cast<unsigned>(Hyphens::kManual)),
        text_emphasis_fill_(kTextEmphasisFillFilled),
        text_emphasis_mark_(kTextEmphasisMarkNone),
        text_emphasis_position_(kTextEmphasisPositionOver),
        text_align_last_(kTextAlignLastAuto),
        text_justify_(kTextJustifyAuto),
        text_orientation_(kTextOrientationMixed),
        text_combine_(kTextCombineNone),
        text_indent_line_(kTextIndentFirstLine),
        text_indent_type_(kTextIndentNormal),
        image_rendering_(kImageRenderingAuto),
        text_underline_position_(kTextUnderlinePositionAuto),
        text_decoration_skip_(kTextDecorationSkipObjects),
        ruby_position_(kRubyPositionBefore),
        subtree_will_change_contents_(false),
        self_or_ancestor_has_dir_auto_attribute_(false),
        respect_image_orientation_(false),
        subtree_is_sticky_(false),
        hyphenation_limit_before_(-1),
        hyphenation_limit_after_(-1),
        hyphenation_limit_lines_(-1),
        line_height_step_(0),
        tap_highlight_color_(LayoutTheme::TapHighlightColor()),
        tab_size_(TabSize(8)),
        text_size_adjust_(TextSizeAdjust::AdjustAuto()) {}

  StyleRareInheritedData(const StyleRareInheritedData&) = default;
};

}  // namespace blink

#endif  // StyleRareInheritedData_h
