// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/Offset.h"

#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/css/properties/Longhand.h"
#include "core/style/ComputedStyle.h"
#include "platform/runtime_enabled_features.h"

namespace blink {
namespace CSSShorthand {

bool Offset::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  // TODO(meade): The propertyID parameter isn't used - it can be removed
  // once all of the ParseSingleValue implementations have been moved to the
  // CSSPropertys, and the base CSSProperty::ParseSingleValue contains
  // no functionality.
  const CSSValue* offset_position =
      ToLonghand(GetCSSPropertyOffsetPosition())
          .ParseSingleValue(range, context, CSSParserLocalContext());
  const CSSValue* offset_path =
      CSSParsingUtils::ConsumeOffsetPath(range, context);
  const CSSValue* offset_distance = nullptr;
  const CSSValue* offset_rotate = nullptr;
  if (offset_path) {
    offset_distance = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, context.Mode(), kValueRangeAll);
    offset_rotate = CSSParsingUtils::ConsumeOffsetRotate(range, context);
    if (offset_rotate && !offset_distance) {
      offset_distance = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          range, context.Mode(), kValueRangeAll);
    }
  }
  const CSSValue* offset_anchor = nullptr;
  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    offset_anchor =
        ToLonghand(GetCSSPropertyOffsetAnchor())
            .ParseSingleValue(range, context, CSSParserLocalContext());
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

const CSSValue* Offset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForOffset(style, layout_object, styled_node,
                                            allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
