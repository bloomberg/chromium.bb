// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyOffsetRotateUtils.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

CSSValue* CSSPropertyOffsetRotateUtils::ConsumeOffsetRotate(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* angle = CSSPropertyParserHelpers::ConsumeAngle(
      range, context, Optional<WebFeature>());
  CSSValue* keyword =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto, CSSValueReverse>(
          range);
  if (!angle && !keyword)
    return nullptr;

  if (!angle) {
    angle = CSSPropertyParserHelpers::ConsumeAngle(range, context,
                                                   Optional<WebFeature>());
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (keyword)
    list->Append(*keyword);
  if (angle)
    list->Append(*angle);
  return list;
}

}  // namespace blink
