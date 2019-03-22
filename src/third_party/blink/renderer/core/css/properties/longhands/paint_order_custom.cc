// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/paint_order.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* PaintOrder::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNormal)
    return css_property_parser_helpers::ConsumeIdent(range);

  Vector<CSSValueID, 3> paint_type_list;
  CSSIdentifierValue* fill = nullptr;
  CSSIdentifierValue* stroke = nullptr;
  CSSIdentifierValue* markers = nullptr;
  do {
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueFill && !fill)
      fill = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueStroke && !stroke)
      stroke = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueMarkers && !markers)
      markers = css_property_parser_helpers::ConsumeIdent(range);
    else
      return nullptr;
    paint_type_list.push_back(id);
  } while (!range.AtEnd());

  // After parsing we serialize the paint-order list. Since it is not possible
  // to pop a last list items from CSSValueList without bigger cost, we create
  // the list after parsing.
  CSSValueID first_paint_order_type = paint_type_list.at(0);
  CSSValueList* paint_order_list = CSSValueList::CreateSpaceSeparated();
  switch (first_paint_order_type) {
    case CSSValueFill:
    case CSSValueStroke:
      paint_order_list->Append(
          first_paint_order_type == CSSValueFill ? *fill : *stroke);
      if (paint_type_list.size() > 1) {
        if (paint_type_list.at(1) == CSSValueMarkers)
          paint_order_list->Append(*markers);
      }
      break;
    case CSSValueMarkers:
      paint_order_list->Append(*markers);
      if (paint_type_list.size() > 1) {
        if (paint_type_list.at(1) == CSSValueStroke)
          paint_order_list->Append(*stroke);
      }
      break;
    default:
      NOTREACHED();
  }

  return paint_order_list;
}

const CSSValue* PaintOrder::CSSValueFromComputedStyleInternal(
    const ComputedStyle&,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  const EPaintOrder paint_order = svg_style.PaintOrder();
  if (paint_order == kPaintOrderNormal)
    return CSSIdentifierValue::Create(CSSValueNormal);

  // Table mapping to the shortest (canonical) form of the property.
  //
  // Per spec, if any keyword is omitted it will be added last using
  // the standard ordering. So "stroke" implies an order "stroke fill
  // markers" etc. From a serialization PoV this means we never need
  // to emit the last keyword.
  //
  // https://svgwg.org/svg2-draft/painting.html#PaintOrder
  static const uint8_t canonical_form[][2] = {
      // kPaintOrderNormal is handled above.
      {PT_FILL, PT_NONE},       // kPaintOrderFillStrokeMarkers
      {PT_FILL, PT_MARKERS},    // kPaintOrderFillMarkersStroke
      {PT_STROKE, PT_NONE},     // kPaintOrderStrokeFillMarkers
      {PT_STROKE, PT_MARKERS},  // kPaintOrderStrokeMarkersFill
      {PT_MARKERS, PT_NONE},    // kPaintOrderMarkersFillStroke
      {PT_MARKERS, PT_STROKE},  // kPaintOrderMarkersStrokeFill
  };
  DCHECK_LT(static_cast<size_t>(paint_order) - 1, base::size(canonical_form));
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& keyword : canonical_form[paint_order - 1]) {
    const auto paint_order_type = static_cast<EPaintOrderType>(keyword);
    if (paint_order_type == PT_NONE)
      break;
    list->Append(*CSSIdentifierValue::Create(paint_order_type));
  }
  return list;
}

}  // namespace css_longhand
}  // namespace blink
