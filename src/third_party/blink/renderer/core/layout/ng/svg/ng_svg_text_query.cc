// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_query.h"

#include <unicode/utf16.h>

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

unsigned AdjustCodeUnitStartOffset(StringView string, unsigned offset) {
  return (U16_IS_TRAIL(string[offset]) && offset > 0 &&
          U16_IS_LEAD(string[offset - 1]))
             ? offset - 1
             : offset;
}

unsigned AdjustCodeUnitEndOffset(StringView string, unsigned offset) {
  return (offset < string.length() && U16_IS_TRAIL(string[offset]) &&
          offset > 0 && U16_IS_LEAD(string[offset - 1]))
             ? offset + 1
             : offset;
}

std::tuple<Vector<const NGFragmentItem*>, const NGFragmentItems*>
FragmentItemsInVisualOrder(const LayoutObject& query_root) {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items = nullptr;
  if (query_root.IsNGSVGText()) {
    DCHECK_LE(To<LayoutBox>(query_root).PhysicalFragmentCount(), 1u);
    for (const auto& fragment : To<LayoutBox>(query_root).PhysicalFragments()) {
      if (!fragment.Items())
        continue;
      items = fragment.Items();
      for (const auto& item : fragment.Items()->Items()) {
        if (item.Type() == NGFragmentItem::kSvgText)
          item_list.push_back(&item);
      }
    }
  } else {
    DCHECK(query_root.IsInLayoutNGInlineFormattingContext());
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(query_root);
    items = &cursor.Items();
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      const NGFragmentItem& item = *cursor.CurrentItem();
      if (item.Type() == NGFragmentItem::kSvgText) {
        item_list.push_back(&item);
      } else if (NGInlineCursor descendants = cursor.CursorForDescendants()) {
        for (; descendants; descendants.MoveToNext()) {
          if (descendants.CurrentItem()->Type() == NGFragmentItem::kSvgText)
            item_list.push_back(descendants.CurrentItem());
        }
      }
    }
  }
  return std::tie(item_list, items);
}

std::tuple<Vector<const NGFragmentItem*>, const NGFragmentItems*>
FragmentItemsInLogicalOrder(const LayoutObject& query_root) {
  auto items_tuple = FragmentItemsInVisualOrder(query_root);
  auto& item_list = std::get<0>(items_tuple);
  // Sort |item_list| in the logical order.
  std::sort(item_list.begin(), item_list.end(),
            [](const NGFragmentItem* a, const NGFragmentItem* b) {
              return a->StartOffset() < b->StartOffset();
            });
  return items_tuple;
}

// Returns a tuple of NGFragmentItem, Item text, IFC text offset for |index|,
// and the next IFC text offset.
std::tuple<const NGFragmentItem*, StringView, unsigned, unsigned>
FindFragmentItemForAddressableCodeUnitIndex(const LayoutObject& query_root,
                                            unsigned index) {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInLogicalOrder(query_root);

  unsigned character_index = 0;
  for (const auto* item : item_list) {
    const StringView item_text = item->Text(*items);
    if (character_index + item_text.length() <= index) {
      character_index += item_text.length();
      continue;
    }
    DCHECK_GE(index, character_index);
    DCHECK_LT(index, character_index + item_text.length());
    unsigned i = AdjustCodeUnitStartOffset(item_text, index - character_index);
    return {item, item_text, item->StartOffset() + i,
            item->StartOffset() + item_text.NextCodePointOffset(i)};
  }
  return {nullptr, StringView(), WTF::kNotFound, WTF::kNotFound};
}

void GetCanvasRotation(void* context,
                       unsigned,
                       Glyph,
                       FloatSize,
                       float,
                       bool,
                       CanvasRotationInVertical rotation,
                       const SimpleFontData*) {
  auto* canvas_rotation = static_cast<CanvasRotationInVertical*>(context);
  *canvas_rotation = rotation;
}

float InlineSize(const NGFragmentItem& item,
                 StringView item_text,
                 unsigned start_code_unit_offset,
                 unsigned end_code_unit_offset) {
  unsigned start_ifc_offset =
      item.StartOffset() +
      AdjustCodeUnitStartOffset(item_text, start_code_unit_offset);
  unsigned end_ifc_offset =
      item.StartOffset() +
      AdjustCodeUnitEndOffset(item_text, end_code_unit_offset);
  PhysicalRect r = item.LocalRect(item_text, start_ifc_offset, end_ifc_offset);
  return (item.IsHorizontal() ? r.Width() : r.Height()) *
         item.SvgFragmentData()->length_adjust_scale / item.SvgScalingFactor();
}

std::tuple<const NGFragmentItem*, FloatRect> ScaledCharacterRectInContainer(
    const LayoutObject& query_root,
    unsigned code_unit_index) {
  const NGFragmentItem* item;
  unsigned start_ifc_offset, end_ifc_offset;
  StringView item_text;
  std::tie(item, item_text, start_ifc_offset, end_ifc_offset) =
      FindFragmentItemForAddressableCodeUnitIndex(query_root, code_unit_index);
  DCHECK(item);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return {item, FloatRect()};
  auto char_rect =
      FloatRect(item->LocalRect(item_text, start_ifc_offset, end_ifc_offset));
  char_rect.MoveBy(item->SvgFragmentData()->rect.Location());
  return {item, char_rect};
}

}  // namespace

unsigned NGSvgTextQuery::NumberOfCharacters() const {
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInLogicalOrder(query_root_);

  unsigned addressable_code_unit_count = 0;
  for (const auto* item : item_list)
    addressable_code_unit_count += item->Text(*items).length();
  return addressable_code_unit_count;
}

float NGSvgTextQuery::SubStringLength(unsigned start_index,
                                      unsigned length) const {
  if (length <= 0)
    return 0.0f;
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInLogicalOrder(query_root_);

  float total_length = 0.0f;
  // Starting addressable code unit index for the current NGFragmentItem.
  unsigned character_index = 0;
  const unsigned end_index = start_index + length;
  for (const auto* item : item_list) {
    if (end_index <= character_index)
      break;
    StringView item_text = item->Text(*items);
    unsigned next_character_index = character_index + item_text.length();
    if ((character_index <= start_index &&
         start_index < next_character_index) ||
        (character_index < end_index && end_index <= next_character_index) ||
        (start_index < character_index && next_character_index < end_index)) {
      total_length += InlineSize(
          *item, item_text,
          start_index < character_index ? 0 : start_index - character_index,
          std::min(end_index, next_character_index) - character_index);
    }
    character_index = next_character_index;
  }
  return total_length;
}

FloatPoint NGSvgTextQuery::StartPositionOfCharacter(unsigned index) const {
  const NGFragmentItem* item;
  FloatRect char_rect;
  std::tie(item, char_rect) =
      ScaledCharacterRectInContainer(query_root_, index);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return FloatPoint();
  const auto& inline_text = *To<LayoutSVGInlineText>(item->GetLayoutObject());
  const float ascent =
      inline_text.ScaledFont().PrimaryFont()->GetFontMetrics().FloatAscent(
          item->Style().GetFontBaseline());
  const bool is_ltr = IsLtr(item->ResolvedDirection());
  FloatPoint point;
  if (item->IsHorizontal()) {
    point = is_ltr ? char_rect.Location() : char_rect.MaxXMinYCorner();
    point.Move(0.0f, ascent);
  } else {
    point = is_ltr ? char_rect.MaxXMinYCorner() : char_rect.MaxXMaxYCorner();
    point.Move(-ascent, 0.0f);
  }
  if (item->HasSvgTransformForPaint())
    point = item->BuildSvgTransformForPaint().MapPoint(point);
  const float scaling_factor = inline_text.ScalingFactor();
  point.Scale(1 / scaling_factor, 1 / scaling_factor);
  return point;
}

FloatPoint NGSvgTextQuery::EndPositionOfCharacter(unsigned index) const {
  const NGFragmentItem* item;
  FloatRect char_rect;
  std::tie(item, char_rect) =
      ScaledCharacterRectInContainer(query_root_, index);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return FloatPoint();
  const auto& inline_text = *To<LayoutSVGInlineText>(item->GetLayoutObject());
  const float ascent =
      inline_text.ScaledFont().PrimaryFont()->GetFontMetrics().FloatAscent(
          item->Style().GetFontBaseline());
  const bool is_ltr = IsLtr(item->ResolvedDirection());
  FloatPoint point;
  if (item->IsHorizontal()) {
    point = is_ltr ? char_rect.MaxXMinYCorner() : char_rect.Location();
    point.Move(0.0f, ascent);
  } else {
    point = is_ltr ? char_rect.MaxXMaxYCorner() : char_rect.MaxXMinYCorner();
    point.Move(-ascent, 0.0f);
  }
  if (item->HasSvgTransformForPaint())
    point = item->BuildSvgTransformForPaint().MapPoint(point);
  const float scaling_factor = inline_text.ScalingFactor();
  point.Scale(1 / scaling_factor, 1 / scaling_factor);
  return point;
}

FloatRect NGSvgTextQuery::ExtentOfCharacter(unsigned index) const {
  const NGFragmentItem* item;
  FloatRect char_rect;
  std::tie(item, char_rect) =
      ScaledCharacterRectInContainer(query_root_, index);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return FloatRect();
  if (item->HasSvgTransformForPaint())
    char_rect = item->BuildSvgTransformForPaint().MapRect(char_rect);
  char_rect.Scale(1 / item->SvgScalingFactor());
  return char_rect;
}

float NGSvgTextQuery::RotationOfCharacter(unsigned index) const {
  const NGFragmentItem* item;
  unsigned start_ifc_offset, end_ifc_offset;
  StringView item_text;
  std::tie(item, item_text, start_ifc_offset, end_ifc_offset) =
      FindFragmentItemForAddressableCodeUnitIndex(query_root_, index);
  DCHECK(item);
  DCHECK_EQ(item->Type(), NGFragmentItem::kSvgText);
  if (item->IsHiddenForPaint())
    return 0.0f;
  float rotation = item->SvgFragmentData()->angle;
  if (item->Style().IsHorizontalWritingMode())
    return rotation;
  ETextOrientation orientation = item->Style().GetTextOrientation();
  if (orientation == ETextOrientation::kUpright)
    return rotation;
  if (orientation == ETextOrientation::kSideways)
    return rotation + 90.0f;
  DCHECK_EQ(orientation, ETextOrientation::kMixed);
  CanvasRotationInVertical canvas_rotation;
  // GetCanvasRotation() is called only once because a pair of
  // start_ifc_offset and end_ifc_offset represents a single glyph.
  item->TextShapeResult()->ForEachGlyph(0, start_ifc_offset, end_ifc_offset, 0,
                                        GetCanvasRotation, &canvas_rotation);
  if (IsCanvasRotationInVerticalUpright(canvas_rotation))
    return rotation;
  return rotation + 90.0f;
}

// https://svgwg.org/svg2-draft/text.html#__svg__SVGTextContentElement__getCharNumAtPosition
int NGSvgTextQuery::CharacterNumberAtPosition(
    const FloatPoint& position) const {
  // The specification says we should do hit-testing in logical order.
  // However, this does it in visual order in order to match to the legacy SVG
  // <text> behavior.
  Vector<const NGFragmentItem*> item_list;
  const NGFragmentItems* items;
  std::tie(item_list, items) = FragmentItemsInVisualOrder(query_root_);

  const NGFragmentItem* hit_item = nullptr;
  for (const auto* item : item_list) {
    if (!item->IsHiddenForPaint() && item->Contains(position)) {
      hit_item = item;
      break;
    }
  }
  if (!hit_item)
    return -1;

  // Count code units before |hit_item|.
  std::sort(item_list.begin(), item_list.end(),
            [](const NGFragmentItem* a, const NGFragmentItem* b) {
              return a->StartOffset() < b->StartOffset();
            });
  unsigned addressable_code_unit_count = 0;
  for (const auto* item : item_list) {
    if (item == hit_item)
      break;
    addressable_code_unit_count += item->Text(*items).length();
  }

  PhysicalOffset transformed_point =
      hit_item->MapPointInContainer(
          PhysicalOffset::FromFloatPointRound(position)) -
      hit_item->OffsetInContainerFragment();
  // NGFragmentItem::TextOffsetForPoint() is not suitable here because it
  // returns an offset for the nearest glyph edge.
  unsigned offset_in_item =
      hit_item->TextShapeResult()->CreateShapeResult()->OffsetForPosition(
          hit_item->ScaleInlineOffset(hit_item->IsHorizontal()
                                          ? transformed_point.left
                                          : transformed_point.top),
          BreakGlyphsOption(true));
  return addressable_code_unit_count + offset_in_item;
}

}  // namespace blink
