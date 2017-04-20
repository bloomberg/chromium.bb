// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyOffsetPathUtils.h"

#include "core/css/CSSPathValue.h"
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
  return CSSPathValue::Create(std::move(byte_stream));
}

}  // namespace

CSSValue* CSSPropertyOffsetPathUtils::ConsumeOffsetPath(
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    bool is_motion_path) {
  CSSValue* value = ConsumePathOrNone(range);

  // Count when we receive a valid path other than 'none'.
  if (value && !value->IsIdentifierValue()) {
    if (is_motion_path) {
      context->Count(UseCounter::kCSSMotionInEffect);
    } else {
      context->Count(UseCounter::kCSSOffsetInEffect);
    }
  }
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
