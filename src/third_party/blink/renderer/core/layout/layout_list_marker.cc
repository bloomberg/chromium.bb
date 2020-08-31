/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
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

#include "third_party/blink/renderer/core/layout/layout_list_marker.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {

const int kCMarkerPaddingPx = 7;

// TODO(glebl): Move to core/html/resources/html.css after
// Blink starts to support ::marker crbug.com/457718
// Recommended UA margin for list markers.
const int kCUAMarkerMarginEm = 1;

LayoutListMarker::LayoutListMarker(Element* element) : LayoutBox(element) {
  DCHECK(ListItem());
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutListMarker::~LayoutListMarker() = default;

void LayoutListMarker::WillBeDestroyed() {
  if (image_)
    image_->RemoveClient(this);
  LayoutBox::WillBeDestroyed();
}

const LayoutListItem* LayoutListMarker::ListItem() const {
  LayoutObject* list_item = GetNode()->parentNode()->GetLayoutObject();
  DCHECK(list_item);
  DCHECK(list_item->IsListItem());
  return ToLayoutListItem(list_item);
}

LayoutSize LayoutListMarker::ImageBulletSize() const {
  DCHECK(IsImage());
  const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutSize();

  // FIXME: This is a somewhat arbitrary default width. Generated images for
  // markers really won't become particularly useful until we support the CSS3
  // marker pseudoclass to allow control over the width and height of the
  // marker box.
  LayoutUnit bullet_width =
      font_data->GetFontMetrics().Ascent() / LayoutUnit(2);
  return RoundedLayoutSize(
      image_->ImageSize(GetDocument(), StyleRef().EffectiveZoom(),
                        LayoutSize(bullet_width, bullet_width),
                        LayoutObject::ShouldRespectImageOrientation(this)));
}

void LayoutListMarker::StyleWillChange(StyleDifference diff,
                                       const ComputedStyle& new_style) {
  if (Style() &&
      (new_style.ListStylePosition() != StyleRef().ListStylePosition() ||
       new_style.ListStyleType() != StyleRef().ListStyleType() ||
       (new_style.ListStyleType() == EListStyleType::kString &&
        new_style.ListStyleStringValue() !=
            StyleRef().ListStyleStringValue()))) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }

  LayoutBox::StyleWillChange(diff, new_style);
}

void LayoutListMarker::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutBox::StyleDidChange(diff, old_style);

  if (image_ != StyleRef().ListStyleImage()) {
    if (image_)
      image_->RemoveClient(this);
    image_ = StyleRef().ListStyleImage();
    if (image_)
      image_->AddClient(this);
  }
}

InlineBox* LayoutListMarker::CreateInlineBox() {
  InlineBox* result = LayoutBox::CreateInlineBox();
  result->SetIsText(IsText());
  return result;
}

bool LayoutListMarker::IsImage() const {
  return image_ && !image_->ErrorOccurred();
}

void LayoutListMarker::Paint(const PaintInfo& paint_info) const {
  ListMarkerPainter(*this).Paint(paint_info);
}

void LayoutListMarker::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  LayoutUnit block_offset = LogicalTop();
  const LayoutListItem* list_item = ListItem();
  for (LayoutBox* o = ParentBox(); o && o != list_item; o = o->ParentBox()) {
    block_offset += o->LogicalTop();
  }
  if (list_item->StyleRef().IsLeftToRightDirection()) {
    line_offset_ = list_item->LogicalLeftOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  } else {
    line_offset_ = list_item->LogicalRightOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  }
  if (IsImage()) {
    UpdateMarginsAndContent();
    LayoutSize image_size(ImageBulletSize());
    SetWidth(image_size.Width());
    SetHeight(image_size.Height());
  } else {
    const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
    DCHECK(font_data);
    SetLogicalWidth(PreferredLogicalWidths().min_size);
    SetLogicalHeight(
        LayoutUnit(font_data ? font_data->GetFontMetrics().Height() : 0));
  }

  ClearNeedsLayout();
}

void LayoutListMarker::ImageChanged(WrappedImagePtr o, CanDeferInvalidation) {
  // A list marker can't have a background or border image, so no need to call
  // the base class method.
  if (!image_ || o != image_->Data())
    return;

  LayoutSize image_size = IsImage() ? ImageBulletSize() : LayoutSize();
  if (Size() != image_size || image_->ErrorOccurred()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kImageChanged);
  } else {
    SetShouldDoFullPaintInvalidation();
  }
}

void LayoutListMarker::UpdateMarginsAndContent() {
  UpdateMargins(PreferredLogicalWidths().min_size);
}

void LayoutListMarker::UpdateContent() {
  DCHECK(IntrinsicLogicalWidthsDirty());

  text_ = "";

  if (IsImage())
    return;

  switch (GetListStyleCategory()) {
    case ListStyleCategory::kNone:
      break;
    case ListStyleCategory::kSymbol:
      text_ = list_marker_text::GetText(StyleRef().ListStyleType(),
                                        0);  // value is ignored for these types
      break;
    case ListStyleCategory::kLanguage:
      text_ = list_marker_text::GetText(StyleRef().ListStyleType(),
                                        ListItem()->Value());
      break;
    case ListStyleCategory::kStaticString:
      text_ = StyleRef().ListStyleStringValue();
      break;
  }
}

String LayoutListMarker::TextAlternative() const {
  if (GetListStyleCategory() == ListStyleCategory::kStaticString)
    return text_;
  UChar suffix =
      list_marker_text::Suffix(StyleRef().ListStyleType(), ListItem()->Value());
  // Return suffix after the marker text, even in RTL, reflecting speech order.
  return text_ + suffix + ' ';
}

LayoutUnit LayoutListMarker::GetWidthOfText(ListStyleCategory category) const {
  // TODO(crbug.com/1012289): this code doesn't support bidi algorithm.
  if (text_.IsEmpty())
    return LayoutUnit();
  const Font& font = StyleRef().GetFont();
  LayoutUnit item_width = LayoutUnit(font.Width(TextRun(text_)));
  if (category == ListStyleCategory::kStaticString) {
    // Don't add a suffix.
    return item_width;
  }
  // TODO(wkorman): Look into constructing a text run for both text and suffix
  // and painting them together.
  UChar suffix[2] = {
      list_marker_text::Suffix(StyleRef().ListStyleType(), ListItem()->Value()),
      ' '};
  TextRun run =
      ConstructTextRun(font, suffix, 2, StyleRef(), StyleRef().Direction());
  LayoutUnit suffix_space_width = LayoutUnit(font.Width(run));
  return item_width + suffix_space_width;
}

MinMaxSizes LayoutListMarker::ComputeIntrinsicLogicalWidths() const {
  DCHECK(IntrinsicLogicalWidthsDirty());
  const_cast<LayoutListMarker*>(this)->UpdateContent();

  MinMaxSizes sizes;
  if (IsImage()) {
    LayoutSize image_size(ImageBulletSize());
    sizes = StyleRef().IsHorizontalWritingMode() ? image_size.Width()
                                                 : image_size.Height();
  } else {
    ListStyleCategory category = GetListStyleCategory();
    switch (category) {
      case ListStyleCategory::kNone:
        break;
      case ListStyleCategory::kSymbol:
        sizes = WidthOfSymbol(StyleRef());
        break;
      case ListStyleCategory::kLanguage:
      case ListStyleCategory::kStaticString:
        sizes = GetWidthOfText(category);
        break;
    }
  }

  const_cast<LayoutListMarker*>(this)->UpdateMargins(sizes.min_size);
  return sizes;
}

MinMaxSizes LayoutListMarker::PreferredLogicalWidths() const {
  return IntrinsicLogicalWidths();
}

LayoutUnit LayoutListMarker::WidthOfSymbol(const ComputedStyle& style) {
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit();
  return LayoutUnit((font_data->GetFontMetrics().Ascent() * 2 / 3 + 1) / 2 + 2);
}

void LayoutListMarker::UpdateMargins(LayoutUnit marker_inline_size) {
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  const ComputedStyle& style = StyleRef();
  if (IsInsideListMarker()) {
    std::tie(margin_start, margin_end) =
        InlineMarginsForInside(style, IsImage());
  } else {
    std::tie(margin_start, margin_end) =
        InlineMarginsForOutside(style, IsImage(), marker_inline_size);
  }

  SetMarginStart(margin_start);
  SetMarginEnd(margin_end);
}

std::pair<LayoutUnit, LayoutUnit> LayoutListMarker::InlineMarginsForInside(
    const ComputedStyle& style,
    bool is_image) {
  if (!style.ContentBehavesAsNormal())
    return {};
  if (is_image)
    return {LayoutUnit(), LayoutUnit(kCMarkerPaddingPx)};
  switch (GetListStyleCategory(style.ListStyleType())) {
    case ListStyleCategory::kSymbol:
      return {LayoutUnit(-1),
              LayoutUnit(kCUAMarkerMarginEm * style.ComputedFontSize())};
    default:
      break;
  }
  return {};
}

std::pair<LayoutUnit, LayoutUnit> LayoutListMarker::InlineMarginsForOutside(
    const ComputedStyle& style,
    bool is_image,
    LayoutUnit marker_inline_size) {
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  if (!style.ContentBehavesAsNormal()) {
    margin_start = -marker_inline_size;
  } else if (is_image) {
    margin_start = -marker_inline_size - kCMarkerPaddingPx;
    margin_end = LayoutUnit(kCMarkerPaddingPx);
  } else {
    switch (GetListStyleCategory(style.ListStyleType())) {
      case ListStyleCategory::kNone:
        break;
      case ListStyleCategory::kSymbol: {
        const SimpleFontData* font_data = style.GetFont().PrimaryFont();
        DCHECK(font_data);
        if (!font_data)
          return {};
        const FontMetrics& font_metrics = font_data->GetFontMetrics();
        int offset = font_metrics.Ascent() * 2 / 3;
        margin_start = LayoutUnit(-offset - kCMarkerPaddingPx - 1);
        margin_end = offset + kCMarkerPaddingPx + 1 - marker_inline_size;
        break;
      }
      default:
        margin_start = -marker_inline_size;
    }
  }
  DCHECK_EQ(margin_start + margin_end, -marker_inline_size);
  return {margin_start, margin_end};
}

LayoutUnit LayoutListMarker::LineHeight(
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  if (!IsImage())
    return ListItem()->LineHeight(first_line, direction,
                                  kPositionOfInteriorLineBoxes);
  return LayoutBox::LineHeight(first_line, direction, line_position_mode);
}

LayoutUnit LayoutListMarker::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  if (!IsImage())
    return ListItem()->BaselinePosition(baseline_type, first_line, direction,
                                        kPositionOfInteriorLineBoxes);
  return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                     line_position_mode);
}

LayoutListMarker::ListStyleCategory LayoutListMarker::GetListStyleCategory()
    const {
  return GetListStyleCategory(StyleRef().ListStyleType());
}

LayoutListMarker::ListStyleCategory LayoutListMarker::GetListStyleCategory(
    EListStyleType type) {
  switch (type) {
    case EListStyleType::kNone:
      return ListStyleCategory::kNone;
    case EListStyleType::kString:
      return ListStyleCategory::kStaticString;
    case EListStyleType::kDisc:
    case EListStyleType::kCircle:
    case EListStyleType::kSquare:
      return ListStyleCategory::kSymbol;
    case EListStyleType::kArabicIndic:
    case EListStyleType::kArmenian:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
    case EListStyleType::kGeorgian:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHebrew:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKannada:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kKhmer:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
    case EListStyleType::kLao:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kLowerRoman:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperArmenian:
    case EListStyleType::kUpperLatin:
    case EListStyleType::kUpperRoman:
    case EListStyleType::kUrdu:
      return ListStyleCategory::kLanguage;
    default:
      NOTREACHED();
      return ListStyleCategory::kLanguage;
  }
}

LayoutRect LayoutListMarker::GetRelativeMarkerRect() const {
  if (IsImage())
    return LayoutRect(LayoutPoint(), ImageBulletSize());

  LayoutRect relative_rect;
  ListStyleCategory category = GetListStyleCategory();
  switch (category) {
    case ListStyleCategory::kNone:
      return LayoutRect();
    case ListStyleCategory::kSymbol:
      return RelativeSymbolMarkerRect(StyleRef(), Size().Width());
    case ListStyleCategory::kLanguage:
    case ListStyleCategory::kStaticString: {
      const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
      DCHECK(font_data);
      if (!font_data)
        return relative_rect;
      relative_rect =
          LayoutRect(LayoutUnit(), LayoutUnit(), GetWidthOfText(category),
                     LayoutUnit(font_data->GetFontMetrics().Height()));
      break;
    }
  }

  if (!StyleRef().IsHorizontalWritingMode()) {
    relative_rect = relative_rect.TransposedRect();
    relative_rect.SetX(Size().Width() - relative_rect.X() -
                       relative_rect.Width());
  }
  return relative_rect;
}

LayoutRect LayoutListMarker::RelativeSymbolMarkerRect(
    const ComputedStyle& style,
    LayoutUnit width) {
  LayoutRect relative_rect;
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutRect();

  // TODO(wkorman): Review and clean up/document the calculations below.
  // http://crbug.com/543193
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  int ascent = font_metrics.Ascent();
  int bullet_width = (ascent * 2 / 3 + 1) / 2;
  relative_rect = LayoutRect(1, 3 * (ascent - ascent * 2 / 3) / 2, bullet_width,
                             bullet_width);
  if (!style.IsHorizontalWritingMode()) {
    relative_rect = relative_rect.TransposedRect();
    relative_rect.SetX(width - relative_rect.X() - relative_rect.Width());
  }
  return relative_rect;
}

}  // namespace blink
