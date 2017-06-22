// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIOffset.h"

#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAPIOffsetAnchor.h"
#include "core/css/properties/CSSPropertyAPIOffsetPosition.h"
#include "core/css/properties/CSSPropertyOffsetPathUtils.h"
#include "core/css/properties/CSSPropertyOffsetRotateUtils.h"

namespace blink {

bool CSSShorthandPropertyAPIOffset::parseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  const CSSValue* offset_position =
      CSSPropertyAPIOffsetPosition::parseSingleValue(range, context,
                                                     CSSParserLocalContext());
  const CSSValue* offset_path =
      CSSPropertyOffsetPathUtils::ConsumeOffsetPath(range, context);
  const CSSValue* offset_distance = nullptr;
  const CSSValue* offset_rotate = nullptr;
  if (offset_path) {
    offset_distance = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, context.Mode(), kValueRangeAll);
    offset_rotate =
        CSSPropertyOffsetRotateUtils::ConsumeOffsetRotate(range, context);
    if (offset_rotate && !offset_distance) {
      offset_distance = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          range, context.Mode(), kValueRangeAll);
    }
  }
  const CSSValue* offset_anchor = nullptr;
  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    offset_anchor = CSSPropertyAPIOffsetAnchor::parseSingleValue(
        range, context, CSSParserLocalContext());
    if (!offset_anchor)
      return false;
  }
  if ((!offset_position && !offset_path) || !range.AtEnd())
    return false;

  if ((offset_position || offset_anchor) &&
      !RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled())
    return false;

  if (offset_position) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetPosition, CSSPropertyOffset, *offset_position,
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  } else if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetPosition, CSSPropertyOffset,
        *CSSInitialValue::Create(), important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  }

  if (offset_path) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetPath, CSSPropertyOffset, *offset_path, important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  } else {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetPath, CSSPropertyOffset, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }

  if (offset_distance) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetDistance, CSSPropertyOffset, *offset_distance,
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  } else {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetDistance, CSSPropertyOffset,
        *CSSInitialValue::Create(), important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  }

  if (offset_rotate) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetRotate, CSSPropertyOffset, *offset_rotate, important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  } else {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetRotate, CSSPropertyOffset, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }

  if (offset_anchor) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetAnchor, CSSPropertyOffset, *offset_anchor, important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  } else if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyOffsetAnchor, CSSPropertyOffset, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }

  return true;
}

}  // namespace blink
