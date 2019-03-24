// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/size.h"

#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {
namespace css_longhand {

static CSSValue* ConsumePageSize(CSSParserTokenRange& range) {
  return css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kA3, CSSValueID::kA4, CSSValueID::kA5, CSSValueID::kB4,
      CSSValueID::kB5, CSSValueID::kLedger, CSSValueID::kLegal,
      CSSValueID::kLetter>(range);
}

static float MmToPx(float mm) {
  return mm * kCssPixelsPerMillimeter;
}
static float InchToPx(float inch) {
  return inch * kCssPixelsPerInch;
}
static FloatSize GetPageSizeFromName(const CSSIdentifierValue& page_size_name) {
  switch (page_size_name.GetValueID()) {
    case CSSValueID::kA5:
      return FloatSize(MmToPx(148), MmToPx(210));
    case CSSValueID::kA4:
      return FloatSize(MmToPx(210), MmToPx(297));
    case CSSValueID::kA3:
      return FloatSize(MmToPx(297), MmToPx(420));
    case CSSValueID::kB5:
      return FloatSize(MmToPx(176), MmToPx(250));
    case CSSValueID::kB4:
      return FloatSize(MmToPx(250), MmToPx(353));
    case CSSValueID::kLetter:
      return FloatSize(InchToPx(8.5), InchToPx(11));
    case CSSValueID::kLegal:
      return FloatSize(InchToPx(8.5), InchToPx(14));
    case CSSValueID::kLedger:
      return FloatSize(InchToPx(11), InchToPx(17));
    default:
      NOTREACHED();
      return FloatSize(0, 0);
  }
}

const CSSValue* Size::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();

  if (range.Peek().Id() == CSSValueID::kAuto) {
    result->Append(*css_property_parser_helpers::ConsumeIdent(range));
    return result;
  }

  if (CSSValue* width = css_property_parser_helpers::ConsumeLength(
          range, context.Mode(), kValueRangeNonNegative)) {
    CSSValue* height = css_property_parser_helpers::ConsumeLength(
        range, context.Mode(), kValueRangeNonNegative);
    result->Append(*width);
    if (height)
      result->Append(*height);
    return result;
  }

  CSSValue* page_size = ConsumePageSize(range);
  CSSValue* orientation =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kPortrait,
                                                CSSValueID::kLandscape>(range);
  if (!page_size)
    page_size = ConsumePageSize(range);

  if (!orientation && !page_size)
    return nullptr;
  if (page_size)
    result->Append(*page_size);
  if (orientation)
    result->Append(*orientation);
  return result;
}

void Size::ApplyInitial(StyleResolverState& state) const {}

void Size::ApplyInherit(StyleResolverState& state) const {}

void Size::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  state.Style()->ResetPageSizeType();
  FloatSize size;
  EPageSizeType page_size_type = EPageSizeType::kAuto;
  const auto& list = To<CSSValueList>(value);
  if (list.length() == 2) {
    // <length>{2} | <page-size> <orientation>
    const CSSValue& first = list.Item(0);
    const CSSValue& second = list.Item(1);
    auto* first_primitive_value = DynamicTo<CSSPrimitiveValue>(first);
    if (first_primitive_value && first_primitive_value->IsLength()) {
      // <length>{2}
      size = FloatSize(
          first_primitive_value->ComputeLength<float>(
              state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0)),
          To<CSSPrimitiveValue>(second).ComputeLength<float>(
              state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0)));
    } else {
      // <page-size> <orientation>
      size = GetPageSizeFromName(To<CSSIdentifierValue>(first));

      DCHECK(To<CSSIdentifierValue>(second).GetValueID() ==
                 CSSValueID::kLandscape ||
             To<CSSIdentifierValue>(second).GetValueID() ==
                 CSSValueID::kPortrait);
      if (To<CSSIdentifierValue>(second).GetValueID() == CSSValueID::kLandscape)
        size = size.TransposedSize();
    }
    page_size_type = EPageSizeType::kResolved;
  } else {
    DCHECK_EQ(list.length(), 1U);
    // <length> | auto | <page-size> | [ portrait | landscape]
    const CSSValue& first = list.Item(0);
    auto* first_primitive_value = DynamicTo<CSSPrimitiveValue>(first);
    if (first_primitive_value && first_primitive_value->IsLength()) {
      // <length>
      page_size_type = EPageSizeType::kResolved;
      float width = first_primitive_value->ComputeLength<float>(
          state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0));
      size = FloatSize(width, width);
    } else {
      const auto& ident = To<CSSIdentifierValue>(first);
      switch (ident.GetValueID()) {
        case CSSValueID::kAuto:
          page_size_type = EPageSizeType::kAuto;
          break;
        case CSSValueID::kPortrait:
          page_size_type = EPageSizeType::kPortrait;
          break;
        case CSSValueID::kLandscape:
          page_size_type = EPageSizeType::kLandscape;
          break;
        default:
          // <page-size>
          page_size_type = EPageSizeType::kResolved;
          size = GetPageSizeFromName(ident);
      }
    }
  }
  state.Style()->SetPageSizeType(page_size_type);
  state.Style()->SetPageSize(size);
}

}  // namespace css_longhand
}  // namespace blink
