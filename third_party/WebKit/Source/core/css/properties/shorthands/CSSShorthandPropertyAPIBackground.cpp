// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/CSSShorthandPropertyAPIBackground.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"
#include "core/frame/WebFeature.h"

namespace blink {

namespace {

CSSValue* ConsumeBackgroundComponent(CSSPropertyID resolved_property,
                                     CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  switch (resolved_property) {
    case CSSPropertyBackgroundClip:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBox(range);
    case CSSPropertyBackgroundAttachment:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBox(range);
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
      return CSSPropertyParserHelpers::ConsumeImageOrNone(range, &context);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
      return CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueLeft,
                                                               CSSValueRight>(
          range, context.Mode());
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
      return CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueTop,
                                                               CSSValueBottom>(
          range, context.Mode());
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundSize(
          range, context.Mode(), ParsingStyle::kNotLegacy);
    case CSSPropertyBackgroundColor:
      return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
    case CSSPropertyWebkitMaskClip:
      return CSSPropertyBackgroundUtils::ConsumePrefixedBackgroundBox(
          range, AllowTextValue::kAllowed);
    case CSSPropertyWebkitMaskOrigin:
      return CSSPropertyBackgroundUtils::ConsumePrefixedBackgroundBox(
          range, AllowTextValue::kNotAllowed);
    default:
      break;
  };
  return nullptr;
}

}  // namespace

// Note: this assumes y properties (e.g. background-position-y) follow the x
// properties in the shorthand array.
// TODO(jiameng): this API is used by background and -webkit-mask, hence we need
// local_context as an input that contains shorthand id. We will consider remove
// local_context as an input after
//   (i). StylePropertyShorthand is refactored and
//   (ii). we split parsing logic of background and -webkit-mask into
//   different APIs.
bool CSSShorthandPropertyAPIBackground::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSProperty, 256>& properties) const {
  CSSPropertyID shorthand_id = local_context.CurrentShorthand();
  DCHECK(shorthand_id == CSSPropertyBackground ||
         shorthand_id == CSSPropertyWebkitMask);
  const StylePropertyShorthand& shorthand =
      shorthand_id == CSSPropertyBackground ? backgroundShorthand()
                                            : webkitMaskShorthand();

  const unsigned longhand_count = shorthand.length();
  CSSValue* longhands[10] = {nullptr};
  DCHECK_LE(longhand_count, 10u);

  bool implicit = false;
  do {
    bool parsed_longhand[10] = {false};
    CSSValue* origin_value = nullptr;
    do {
      bool found_property = false;
      for (size_t i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i])
          continue;

        CSSValue* value = nullptr;
        CSSValue* value_y = nullptr;
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundRepeatX ||
            property == CSSPropertyWebkitMaskRepeatX) {
          CSSPropertyBackgroundUtils::ConsumeRepeatStyleComponent(
              range, value, value_y, implicit);
        } else if (property == CSSPropertyBackgroundPositionX ||
                   property == CSSPropertyWebkitMaskPositionX) {
          if (!CSSPropertyParserHelpers::ConsumePosition(
                  range, context,
                  CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                  WebFeature::kThreeValuedPositionBackground, value, value_y))
            continue;
        } else if (property == CSSPropertyBackgroundSize ||
                   property == CSSPropertyWebkitMaskSize) {
          if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
            continue;
          value = CSSPropertyBackgroundUtils::ConsumeBackgroundSize(
              range, context.Mode(), ParsingStyle::kNotLegacy);
          if (!value ||
              !parsed_longhand[i - 1])  // Position must have been
                                        // parsed in the current layer.
          {
            return false;
          }
        } else if (property == CSSPropertyBackgroundPositionY ||
                   property == CSSPropertyBackgroundRepeatY ||
                   property == CSSPropertyWebkitMaskPositionY ||
                   property == CSSPropertyWebkitMaskRepeatY) {
          continue;
        } else {
          value = ConsumeBackgroundComponent(property, range, context);
        }
        if (value) {
          if (property == CSSPropertyBackgroundOrigin ||
              property == CSSPropertyWebkitMaskOrigin) {
            origin_value = value;
          }
          parsed_longhand[i] = true;
          found_property = true;
          CSSPropertyBackgroundUtils::AddBackgroundValue(longhands[i], value);
          if (value_y) {
            parsed_longhand[i + 1] = true;
            CSSPropertyBackgroundUtils::AddBackgroundValue(longhands[i + 1],
                                                           value_y);
          }
        }
      }
      if (!found_property)
        return false;
    } while (!range.AtEnd() && range.Peek().GetType() != kCommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (size_t i = 0; i < longhand_count; ++i) {
      CSSPropertyID property = shorthand.properties()[i];
      if (property == CSSPropertyBackgroundColor && !range.AtEnd()) {
        if (parsed_longhand[i])
          return false;  // Colors are only allowed in the last layer.
        continue;
      }
      if ((property == CSSPropertyBackgroundClip ||
           property == CSSPropertyWebkitMaskClip) &&
          !parsed_longhand[i] && origin_value) {
        CSSPropertyBackgroundUtils::AddBackgroundValue(longhands[i],
                                                       origin_value);
        continue;
      }
      if (!parsed_longhand[i]) {
        CSSPropertyBackgroundUtils::AddBackgroundValue(
            longhands[i], CSSInitialValue::Create());
      }
    }
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd())
    return false;

  for (size_t i = 0; i < longhand_count; ++i) {
    CSSPropertyID property = shorthand.properties()[i];
    if (property == CSSPropertyBackgroundSize && longhands[i] &&
        context.UseLegacyBackgroundSizeShorthandBehavior())
      continue;
    CSSPropertyParserHelpers::AddProperty(
        property, shorthand.id(), *longhands[i], important,
        implicit ? CSSPropertyParserHelpers::IsImplicitProperty::kImplicit
                 : CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return true;
}

}  // namespace blink
