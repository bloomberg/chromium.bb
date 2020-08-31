/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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

#include "third_party/blink/renderer/core/layout/layout_text_control.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"

namespace blink {

LayoutTextControl::LayoutTextControl(TextControlElement* element)
    : LayoutBlockFlow(element) {
  DCHECK(element);
}

LayoutTextControl::~LayoutTextControl() = default;

TextControlElement* LayoutTextControl::GetTextControlElement() const {
  return ToTextControl(GetNode());
}

TextControlInnerEditorElement* LayoutTextControl::InnerEditorElement() const {
  return GetTextControlElement()->InnerEditorElement();
}

void LayoutTextControl::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  LayoutBlockFlow::StyleDidChange(diff, old_style);
  TextControlInnerEditorElement* inner_editor = InnerEditorElement();
  if (!inner_editor)
    return;
  LayoutBlock* inner_editor_layout_object =
      To<LayoutBlock>(inner_editor->GetLayoutObject());
  if (inner_editor_layout_object) {
    // This is necessary to update the style on the inner_editor based on the
    // changes in the input element ComputedStyle.
    // (See TextControlInnerEditorElement::CreateInnerEditorStyle()).
    inner_editor->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kControl));

    // The inner editor element uses the LayoutTextControl's ::selection style
    // (see: GetUncachedSelectionStyle in SelectionPaintingUtils.cpp) so ensure
    // the inner editor selection is invalidated anytime style changes and a
    // ::selection style is or was present on LayoutTextControl.
    if (StyleRef().HasPseudoElementStyle(kPseudoIdSelection) ||
        (old_style && old_style->HasPseudoElementStyle(kPseudoIdSelection))) {
      inner_editor_layout_object->InvalidateSelectedChildrenOnStyleChange();
    }
  }
}

int LayoutTextControl::ScrollbarThickness() const {
  // FIXME: We should get the size of the scrollbar from the LayoutTheme
  // instead.
  return GetDocument().GetPage()->GetScrollbarTheme().ScrollbarThickness();
}

void LayoutTextControl::ComputeLogicalHeight(
    LayoutUnit logical_height,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  HTMLElement* inner_editor = InnerEditorElement();
  DCHECK(inner_editor);
  if (LayoutBox* inner_editor_box = inner_editor->GetLayoutBox()) {
    LayoutUnit non_content_height = inner_editor_box->BorderAndPaddingHeight() +
                                    inner_editor_box->MarginHeight();
    logical_height = ComputeControlLogicalHeight(
        inner_editor_box->LineHeight(true, kHorizontalLine,
                                     kPositionOfInteriorLineBoxes),
        non_content_height);

    // We are able to have a horizontal scrollbar if the overflow style is
    // scroll, or if its auto and there's no word wrap.
    if (StyleRef().OverflowInlineDirection() == EOverflow::kScroll ||
        (StyleRef().OverflowInlineDirection() == EOverflow::kAuto &&
         inner_editor->GetLayoutObject()->StyleRef().OverflowWrap() ==
             EOverflowWrap::kNormal))
      logical_height += ScrollbarThickness();

    // FIXME: The logical height of the inner text box should have been added
    // before calling computeLogicalHeight to avoid this hack.
    SetIntrinsicContentLogicalHeight(logical_height);

    logical_height += BorderAndPaddingHeight();
  }

  LayoutBox::ComputeLogicalHeight(logical_height, logical_top, computed_values);
}

void LayoutTextControl::HitInnerEditorElement(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset) {
  HTMLElement* inner_editor = InnerEditorElement();
  if (!inner_editor->GetLayoutObject())
    return;

  PhysicalOffset local_point =
      hit_test_location.Point() - accumulated_offset -
      inner_editor->GetLayoutObject()->LocalToAncestorPoint(PhysicalOffset(),
                                                            this);
  result.SetNodeAndPosition(inner_editor, local_point);
}

static const char* const kFontFamiliesWithInvalidCharWidth[] = {
    "American Typewriter",
    "Arial Hebrew",
    "Chalkboard",
    "Cochin",
    "Corsiva Hebrew",
    "Courier",
    "Euphemia UCAS",
    "Geneva",
    "Gill Sans",
    "Hei",
    "Helvetica",
    "Hoefler Text",
    "InaiMathi",
    "Kai",
    "Lucida Grande",
    "Marker Felt",
    "Monaco",
    "Mshtakan",
    "New Peninim MT",
    "Osaka",
    "Raanana",
    "STHeiti",
    "Symbol",
    "Times",
    "Apple Braille",
    "Apple LiGothic",
    "Apple LiSung",
    "Apple Symbols",
    "AppleGothic",
    "AppleMyungjo",
    "#GungSeo",
    "#HeadLineA",
    "#PCMyungjo",
    "#PilGi",
};

// For font families where any of the fonts don't have a valid entry in the OS/2
// table for avgCharWidth, fallback to the legacy webkit behavior of getting the
// avgCharWidth from the width of a '0'. This only seems to apply to a fixed
// number of Mac fonts, but, in order to get similar rendering across platforms,
// we do this check for all platforms.
bool LayoutTextControl::HasValidAvgCharWidth(const SimpleFontData* font_data,
                                             const AtomicString& family) {
  // Some fonts match avgCharWidth to CJK full-width characters.
  // Heuristic check to avoid such fonts.
  DCHECK(font_data);
  if (!font_data)
    return false;
  const FontMetrics& metrics = font_data->GetFontMetrics();
  if (metrics.HasZeroWidth() &&
      font_data->AvgCharWidth() > metrics.ZeroWidth() * 1.7)
    return false;

  static HashSet<AtomicString>* font_families_with_invalid_char_width_map =
      nullptr;

  if (family.IsEmpty())
    return false;

  if (!font_families_with_invalid_char_width_map) {
    font_families_with_invalid_char_width_map = new HashSet<AtomicString>;

    for (size_t i = 0; i < base::size(kFontFamiliesWithInvalidCharWidth); ++i)
      font_families_with_invalid_char_width_map->insert(
          AtomicString(kFontFamiliesWithInvalidCharWidth[i]));
  }

  return !font_families_with_invalid_char_width_map->Contains(family);
}

float LayoutTextControl::GetAvgCharWidth(const AtomicString& family) const {
  const Font& font = StyleRef().GetFont();

  const SimpleFontData* primary_font = font.PrimaryFont();
  if (primary_font && HasValidAvgCharWidth(primary_font, family))
    return roundf(primary_font->AvgCharWidth());

  const UChar kCh = '0';
  const String str = String(&kCh, 1);
  TextRun text_run =
      ConstructTextRun(font, str, StyleRef(), TextRun::kAllowTrailingExpansion);
  return font.Width(text_run);
}

MinMaxSizes LayoutTextControl::ComputeIntrinsicLogicalWidths() const {
  MinMaxSizes sizes;
  sizes += BorderAndPaddingLogicalWidth();

  // Use average character width. Matches IE.
  AtomicString family =
      StyleRef().GetFont().GetFontDescription().Family().Family();
  sizes.max_size += PreferredContentLogicalWidth(
      const_cast<LayoutTextControl*>(this)->GetAvgCharWidth(family));
  if (InnerEditorElement()) {
    if (LayoutBox* inner_editor_layout_box =
            InnerEditorElement()->GetLayoutBox()) {
      sizes.max_size += inner_editor_layout_box->PaddingStart() +
                        inner_editor_layout_box->PaddingEnd();
    }
  }
  if (!StyleRef().LogicalWidth().IsPercentOrCalc())
    sizes.min_size = sizes.max_size;
  return sizes;
}

void LayoutTextControl::AddOutlineRects(Vector<PhysicalRect>& rects,
                                        const PhysicalOffset& additional_offset,
                                        NGOutlineType) const {
  rects.emplace_back(additional_offset, Size());
}

LayoutObject* LayoutTextControl::LayoutSpecialExcludedChild(
    bool relayout_children,
    SubtreeLayoutScope& layout_scope) {
  HTMLElement* placeholder = ToTextControl(GetNode())->PlaceholderElement();
  LayoutObject* placeholder_layout_object =
      placeholder ? placeholder->GetLayoutObject() : nullptr;
  if (!placeholder_layout_object)
    return nullptr;
  if (relayout_children)
    layout_scope.SetChildNeedsLayout(placeholder_layout_object);
  return placeholder_layout_object;
}

LayoutUnit LayoutTextControl::FirstLineBoxBaseline() const {
  if (ShouldApplyLayoutContainment())
    return LayoutUnit(-1);

  LayoutUnit result = LayoutBlock::FirstLineBoxBaseline();
  if (result != -1)
    return result;

  // When the text is empty, |LayoutBlock::firstLineBoxBaseline()| cannot
  // compute the baseline because lineboxes do not exist.
  Element* inner_editor = InnerEditorElement();
  if (!inner_editor || !inner_editor->GetLayoutObject())
    return LayoutUnit(-1);

  LayoutBlock* inner_editor_layout_object =
      To<LayoutBlock>(inner_editor->GetLayoutObject());
  const SimpleFontData* font_data =
      inner_editor_layout_object->Style(true)->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit(-1);

  LayoutUnit baseline(font_data->GetFontMetrics().Ascent(kAlphabeticBaseline));
  for (LayoutObject* box = inner_editor_layout_object; box && box != this;
       box = box->Parent()) {
    if (box->IsBox())
      baseline += ToLayoutBox(box)->LogicalTop();
  }
  return baseline;
}

}  // namespace blink
