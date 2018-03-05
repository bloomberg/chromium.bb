// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/BorderImage.h"

#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool BorderImage::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;

  if (!CSSParsingUtils::ConsumeBorderImageComponents(
          range, context, source, slice, width, outset, repeat,
          CSSParsingUtils::DefaultFill::kNoFill)) {
    return false;
  }

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderImageSource, CSSPropertyBorderImage,
      source ? *source : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderImageSlice, CSSPropertyBorderImage,
      slice ? *slice : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderImageWidth, CSSPropertyBorderImage,
      width ? *width : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderImageOutset, CSSPropertyBorderImage,
      outset ? *outset : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderImageRepeat, CSSPropertyBorderImage,
      repeat ? *repeat : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* BorderImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImage(style.BorderImage(), style);
}

}  // namespace CSSShorthand
}  // namespace blink
