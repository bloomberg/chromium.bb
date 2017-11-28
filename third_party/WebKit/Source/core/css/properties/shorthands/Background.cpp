// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/Background.h"

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
namespace CSSShorthand {

// Note: this assumes y properties (e.g. background-position-y) follow the x
// properties in the shorthand array.
// TODO(jiameng): this class is used by background and -webkit-mask, hence we
// need local_context as an input that contains shorthand id. We will consider
// remove local_context as an input after
//   (i). StylePropertyShorthand is refactored and
//   (ii). we split parsing logic of background and -webkit-mask into
//   different property classes.
bool Background::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 256>& properties) const {
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
        const CSSProperty& property = *shorthand.properties()[i];
        if (property.IDEquals(CSSPropertyBackgroundRepeatX) ||
            property.IDEquals(CSSPropertyWebkitMaskRepeatX)) {
          CSSPropertyBackgroundUtils::ConsumeRepeatStyleComponent(
              range, value, value_y, implicit);
        } else if (property.IDEquals(CSSPropertyBackgroundPositionX) ||
                   property.IDEquals(CSSPropertyWebkitMaskPositionX)) {
          if (!CSSPropertyParserHelpers::ConsumePosition(
                  range, context,
                  CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                  WebFeature::kThreeValuedPositionBackground, value, value_y))
            continue;
        } else if (property.IDEquals(CSSPropertyBackgroundSize) ||
                   property.IDEquals(CSSPropertyWebkitMaskSize)) {
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
        } else if (property.IDEquals(CSSPropertyBackgroundPositionY) ||
                   property.IDEquals(CSSPropertyBackgroundRepeatY) ||
                   property.IDEquals(CSSPropertyWebkitMaskPositionY) ||
                   property.IDEquals(CSSPropertyWebkitMaskRepeatY)) {
          continue;
        } else {
          value =
              ConsumeBackgroundComponent(property.PropertyID(), range, context);
        }
        if (value) {
          if (property.IDEquals(CSSPropertyBackgroundOrigin) ||
              property.IDEquals(CSSPropertyWebkitMaskOrigin)) {
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
      const CSSProperty& property = *shorthand.properties()[i];
      if (property.IDEquals(CSSPropertyBackgroundColor) && !range.AtEnd()) {
        if (parsed_longhand[i])
          return false;  // Colors are only allowed in the last layer.
        continue;
      }
      if ((property.IDEquals(CSSPropertyBackgroundClip) ||
           property.IDEquals(CSSPropertyWebkitMaskClip)) &&
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
    const CSSProperty& property = *shorthand.properties()[i];
    if (property.IDEquals(CSSPropertyBackgroundSize) && longhands[i] &&
        context.UseLegacyBackgroundSizeShorthandBehavior())
      continue;
    CSSPropertyParserHelpers::AddProperty(
        property.PropertyID(), shorthand.id(), *longhands[i], important,
        implicit ? CSSPropertyParserHelpers::IsImplicitProperty::kImplicit
                 : CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
