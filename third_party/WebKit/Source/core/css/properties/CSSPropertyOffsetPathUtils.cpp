// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyOffsetPathUtils.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSRayValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/svg/SVGPathByteStream.h"
#include "core/svg/SVGPathUtilities.h"

namespace blink {

namespace {

CSSValue* ConsumePath(CSSParserTokenRange& range) {
  // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
  if (range.Peek().FunctionId() != CSSValuePath)
    return nullptr;

  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args =
      CSSPropertyParserHelpers::ConsumeFunction(function_range);

  if (function_args.Peek().GetType() != kStringToken)
    return nullptr;
  String path_string =
      function_args.ConsumeIncludingWhitespace().Value().ToString();

  std::unique_ptr<SVGPathByteStream> byte_stream = SVGPathByteStream::Create();
  if (BuildByteStreamFromString(path_string, *byte_stream) !=
          SVGParseStatus::kNoError ||
      !function_args.AtEnd()) {
    return nullptr;
  }

  range = function_range;
  if (byte_stream->IsEmpty())
    return CSSIdentifierValue::Create(CSSValueNone);
  return cssvalue::CSSPathValue::Create(std::move(byte_stream));
}

CSSValue* ConsumeRay(CSSParserTokenRange& range,
                     const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueRay);
  CSSParserTokenRange function_range = range;
  CSSParserTokenRange function_args =
      CSSPropertyParserHelpers::ConsumeFunction(function_range);

  CSSPrimitiveValue* angle = nullptr;
  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* contain = nullptr;
  while (!function_args.AtEnd()) {
    if (!angle) {
      angle = CSSPropertyParserHelpers::ConsumeAngle(
          function_args, &context, WTF::Optional<WebFeature>());
      if (angle)
        continue;
    }
    if (!size) {
      size = CSSPropertyParserHelpers::ConsumeIdent<
          CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide,
          CSSValueFarthestCorner, CSSValueSides>(function_args);
      if (size)
        continue;
    }
    if (RuntimeEnabledFeatures::CSSOffsetPathRayContainEnabled() && !contain) {
      contain = CSSPropertyParserHelpers::ConsumeIdent<CSSValueContain>(
          function_args);
      if (contain)
        continue;
    }
    return nullptr;
  }
  if (!angle || !size)
    return nullptr;
  range = function_range;
  return CSSRayValue::Create(*angle, *size, contain);
}

}  // namespace

CSSValue* CSSPropertyOffsetPathUtils::ConsumeOffsetPath(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* value = nullptr;
  if (RuntimeEnabledFeatures::CSSOffsetPathRayEnabled() &&
      range.Peek().FunctionId() == CSSValueRay)
    value = ConsumeRay(range, context);
  else
    value = ConsumePathOrNone(range);

  // Count when we receive a valid path other than 'none'.
  if (value && !value->IsIdentifierValue())
    context.Count(WebFeature::kCSSOffsetInEffect);
  return value;
}

CSSValue* CSSPropertyOffsetPathUtils::ConsumePathOrNone(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  return ConsumePath(range);
}

}  // namespace blink
