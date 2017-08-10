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

#include "core/layout/LayoutListMarker.h"

#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutListItem.h"
#include "core/layout/ListMarkerText.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/paint/ListMarkerPainter.h"
#include "platform/fonts/Font.h"

namespace blink {

const int kCMarkerPaddingPx = 7;

// TODO(glebl): Move to WebKit/Source/core/css/html.css after
// Blink starts to support ::marker crbug.com/457718
// Recommended UA margin for list markers.
const int kCUAMarkerMarginEm = 1;

LayoutListMarker::LayoutListMarker(LayoutListItem* item)
    : LayoutBox(nullptr), list_item_(item), line_offset_() {
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutListMarker::~LayoutListMarker() {}

void LayoutListMarker::WillBeDestroyed() {
  if (image_)
    image_->RemoveClient(this);
  LayoutBox::WillBeDestroyed();
}

LayoutListMarker* LayoutListMarker::CreateAnonymous(LayoutListItem* item) {
  Document& document = item->GetDocument();
  LayoutListMarker* layout_object = new LayoutListMarker(item);
  layout_object->SetDocumentForAnonymous(&document);
  return layout_object;
}

LayoutSize LayoutListMarker::ImageBulletSize() const {
  DCHECK(IsImage());
  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutSize();

  // FIXME: This is a somewhat arbitrary default width. Generated images for
  // markers really won't become particularly useful until we support the CSS3
  // marker pseudoclass to allow control over the width and height of the
  // marker box.
  LayoutUnit bullet_width =
      font_data->GetFontMetrics().Ascent() / LayoutUnit(2);
  return image_->ImageSize(GetDocument(), Style()->EffectiveZoom(),
                           LayoutSize(bullet_width, bullet_width));
}

void LayoutListMarker::StyleWillChange(StyleDifference diff,
                                       const ComputedStyle& new_style) {
  if (Style() &&
      (new_style.ListStylePosition() != Style()->ListStylePosition() ||
       new_style.ListStyleType() != Style()->ListStyleType()))
    SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::kStyleChange);

  LayoutBox::StyleWillChange(diff, new_style);
}

void LayoutListMarker::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutBox::StyleDidChange(diff, old_style);

  if (image_ != Style()->ListStyleImage()) {
    if (image_)
      image_->RemoveClient(this);
    image_ = Style()->ListStyleImage();
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

LayoutRect LayoutListMarker::LocalSelectionRect() const {
  InlineBox* box = InlineBoxWrapper();
  if (!box)
    return LayoutRect(LayoutPoint(), Size());
  RootInlineBox& root = InlineBoxWrapper()->Root();
  const ComputedStyle* block_style = root.Block().Style();
  LayoutUnit new_logical_top =
      block_style->IsFlippedBlocksWritingMode()
          ? InlineBoxWrapper()->LogicalBottom() - root.SelectionBottom()
          : root.SelectionTop() - InlineBoxWrapper()->LogicalTop();
  return block_style->IsHorizontalWritingMode()
             ? LayoutRect(LayoutUnit(), new_logical_top, Size().Width(),
                          root.SelectionHeight())
             : LayoutRect(new_logical_top, LayoutUnit(), root.SelectionHeight(),
                          Size().Height());
}

void LayoutListMarker::Paint(const PaintInfo& paint_info,
                             const LayoutPoint& paint_offset) const {
  ListMarkerPainter(*this).Paint(paint_info, paint_offset);
}

void LayoutListMarker::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  LayoutUnit block_offset = LogicalTop();
  for (LayoutBox* o = ParentBox(); o && o != ListItem(); o = o->ParentBox()) {
    block_offset += o->LogicalTop();
  }
  if (ListItem()->Style()->IsLeftToRightDirection()) {
    line_offset_ = ListItem()->LogicalLeftOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  } else {
    line_offset_ = ListItem()->LogicalRightOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  }
  if (IsImage()) {
    UpdateMarginsAndContent();
    LayoutSize image_size(ImageBulletSize());
    SetWidth(image_size.Width());
    SetHeight(image_size.Height());
  } else {
    const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
    DCHECK(font_data);
    SetLogicalWidth(MinPreferredLogicalWidth());
    SetLogicalHeight(
        LayoutUnit(font_data ? font_data->GetFontMetrics().Height() : 0));
  }

  SetMarginStart(LayoutUnit());
  SetMarginEnd(LayoutUnit());

  Length start_margin = Style()->MarginStart();
  Length end_margin = Style()->MarginEnd();
  if (start_margin.IsFixed())
    SetMarginStart(LayoutUnit(start_margin.Value()));
  if (end_margin.IsFixed())
    SetMarginEnd(LayoutUnit(end_margin.Value()));

  ClearNeedsLayout();
}

void LayoutListMarker::ImageChanged(WrappedImagePtr o, const IntRect*) {
  // A list marker can't have a background or border image, so no need to call
  // the base class method.
  if (!image_ || o != image_->Data())
    return;

  LayoutSize image_size = IsImage() ? ImageBulletSize() : LayoutSize();
  if (Size() != image_size || image_->ErrorOccurred())
    SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::kImageChanged);
  else
    SetShouldDoFullPaintInvalidation();
}

void LayoutListMarker::UpdateMarginsAndContent() {
  UpdateContent();
  UpdateMargins();
}

void LayoutListMarker::UpdateContent() {
  // FIXME: This if-statement is just a performance optimization, but it's messy
  // to use the preferredLogicalWidths dirty bit for this.
  // It's unclear if this is a premature optimization.
  if (!PreferredLogicalWidthsDirty())
    return;

  text_ = "";

  if (IsImage())
    return;

  switch (GetListStyleCategory()) {
    case ListStyleCategory::kNone:
      break;
    case ListStyleCategory::kSymbol:
      text_ = ListMarkerText::GetText(Style()->ListStyleType(),
                                      0);  // value is ignored for these types
      break;
    case ListStyleCategory::kLanguage:
      text_ = ListMarkerText::GetText(Style()->ListStyleType(),
                                      list_item_->Value());
      break;
  }
}

LayoutUnit LayoutListMarker::GetWidthOfTextWithSuffix() const {
  if (text_.IsEmpty())
    return LayoutUnit();
  const Font& font = Style()->GetFont();
  LayoutUnit item_width = LayoutUnit(font.Width(TextRun(text_)));
  // TODO(wkorman): Look into constructing a text run for both text and suffix
  // and painting them together.
  UChar suffix[2] = {
      ListMarkerText::Suffix(Style()->ListStyleType(), list_item_->Value()),
      ' '};
  TextRun run =
      ConstructTextRun(font, suffix, 2, StyleRef(), Style()->Direction());
  LayoutUnit suffix_space_width = LayoutUnit(font.Width(run));
  return item_width + suffix_space_width;
}

void LayoutListMarker::ComputePreferredLogicalWidths() {
  DCHECK(PreferredLogicalWidthsDirty());
  UpdateContent();

  if (IsImage()) {
    LayoutSize image_size(ImageBulletSize());
    min_preferred_logical_width_ = max_preferred_logical_width_ =
        Style()->IsHorizontalWritingMode() ? image_size.Width()
                                           : image_size.Height();
    ClearPreferredLogicalWidthsDirty();
    UpdateMargins();
    return;
  }

  const Font& font = Style()->GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  LayoutUnit logical_width;
  switch (GetListStyleCategory()) {
    case ListStyleCategory::kNone:
      break;
    case ListStyleCategory::kSymbol:
      logical_width = LayoutUnit(
          (font_data->GetFontMetrics().Ascent() * 2 / 3 + 1) / 2 + 2);
      break;
    case ListStyleCategory::kLanguage:
      logical_width = GetWidthOfTextWithSuffix();
      break;
  }

  min_preferred_logical_width_ = logical_width;
  max_preferred_logical_width_ = logical_width;

  ClearPreferredLogicalWidthsDirty();

  UpdateMargins();
}

void LayoutListMarker::UpdateMargins() {
  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;
  const FontMetrics& font_metrics = font_data->GetFontMetrics();

  LayoutUnit margin_start;
  LayoutUnit margin_end;

  if (IsInside()) {
    if (IsImage()) {
      margin_end = LayoutUnit(kCMarkerPaddingPx);
    } else {
      switch (GetListStyleCategory()) {
        case ListStyleCategory::kSymbol:
          margin_start = LayoutUnit(-1);
          margin_end =
              LayoutUnit(kCUAMarkerMarginEm * Style()->ComputedFontSize());
          break;
        default:
          break;
      }
    }
  } else {
    if (Style()->IsLeftToRightDirection()) {
      if (IsImage()) {
        margin_start = -MinPreferredLogicalWidth() - kCMarkerPaddingPx;
      } else {
        int offset = font_metrics.Ascent() * 2 / 3;
        switch (GetListStyleCategory()) {
          case ListStyleCategory::kNone:
            break;
          case ListStyleCategory::kSymbol:
            margin_start = LayoutUnit(-offset - kCMarkerPaddingPx - 1);
            break;
          default:
            margin_start =
                text_.IsEmpty() ? LayoutUnit() : -MinPreferredLogicalWidth();
        }
      }
      margin_end = -margin_start - MinPreferredLogicalWidth();
    } else {
      if (IsImage()) {
        margin_end = LayoutUnit(kCMarkerPaddingPx);
      } else {
        int offset = font_metrics.Ascent() * 2 / 3;
        switch (GetListStyleCategory()) {
          case ListStyleCategory::kNone:
            break;
          case ListStyleCategory::kSymbol:
            margin_end =
                offset + kCMarkerPaddingPx + 1 - MinPreferredLogicalWidth();
            break;
          default:
            margin_end = LayoutUnit();
        }
      }
      margin_start = -margin_end - MinPreferredLogicalWidth();
    }
  }

  MutableStyleRef().SetMarginStart(Length(margin_start, kFixed));
  MutableStyleRef().SetMarginEnd(Length(margin_end, kFixed));
}

LayoutUnit LayoutListMarker::LineHeight(
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  if (!IsImage())
    return list_item_->LineHeight(first_line, direction,
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
    return list_item_->BaselinePosition(baseline_type, first_line, direction,
                                        kPositionOfInteriorLineBoxes);
  return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                     line_position_mode);
}

LayoutListMarker::ListStyleCategory LayoutListMarker::GetListStyleCategory()
    const {
  switch (Style()->ListStyleType()) {
    case EListStyleType::kNone:
      return ListStyleCategory::kNone;
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

bool LayoutListMarker::IsInside() const {
  return list_item_->NotInList() ||
         Style()->ListStylePosition() == EListStylePosition::kInside;
}

IntRect LayoutListMarker::GetRelativeMarkerRect() const {
  if (IsImage()) {
    IntSize image_size = FlooredIntSize(ImageBulletSize());
    return IntRect(0, 0, image_size.Width(), image_size.Height());
  }

  IntRect relative_rect;
  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return relative_rect;

  switch (GetListStyleCategory()) {
    case ListStyleCategory::kNone:
      return IntRect();
    case ListStyleCategory::kSymbol: {
      // TODO(wkorman): Review and clean up/document the calculations below.
      // http://crbug.com/543193
      const FontMetrics& font_metrics = font_data->GetFontMetrics();
      int ascent = font_metrics.Ascent();
      int bullet_width = (ascent * 2 / 3 + 1) / 2;
      relative_rect = IntRect(1, 3 * (ascent - ascent * 2 / 3) / 2,
                              bullet_width, bullet_width);
    } break;
    case ListStyleCategory::kLanguage:
      relative_rect = IntRect(0, 0, GetWidthOfTextWithSuffix().ToInt(),
                              font_data->GetFontMetrics().Height());
      break;
  }

  if (!Style()->IsHorizontalWritingMode()) {
    relative_rect = relative_rect.TransposedRect();
    relative_rect.SetX(
        (Size().Width() - relative_rect.X() - relative_rect.Width()).ToInt());
  }

  return relative_rect;
}

void LayoutListMarker::ListItemStyleDidChange() {
  RefPtr<ComputedStyle> new_style = ComputedStyle::Create();
  // The marker always inherits from the list item, regardless of where it might
  // end up (e.g., in some deeply nested line box). See CSS3 spec.
  new_style->InheritFrom(list_item_->StyleRef());
  if (Style()) {
    // Reuse the current margins. Otherwise resetting the margins to initial
    // values would trigger unnecessary layout.
    new_style->SetMarginStart(Style()->MarginStart());
    new_style->SetMarginEnd(Style()->MarginRight());
  }
  SetStyle(std::move(new_style));
}

}  // namespace blink
