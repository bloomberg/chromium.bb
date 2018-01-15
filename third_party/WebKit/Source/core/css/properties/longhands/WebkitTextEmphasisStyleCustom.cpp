// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitTextEmphasisStyle.h"

#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitTextEmphasisStyle::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (CSSValue* text_emphasis_style =
          CSSPropertyParserHelpers::ConsumeString(range))
    return text_emphasis_style;

  CSSIdentifierValue* fill =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueFilled, CSSValueOpen>(
          range);
  CSSIdentifierValue* shape =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueDot, CSSValueCircle,
                                             CSSValueDoubleCircle,
                                             CSSValueTriangle, CSSValueSesame>(
          range);
  if (!fill) {
    fill = CSSPropertyParserHelpers::ConsumeIdent<CSSValueFilled, CSSValueOpen>(
        range);
  }
  if (fill && shape) {
    CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
    parsed_values->Append(*fill);
    parsed_values->Append(*shape);
    return parsed_values;
  }
  if (fill)
    return fill;
  if (shape)
    return shape;
  return nullptr;
}

const CSSValue* WebkitTextEmphasisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  switch (style.GetTextEmphasisMark()) {
    case TextEmphasisMark::kNone:
      return CSSIdentifierValue::Create(CSSValueNone);
    case TextEmphasisMark::kCustom:
      return CSSStringValue::Create(style.TextEmphasisCustomMark());
    case TextEmphasisMark::kAuto:
      NOTREACHED();
    // Fall through
    case TextEmphasisMark::kDot:
    case TextEmphasisMark::kCircle:
    case TextEmphasisMark::kDoubleCircle:
    case TextEmphasisMark::kTriangle:
    case TextEmphasisMark::kSesame: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
      return list;
    }
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace CSSLonghand
}  // namespace blink
