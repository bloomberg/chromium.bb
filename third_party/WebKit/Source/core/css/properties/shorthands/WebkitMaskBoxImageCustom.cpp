// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/WebkitMaskBoxImage.h"

#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool WebkitMaskBoxImage::ParseShorthand(
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
          CSSParsingUtils::DefaultFill::kFill)) {
    return false;
  }

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMaskBoxImageSource, CSSPropertyWebkitMaskBoxImage,
      source ? *source : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMaskBoxImageSlice, CSSPropertyWebkitMaskBoxImage,
      slice ? *slice : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMaskBoxImageWidth, CSSPropertyWebkitMaskBoxImage,
      width ? *width : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMaskBoxImageOutset, CSSPropertyWebkitMaskBoxImage,
      outset ? *outset : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMaskBoxImageRepeat, CSSPropertyWebkitMaskBoxImage,
      repeat ? *repeat : *CSSInitialValue::Create(), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* WebkitMaskBoxImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImage(style.MaskBoxImage(),
                                                    style);
}

}  // namespace CSSShorthand
}  // namespace blink
