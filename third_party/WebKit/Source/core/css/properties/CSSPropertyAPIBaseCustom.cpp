// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPI.h"

#include "core/CSSPropertyNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAPIBaseHelper.h"
#include "core/css/properties/CSSPropertyAnimationTimingFunctionUtils.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"
#include "core/css/properties/CSSPropertyBorderImageUtils.h"
#include "core/css/properties/CSSPropertyBoxShadowUtils.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"
#include "core/css/properties/CSSPropertyMarginUtils.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"
#include "core/css/properties/CSSPropertyTextDecorationLineUtils.h"
#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"

namespace blink {

using namespace CSSPropertyAPIBaseHelper;
using namespace CSSPropertyParserHelpers;

namespace {

bool ConsumeRepeatStyle(CSSParserTokenRange& range,
                        CSSValue*& result_x,
                        CSSValue*& result_y,
                        bool& implicit) {
  do {
    CSSValue* repeat_x = nullptr;
    CSSValue* repeat_y = nullptr;
    if (!ConsumeRepeatStyleComponent(range, repeat_x, repeat_y, implicit))
      return false;
    CSSPropertyBackgroundUtils::AddBackgroundValue(result_x, repeat_x);
    CSSPropertyBackgroundUtils::AddBackgroundValue(result_y, repeat_y);
  } while (ConsumeCommaIncludingWhitespace(range));
  return true;
}

}  // namespace

const CSSValue* CSSPropertyAPI::ParseSingleValue(
    CSSPropertyID property,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  // This is the legacy ParseSingleValue code.
  // TODO(bugsnash): Move all of this to individual CSSPropertyAPI subclasses.
  switch (property) {
    case CSSPropertyTextDecoration:
      DCHECK(!RuntimeEnabledFeatures::CSS3TextDecorationsEnabled());
      return CSSPropertyTextDecorationLineUtils::ConsumeTextDecorationLine(
          range);
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitMaskClip:
      return ConsumeCommaSeparatedList(ConsumePrefixedBackgroundBox, range,
                                       true /* allow_text_value */);
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskOrigin:
      return ConsumeCommaSeparatedList(ConsumePrefixedBackgroundBox, range,
                                       false /* allow_text_value */);
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
      return nullptr;
    default:
      return nullptr;
  }
}

bool CSSPropertyAPI::ParseShorthand(
    CSSPropertyID property,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSProperty, 256>& properties) const {
  // This is the legacy ParseShorthand code.
  // TODO(jiameng): Move all of this to individual CSSPropertyAPI subclasses.
  switch (property) {
    case CSSPropertyBorder:
      return ConsumeBorder(important, range, context, properties);
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
      CSSValue* result_x = nullptr;
      CSSValue* result_y = nullptr;
      bool implicit = false;
      if (!ConsumeRepeatStyle(range, result_x, result_y, implicit) ||
          !range.AtEnd())
        return false;
      IsImplicitProperty enum_implicit = implicit
                                             ? IsImplicitProperty::kImplicit
                                             : IsImplicitProperty::kNotImplicit;
      AddProperty(property == CSSPropertyBackgroundRepeat
                      ? CSSPropertyBackgroundRepeatX
                      : CSSPropertyWebkitMaskRepeatX,
                  property, *result_x, important, enum_implicit, properties);
      AddProperty(property == CSSPropertyBackgroundRepeat
                      ? CSSPropertyBackgroundRepeatY
                      : CSSPropertyWebkitMaskRepeatY,
                  property, *result_y, important, enum_implicit, properties);
      return true;
    }
    case CSSPropertyBackground:
      return ConsumeBackgroundShorthand(backgroundShorthand(), important, range,
                                        context, properties);
    case CSSPropertyWebkitMask:
      return ConsumeBackgroundShorthand(webkitMaskShorthand(), important, range,
                                        context, properties);
    default:
      return false;
  }
}

}  // namespace blink
