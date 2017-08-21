// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBaseHelper.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"
#include "core/style/GridArea.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace CSSPropertyAPIBaseHelper {

using namespace CSSPropertyParserHelpers;

namespace {

// This is copied from CSSPropertyParser for now. This should go away once we
// finish ribbonizing all the shorthands.
void AddExpandedPropertyForValue(CSSPropertyID property,
                                 const CSSValue& value,
                                 bool important,
                                 HeapVector<CSSProperty, 256>& properties) {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  unsigned shorthand_length = shorthand.length();
  DCHECK(shorthand_length);
  const CSSPropertyID* longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand_length; ++i) {
    AddProperty(longhands[i], shorthand.id(), value, important,
                IsImplicitProperty::kNotImplicit, properties);
  }
}

}  // namespace

CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange& range,
                                       bool allow_text_value) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value =
          ConsumeIdentRange(range, CSSValueBorder, CSSValuePaddingBox))
    return value;
  if (allow_text_value && range.Peek().Id() == CSSValueText)
    return ConsumeIdent(range);
  return nullptr;
}

static CSSValue* ConsumeBackgroundComponent(CSSPropertyID unresolved_property,
                                            CSSParserTokenRange& range,
                                            const CSSParserContext& context) {
  switch (unresolved_property) {
    case CSSPropertyBackgroundClip:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBox(range);
    case CSSPropertyBackgroundBlendMode:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBox(range);
    case CSSPropertyWebkitMaskComposite:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundComposite(range);
    case CSSPropertyMaskSourceType:
      return CSSPropertyBackgroundUtils::ConsumeMaskSourceType(range);
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitMaskClip:
      return ConsumePrefixedBackgroundBox(range, true /* allow_text_value */);
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskOrigin:
      return ConsumePrefixedBackgroundBox(range, false /* allow_text_value */);
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
      return ConsumeImageOrNone(range, &context);
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
    case CSSPropertyAliasWebkitBackgroundSize:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundSize(
          range, context.Mode(), ParsingStyle::kLegacy);
    case CSSPropertyBackgroundColor:
      return ConsumeColor(range, context.Mode());
    default:
      break;
  };
  return nullptr;
}

bool ConsumeBorder(bool important,
                   CSSParserTokenRange& range,
                   const CSSParserContext& context,
                   HeapVector<CSSProperty, 256>& properties) {
  CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  CSSValue* color = nullptr;

  while (!width || !style || !color) {
    if (!width) {
      width = ConsumeLineWidth(range, context.Mode(), UnitlessQuirk::kForbid);
      if (width)
        continue;
    }
    if (!style) {
      style = CSSPropertyParserHelpers::ParseLonghandViaAPI(
          CSSPropertyBorderLeftStyle, CSSPropertyBorder, context, range);
      if (style)
        continue;
    }
    if (!color) {
      color = ConsumeColor(range, context.Mode());
      if (color)
        continue;
    }
    break;
  }

  if (!width && !style && !color)
    return false;

  if (!width)
    width = CSSInitialValue::Create();
  if (!style)
    style = CSSInitialValue::Create();
  if (!color)
    color = CSSInitialValue::Create();

  AddExpandedPropertyForValue(CSSPropertyBorderWidth, *width, important,
                              properties);
  AddExpandedPropertyForValue(CSSPropertyBorderStyle, *style, important,
                              properties);
  AddExpandedPropertyForValue(CSSPropertyBorderColor, *color, important,
                              properties);
  AddExpandedPropertyForValue(CSSPropertyBorderImage,
                              *CSSInitialValue::Create(), important,
                              properties);

  return range.AtEnd();
}

bool ConsumeRepeatStyleComponent(CSSParserTokenRange& range,
                                 CSSValue*& value1,
                                 CSSValue*& value2,
                                 bool& implicit) {
  if (ConsumeIdent<CSSValueRepeatX>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueNoRepeat);
    implicit = true;
    return true;
  }
  if (ConsumeIdent<CSSValueRepeatY>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueNoRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueRepeat);
    implicit = true;
    return true;
  }
  value1 = ConsumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound,
                        CSSValueSpace>(range);
  if (!value1)
    return false;

  value2 = ConsumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound,
                        CSSValueSpace>(range);
  if (!value2) {
    value2 = value1;
    implicit = true;
  }
  return true;
}

// Note: consumeBackgroundShorthand assumes y properties (for example
// background-position-y) follow the x properties in the shorthand array.
bool ConsumeBackgroundShorthand(const StylePropertyShorthand& shorthand,
                                bool important,
                                CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                HeapVector<CSSProperty, 256>& properties) {
  const unsigned longhand_count = shorthand.length();
  CSSValue* longhands[10] = {0};
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
          ConsumeRepeatStyleComponent(range, value, value_y, implicit);
        } else if (property == CSSPropertyBackgroundPositionX ||
                   property == CSSPropertyWebkitMaskPositionX) {
          if (!ConsumePosition(range, context, UnitlessQuirk::kForbid,
                               WebFeature::kThreeValuedPositionBackground,
                               value, value_y))
            continue;
        } else if (property == CSSPropertyBackgroundSize ||
                   property == CSSPropertyWebkitMaskSize) {
          if (!ConsumeSlashIncludingWhitespace(range))
            continue;
          value = CSSPropertyBackgroundUtils::ConsumeBackgroundSize(
              range, context.Mode(), ParsingStyle::kNotLegacy);
          if (!value || !parsed_longhand[i - 1]) {
            // Position must have been parsed in the current layer.
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
              property == CSSPropertyWebkitMaskOrigin)
            origin_value = value;
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
  } while (ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd())
    return false;

  for (size_t i = 0; i < longhand_count; ++i) {
    CSSPropertyID property = shorthand.properties()[i];
    if (property == CSSPropertyBackgroundSize && longhands[i] &&
        context.UseLegacyBackgroundSizeShorthandBehavior())
      continue;
    AddProperty(property, shorthand.id(), *longhands[i], important,
                implicit ? IsImplicitProperty::kImplicit
                         : IsImplicitProperty::kNotImplicit,
                properties);
  }
  return true;
}

CSSValueList* ConsumeImplicitAutoFlow(CSSParserTokenRange& range,
                                      const CSSValue& flow_direction) {
  // [ auto-flow && dense? ]
  CSSValue* dense_algorithm = nullptr;
  if ((ConsumeIdent<CSSValueAutoFlow>(range))) {
    dense_algorithm = ConsumeIdent<CSSValueDense>(range);
  } else {
    dense_algorithm = ConsumeIdent<CSSValueDense>(range);
    if (!dense_algorithm)
      return nullptr;
    if (!ConsumeIdent<CSSValueAutoFlow>(range))
      return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(flow_direction);
  if (dense_algorithm)
    list->Append(*dense_algorithm);
  return list;
}

}  // namespace CSSPropertyAPIBaseHelper

}  // namespace blink
