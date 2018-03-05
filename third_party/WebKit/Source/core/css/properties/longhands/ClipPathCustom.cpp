// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ClipPath.h"

#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSURIValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ClipPath::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  if (CSSURIValue* url = CSSPropertyParserHelpers::ConsumeUrl(range, &context))
    return url;
  return CSSParsingUtils::ConsumeBasicShape(range, context);
}

const CSSValue* ClipPath::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (ClipPathOperation* operation = style.ClipPath()) {
    if (operation->GetType() == ClipPathOperation::SHAPE) {
      return ValueForBasicShape(
          style, ToShapeClipPathOperation(operation)->GetBasicShape());
    }
    if (operation->GetType() == ClipPathOperation::REFERENCE) {
      return CSSURIValue::Create(
          AtomicString(ToReferenceClipPathOperation(operation)->Url()));
    }
  }
  return CSSIdentifierValue::Create(CSSValueNone);
}

}  // namespace CSSLonghand
}  // namespace blink
