// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParser.h"

#include <memory>
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontVariationValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/FontFace.h"
#include "core/css/HashTools.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/parser/FontVariantLigaturesParser.h"
#include "core/css/parser/FontVariantNumericParser.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "core/css/properties/CSSPropertyColumnUtils.h"
#include "core/css/properties/CSSPropertyDescriptor.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"
#include "core/css/properties/CSSPropertyMarginUtils.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"
#include "core/css/properties/CSSPropertyShapeUtils.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutTheme.h"
#include "core/svg/SVGPathUtilities.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

CSSPropertyParser::CSSPropertyParser(
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSProperty, 256>* parsedProperties)
    : m_range(range), m_context(context), m_parsedProperties(parsedProperties) {
  m_range.consumeWhitespace();
}

void CSSPropertyParser::addProperty(CSSPropertyID property,
                                    CSSPropertyID currentShorthand,
                                    const CSSValue& value,
                                    bool important,
                                    bool implicit) {
  ASSERT(!isPropertyAlias(property));

  int shorthandIndex = 0;
  bool setFromShorthand = false;

  if (currentShorthand) {
    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(property, &shorthands);
    setFromShorthand = true;
    if (shorthands.size() > 1)
      shorthandIndex =
          indexOfShorthandForLonghand(currentShorthand, shorthands);
  }

  m_parsedProperties->push_back(CSSProperty(
      property, value, important, setFromShorthand, shorthandIndex, implicit));
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID property,
                                                    const CSSValue& value,
                                                    bool important) {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  unsigned shorthandLength = shorthand.length();
  ASSERT(shorthandLength);
  const CSSPropertyID* longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthandLength; ++i)
    addProperty(longhands[i], property, value, important);
}

bool CSSPropertyParser::parseValue(
    CSSPropertyID unresolvedProperty,
    bool important,
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSProperty, 256>& parsedProperties,
    StyleRule::RuleType ruleType) {
  int parsedPropertiesSize = parsedProperties.size();

  CSSPropertyParser parser(range, context, &parsedProperties);
  CSSPropertyID resolvedProperty = resolveCSSPropertyID(unresolvedProperty);
  bool parseSuccess;

  if (ruleType == StyleRule::Viewport) {
    parseSuccess = (RuntimeEnabledFeatures::cssViewportEnabled() ||
                    isUASheetBehavior(context->mode())) &&
                   parser.parseViewportDescriptor(resolvedProperty, important);
  } else if (ruleType == StyleRule::FontFace) {
    parseSuccess = parser.parseFontFaceDescriptor(resolvedProperty);
  } else {
    parseSuccess = parser.parseValueStart(unresolvedProperty, important);
  }

  // This doesn't count UA style sheets
  if (parseSuccess)
    context->count(context->mode(), unresolvedProperty);

  if (!parseSuccess)
    parsedProperties.shrink(parsedPropertiesSize);

  return parseSuccess;
}

const CSSValue* CSSPropertyParser::parseSingleValue(
    CSSPropertyID property,
    const CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSPropertyParser parser(range, context, nullptr);
  const CSSValue* value = parser.parseSingleValue(property);
  if (!value || !parser.m_range.atEnd())
    return nullptr;
  return value;
}

bool CSSPropertyParser::parseValueStart(CSSPropertyID unresolvedProperty,
                                        bool important) {
  if (consumeCSSWideKeyword(unresolvedProperty, important))
    return true;

  CSSParserTokenRange originalRange = m_range;
  CSSPropertyID propertyId = resolveCSSPropertyID(unresolvedProperty);
  bool isShorthand = isShorthandProperty(propertyId);

  if (isShorthand) {
    // Variable references will fail to parse here and will fall out to the
    // variable ref parser below.
    if (parseShorthand(unresolvedProperty, important))
      return true;
  } else {
    if (const CSSValue* parsedValue = parseSingleValue(unresolvedProperty)) {
      if (m_range.atEnd()) {
        addProperty(propertyId, CSSPropertyInvalid, *parsedValue, important);
        return true;
      }
    }
  }

  if (CSSVariableParser::containsValidVariableReferences(originalRange)) {
    bool isAnimationTainted = false;
    CSSVariableReferenceValue* variable = CSSVariableReferenceValue::create(
        CSSVariableData::create(originalRange, isAnimationTainted, true));

    if (isShorthand) {
      const CSSPendingSubstitutionValue& pendingValue =
          *CSSPendingSubstitutionValue::create(propertyId, variable);
      addExpandedPropertyForValue(propertyId, pendingValue, important);
    } else {
      addProperty(propertyId, CSSPropertyInvalid, *variable, important);
    }
    return true;
  }

  return false;
}

template <typename CharacterType>
static CSSPropertyID unresolvedCSSPropertyID(const CharacterType* propertyName,
                                             unsigned length) {
  if (length == 0)
    return CSSPropertyInvalid;
  if (length >= 2 && propertyName[0] == '-' && propertyName[1] == '-')
    return CSSPropertyVariable;
  if (length > maxCSSPropertyNameLength)
    return CSSPropertyInvalid;

  char buffer[maxCSSPropertyNameLength + 1];  // 1 for null character

  for (unsigned i = 0; i != length; ++i) {
    CharacterType c = propertyName[i];
    if (c == 0 || c >= 0x7F)
      return CSSPropertyInvalid;  // illegal character
    buffer[i] = toASCIILower(c);
  }
  buffer[length] = '\0';

  const char* name = buffer;
  const Property* hashTableEntry = findProperty(name, length);
  if (!hashTableEntry)
    return CSSPropertyInvalid;
  CSSPropertyID property = static_cast<CSSPropertyID>(hashTableEntry->id);
  if (!CSSPropertyMetadata::isEnabledProperty(property))
    return CSSPropertyInvalid;
  return property;
}

CSSPropertyID unresolvedCSSPropertyID(const String& string) {
  unsigned length = string.length();
  return string.is8Bit()
             ? unresolvedCSSPropertyID(string.characters8(), length)
             : unresolvedCSSPropertyID(string.characters16(), length);
}

CSSPropertyID unresolvedCSSPropertyID(StringView string) {
  unsigned length = string.length();
  return string.is8Bit()
             ? unresolvedCSSPropertyID(string.characters8(), length)
             : unresolvedCSSPropertyID(string.characters16(), length);
}

template <typename CharacterType>
static CSSValueID cssValueKeywordID(const CharacterType* valueKeyword,
                                    unsigned length) {
  char buffer[maxCSSValueKeywordLength + 1];  // 1 for null character

  for (unsigned i = 0; i != length; ++i) {
    CharacterType c = valueKeyword[i];
    if (c == 0 || c >= 0x7F)
      return CSSValueInvalid;  // illegal character
    buffer[i] = WTF::toASCIILower(c);
  }
  buffer[length] = '\0';

  const Value* hashTableEntry = findValue(buffer, length);
  return hashTableEntry ? static_cast<CSSValueID>(hashTableEntry->id)
                        : CSSValueInvalid;
}

CSSValueID cssValueKeywordID(StringView string) {
  unsigned length = string.length();
  if (!length)
    return CSSValueInvalid;
  if (length > maxCSSValueKeywordLength)
    return CSSValueInvalid;

  return string.is8Bit() ? cssValueKeywordID(string.characters8(), length)
                         : cssValueKeywordID(string.characters16(), length);
}

bool CSSPropertyParser::consumeCSSWideKeyword(CSSPropertyID unresolvedProperty,
                                              bool important) {
  CSSParserTokenRange rangeCopy = m_range;
  CSSValueID id = rangeCopy.consumeIncludingWhitespace().id();
  if (!rangeCopy.atEnd())
    return false;

  CSSValue* value = nullptr;
  if (id == CSSValueInitial)
    value = CSSInitialValue::create();
  else if (id == CSSValueInherit)
    value = CSSInheritedValue::create();
  else if (id == CSSValueUnset)
    value = CSSUnsetValue::create();
  else
    return false;

  CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  if (!shorthand.length()) {
    if (CSSPropertyMetadata::isDescriptorOnly(unresolvedProperty))
      return false;
    addProperty(property, CSSPropertyInvalid, *value, important);
  } else {
    addExpandedPropertyForValue(property, *value, important);
  }
  m_range = rangeCopy;
  return true;
}

static CSSFontFeatureValue* consumeFontFeatureTag(CSSParserTokenRange& range) {
  // Feature tag name consists of 4-letter characters.
  static const unsigned tagNameLength = 4;

  const CSSParserToken& token = range.consumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.type() != StringToken)
    return nullptr;
  if (token.value().length() != tagNameLength)
    return nullptr;
  AtomicString tag = token.value().toAtomicString();
  for (unsigned i = 0; i < tagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E)
      return nullptr;
  }

  int tagValue = 1;
  // Feature tag values could follow: <integer> | on | off
  if (range.peek().type() == NumberToken &&
      range.peek().numericValueType() == IntegerValueType &&
      range.peek().numericValue() >= 0) {
    tagValue = clampTo<int>(range.consumeIncludingWhitespace().numericValue());
    if (tagValue < 0)
      return nullptr;
  } else if (range.peek().id() == CSSValueOn ||
             range.peek().id() == CSSValueOff) {
    tagValue = range.consumeIncludingWhitespace().id() == CSSValueOn;
  }
  return CSSFontFeatureValue::create(tag, tagValue);
}

static CSSValue* consumeFontFeatureSettings(CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueNormal)
    return consumeIdent(range);
  CSSValueList* settings = CSSValueList::createCommaSeparated();
  do {
    CSSFontFeatureValue* fontFeatureValue = consumeFontFeatureTag(range);
    if (!fontFeatureValue)
      return nullptr;
    settings->append(*fontFeatureValue);
  } while (consumeCommaIncludingWhitespace(range));
  return settings;
}

static CSSIdentifierValue* consumeFontVariantCSS21(CSSParserTokenRange& range) {
  return consumeIdent<CSSValueNormal, CSSValueSmallCaps>(range);
}

static CSSValue* consumeFontVariantList(CSSParserTokenRange& range) {
  CSSValueList* values = CSSValueList::createCommaSeparated();
  do {
    if (range.peek().id() == CSSValueAll) {
      // FIXME: CSSPropertyParser::parseFontVariant() implements
      // the old css3 draft:
      // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
      // 'all' is only allowed in @font-face and with no other values.
      if (values->length())
        return nullptr;
      return consumeIdent(range);
    }
    CSSIdentifierValue* fontVariant = consumeFontVariantCSS21(range);
    if (fontVariant)
      values->append(*fontVariant);
  } while (consumeCommaIncludingWhitespace(range));

  if (values->length())
    return values;

  return nullptr;
}

static CSSIdentifierValue* consumeFontWeight(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.peek();
  if (token.id() >= CSSValueNormal && token.id() <= CSSValueLighter)
    return consumeIdent(range);
  if (token.type() != NumberToken ||
      token.numericValueType() != IntegerValueType)
    return nullptr;
  int weight = static_cast<int>(token.numericValue());
  if ((weight % 100) || weight < 100 || weight > 900)
    return nullptr;
  range.consumeIncludingWhitespace();
  return CSSIdentifierValue::create(
      static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1));
}

static String concatenateFamilyName(CSSParserTokenRange& range) {
  StringBuilder builder;
  bool addedSpace = false;
  const CSSParserToken& firstToken = range.peek();
  while (range.peek().type() == IdentToken) {
    if (!builder.isEmpty()) {
      builder.append(' ');
      addedSpace = true;
    }
    builder.append(range.consumeIncludingWhitespace().value());
  }
  if (!addedSpace && isCSSWideKeyword(firstToken.value()))
    return String();
  return builder.toString();
}

static CSSValue* consumeFamilyName(CSSParserTokenRange& range) {
  if (range.peek().type() == StringToken)
    return CSSFontFamilyValue::create(
        range.consumeIncludingWhitespace().value().toString());
  if (range.peek().type() != IdentToken)
    return nullptr;
  String familyName = concatenateFamilyName(range);
  if (familyName.isNull())
    return nullptr;
  return CSSFontFamilyValue::create(familyName);
}

static CSSValue* consumeGenericFamily(CSSParserTokenRange& range) {
  return consumeIdentRange(range, CSSValueSerif, CSSValueWebkitBody);
}

static CSSValueList* consumeFontFamily(CSSParserTokenRange& range) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  do {
    CSSValue* parsedValue = consumeGenericFamily(range);
    if (parsedValue) {
      list->append(*parsedValue);
    } else {
      parsedValue = consumeFamilyName(range);
      if (parsedValue) {
        list->append(*parsedValue);
      } else {
        return nullptr;
      }
    }
  } while (consumeCommaIncludingWhitespace(range));
  return list;
}

static CSSValue* consumeFontSize(
    CSSParserTokenRange& range,
    CSSParserMode cssParserMode,
    UnitlessQuirk unitless = UnitlessQuirk::Forbid) {
  if (range.peek().id() >= CSSValueXxSmall &&
      range.peek().id() <= CSSValueLarger)
    return consumeIdent(range);
  return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative,
                                unitless);
}

static CSSValue* consumeLineHeight(CSSParserTokenRange& range,
                                   CSSParserMode cssParserMode) {
  if (range.peek().id() == CSSValueNormal)
    return consumeIdent(range);

  CSSPrimitiveValue* lineHeight = consumeNumber(range, ValueRangeNonNegative);
  if (lineHeight)
    return lineHeight;
  return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
}

static CSSValue* consumeCounter(CSSParserTokenRange& range, int defaultValue) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  do {
    CSSCustomIdentValue* counterName = consumeCustomIdent(range);
    if (!counterName)
      return nullptr;
    int i = defaultValue;
    if (CSSPrimitiveValue* counterValue = consumeInteger(range))
      i = clampTo<int>(counterValue->getDoubleValue());
    list->append(*CSSValuePair::create(
        counterName,
        CSSPrimitiveValue::create(i, CSSPrimitiveValue::UnitType::Integer),
        CSSValuePair::DropIdenticalValues));
  } while (!range.atEnd());
  return list;
}

static CSSValue* consumeLocale(CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueAuto)
    return consumeIdent(range);
  return consumeString(range);
}

static CSSValue* consumeAnimationIterationCount(CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueInfinite)
    return consumeIdent(range);
  return consumeNumber(range, ValueRangeNonNegative);
}

static CSSValue* consumeAnimationName(CSSParserTokenRange& range,
                                      const CSSParserContext* context,
                                      bool allowQuotedName) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);

  if (allowQuotedName && range.peek().type() == StringToken) {
    // Legacy support for strings in prefixed animations.
    context->count(UseCounter::QuotedAnimationName);

    const CSSParserToken& token = range.consumeIncludingWhitespace();
    if (equalIgnoringASCIICase(token.value(), "none"))
      return CSSIdentifierValue::create(CSSValueNone);
    return CSSCustomIdentValue::create(token.value().toAtomicString());
  }

  return consumeCustomIdent(range);
}

static CSSValue* consumeTransitionProperty(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.peek();
  if (token.type() != IdentToken)
    return nullptr;
  if (token.id() == CSSValueNone)
    return consumeIdent(range);

  CSSPropertyID unresolvedProperty = token.parseAsUnresolvedCSSPropertyID();
  if (unresolvedProperty != CSSPropertyInvalid &&
      unresolvedProperty != CSSPropertyVariable) {
    DCHECK(CSSPropertyMetadata::isEnabledProperty(unresolvedProperty));
    range.consumeIncludingWhitespace();
    return CSSCustomIdentValue::create(unresolvedProperty);
  }
  return consumeCustomIdent(range);
}

static CSSValue* consumeSteps(CSSParserTokenRange& range) {
  ASSERT(range.peek().functionId() == CSSValueSteps);
  CSSParserTokenRange rangeCopy = range;
  CSSParserTokenRange args = consumeFunction(rangeCopy);

  CSSPrimitiveValue* steps = consumePositiveInteger(args);
  if (!steps)
    return nullptr;

  StepsTimingFunction::StepPosition position =
      StepsTimingFunction::StepPosition::END;
  if (consumeCommaIncludingWhitespace(args)) {
    switch (args.consumeIncludingWhitespace().id()) {
      case CSSValueMiddle:
        if (!RuntimeEnabledFeatures::webAnimationsAPIEnabled())
          return nullptr;
        position = StepsTimingFunction::StepPosition::MIDDLE;
        break;
      case CSSValueStart:
        position = StepsTimingFunction::StepPosition::START;
        break;
      case CSSValueEnd:
        position = StepsTimingFunction::StepPosition::END;
        break;
      default:
        return nullptr;
    }
  }

  if (!args.atEnd())
    return nullptr;

  range = rangeCopy;
  return CSSStepsTimingFunctionValue::create(steps->getIntValue(), position);
}

static CSSValue* consumeCubicBezier(CSSParserTokenRange& range) {
  ASSERT(range.peek().functionId() == CSSValueCubicBezier);
  CSSParserTokenRange rangeCopy = range;
  CSSParserTokenRange args = consumeFunction(rangeCopy);

  double x1, y1, x2, y2;
  if (consumeNumberRaw(args, x1) && x1 >= 0 && x1 <= 1 &&
      consumeCommaIncludingWhitespace(args) && consumeNumberRaw(args, y1) &&
      consumeCommaIncludingWhitespace(args) && consumeNumberRaw(args, x2) &&
      x2 >= 0 && x2 <= 1 && consumeCommaIncludingWhitespace(args) &&
      consumeNumberRaw(args, y2) && args.atEnd()) {
    range = rangeCopy;
    return CSSCubicBezierTimingFunctionValue::create(x1, y1, x2, y2);
  }

  return nullptr;
}

static CSSValue* consumeAnimationTimingFunction(CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueEase || id == CSSValueLinear || id == CSSValueEaseIn ||
      id == CSSValueEaseOut || id == CSSValueEaseInOut ||
      id == CSSValueStepStart || id == CSSValueStepEnd ||
      id == CSSValueStepMiddle)
    return consumeIdent(range);

  CSSValueID function = range.peek().functionId();
  if (function == CSSValueSteps)
    return consumeSteps(range);
  if (function == CSSValueCubicBezier)
    return consumeCubicBezier(range);
  return nullptr;
}

static CSSValue* consumeAnimationValue(CSSPropertyID property,
                                       CSSParserTokenRange& range,
                                       const CSSParserContext* context,
                                       bool useLegacyParsing) {
  switch (property) {
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
      return consumeTime(range, ValueRangeAll);
    case CSSPropertyAnimationDirection:
      return consumeIdent<CSSValueNormal, CSSValueAlternate, CSSValueReverse,
                          CSSValueAlternateReverse>(range);
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
      return consumeTime(range, ValueRangeNonNegative);
    case CSSPropertyAnimationFillMode:
      return consumeIdent<CSSValueNone, CSSValueForwards, CSSValueBackwards,
                          CSSValueBoth>(range);
    case CSSPropertyAnimationIterationCount:
      return consumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
      return consumeAnimationName(range, context, useLegacyParsing);
    case CSSPropertyAnimationPlayState:
      return consumeIdent<CSSValueRunning, CSSValuePaused>(range);
    case CSSPropertyTransitionProperty:
      return consumeTransitionProperty(range);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
      return consumeAnimationTimingFunction(range);
    default:
      ASSERT_NOT_REACHED();
      return nullptr;
  }
}

static bool isValidAnimationPropertyList(CSSPropertyID property,
                                         const CSSValueList& valueList) {
  if (property != CSSPropertyTransitionProperty || valueList.length() < 2)
    return true;
  for (auto& value : valueList) {
    if (value->isIdentifierValue() &&
        toCSSIdentifierValue(*value).getValueID() == CSSValueNone)
      return false;
  }
  return true;
}

static CSSValueList* consumeAnimationPropertyList(
    CSSPropertyID property,
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    bool useLegacyParsing) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  do {
    CSSValue* value =
        consumeAnimationValue(property, range, context, useLegacyParsing);
    if (!value)
      return nullptr;
    list->append(*value);
  } while (consumeCommaIncludingWhitespace(range));
  if (!isValidAnimationPropertyList(property, *list))
    return nullptr;
  ASSERT(list->length());
  return list;
}

bool CSSPropertyParser::consumeAnimationShorthand(
    const StylePropertyShorthand& shorthand,
    bool useLegacyParsing,
    bool important) {
  const unsigned longhandCount = shorthand.length();
  CSSValueList* longhands[8];
  ASSERT(longhandCount <= 8);
  for (size_t i = 0; i < longhandCount; ++i)
    longhands[i] = CSSValueList::createCommaSeparated();

  do {
    bool parsedLonghand[8] = {false};
    do {
      bool foundProperty = false;
      for (size_t i = 0; i < longhandCount; ++i) {
        if (parsedLonghand[i])
          continue;

        if (CSSValue* value =
                consumeAnimationValue(shorthand.properties()[i], m_range,
                                      m_context, useLegacyParsing)) {
          parsedLonghand[i] = true;
          foundProperty = true;
          longhands[i]->append(*value);
          break;
        }
      }
      if (!foundProperty)
        return false;
    } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (size_t i = 0; i < longhandCount; ++i) {
      if (!parsedLonghand[i])
        longhands[i]->append(*CSSInitialValue::create());
      parsedLonghand[i] = false;
    }
  } while (consumeCommaIncludingWhitespace(m_range));

  for (size_t i = 0; i < longhandCount; ++i) {
    if (!isValidAnimationPropertyList(shorthand.properties()[i], *longhands[i]))
      return false;
  }

  for (size_t i = 0; i < longhandCount; ++i)
    addProperty(shorthand.properties()[i], shorthand.id(), *longhands[i],
                important);

  return m_range.atEnd();
}

static CSSShadowValue* parseSingleShadow(CSSParserTokenRange& range,
                                         CSSParserMode cssParserMode,
                                         bool allowInset,
                                         bool allowSpread) {
  CSSIdentifierValue* style = nullptr;
  CSSValue* color = nullptr;

  if (range.atEnd())
    return nullptr;
  if (range.peek().id() == CSSValueInset) {
    if (!allowInset)
      return nullptr;
    style = consumeIdent(range);
  }
  color = consumeColor(range, cssParserMode);

  CSSPrimitiveValue* horizontalOffset =
      consumeLength(range, cssParserMode, ValueRangeAll);
  if (!horizontalOffset)
    return nullptr;

  CSSPrimitiveValue* verticalOffset =
      consumeLength(range, cssParserMode, ValueRangeAll);
  if (!verticalOffset)
    return nullptr;

  CSSPrimitiveValue* blurRadius =
      consumeLength(range, cssParserMode, ValueRangeAll);
  CSSPrimitiveValue* spreadDistance = nullptr;
  if (blurRadius) {
    // Blur radius must be non-negative.
    if (blurRadius->getDoubleValue() < 0)
      return nullptr;
    if (allowSpread)
      spreadDistance = consumeLength(range, cssParserMode, ValueRangeAll);
  }

  if (!range.atEnd()) {
    if (!color)
      color = consumeColor(range, cssParserMode);
    if (range.peek().id() == CSSValueInset) {
      if (!allowInset || style)
        return nullptr;
      style = consumeIdent(range);
    }
  }
  return CSSShadowValue::create(horizontalOffset, verticalOffset, blurRadius,
                                spreadDistance, style, color);
}

static CSSValue* consumeShadow(CSSParserTokenRange& range,
                               CSSParserMode cssParserMode,
                               bool isBoxShadowProperty) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);

  CSSValueList* shadowValueList = CSSValueList::createCommaSeparated();
  do {
    if (CSSShadowValue* shadowValue = parseSingleShadow(
            range, cssParserMode, isBoxShadowProperty, isBoxShadowProperty))
      shadowValueList->append(*shadowValue);
    else
      return nullptr;
  } while (consumeCommaIncludingWhitespace(range));
  return shadowValueList;
}

static CSSFunctionValue* consumeFilterFunction(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueID filterType = range.peek().functionId();
  if (filterType < CSSValueInvert || filterType > CSSValueDropShadow)
    return nullptr;
  CSSParserTokenRange args = consumeFunction(range);
  CSSFunctionValue* filterValue = CSSFunctionValue::create(filterType);
  CSSValue* parsedValue = nullptr;

  if (filterType == CSSValueDropShadow) {
    parsedValue = parseSingleShadow(args, context->mode(), false, false);
  } else {
    if (args.atEnd()) {
      context->count(UseCounter::CSSFilterFunctionNoArguments);
      return filterValue;
    }
    if (filterType == CSSValueBrightness) {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsedValue = consumePercent(args, ValueRangeAll);
      if (!parsedValue)
        parsedValue = consumeNumber(args, ValueRangeAll);
    } else if (filterType == CSSValueHueRotate) {
      parsedValue = consumeAngle(args);
    } else if (filterType == CSSValueBlur) {
      parsedValue =
          consumeLength(args, HTMLStandardMode, ValueRangeNonNegative);
    } else {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsedValue = consumePercent(args, ValueRangeNonNegative);
      if (!parsedValue)
        parsedValue = consumeNumber(args, ValueRangeNonNegative);
      if (parsedValue && filterType != CSSValueSaturate &&
          filterType != CSSValueContrast) {
        bool isPercentage = toCSSPrimitiveValue(parsedValue)->isPercentage();
        double maxAllowed = isPercentage ? 100.0 : 1.0;
        if (toCSSPrimitiveValue(parsedValue)->getDoubleValue() > maxAllowed) {
          parsedValue = CSSPrimitiveValue::create(
              maxAllowed, isPercentage ? CSSPrimitiveValue::UnitType::Percentage
                                       : CSSPrimitiveValue::UnitType::Number);
        }
      }
    }
  }
  if (!parsedValue || !args.atEnd())
    return nullptr;
  filterValue->append(*parsedValue);
  return filterValue;
}

static CSSValue* consumeFilter(CSSParserTokenRange& range,
                               const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  do {
    CSSValue* filterValue = consumeUrl(range, context);
    if (!filterValue) {
      filterValue = consumeFilterFunction(range, context);
      if (!filterValue)
        return nullptr;
    }
    list->append(*filterValue);
  } while (!range.atEnd());
  return list;
}

static CSSValue* consumeTextDecorationLine(CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  while (true) {
    CSSIdentifierValue* ident =
        consumeIdent<CSSValueBlink, CSSValueUnderline, CSSValueOverline,
                     CSSValueLineThrough>(range);
    if (!ident)
      break;
    if (list->hasValue(*ident))
      return nullptr;
    list->append(*ident);
  }

  if (!list->length())
    return nullptr;
  return list;
}

static CSSValue* consumePath(CSSParserTokenRange& range) {
  // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
  if (range.peek().functionId() != CSSValuePath)
    return nullptr;

  CSSParserTokenRange functionRange = range;
  CSSParserTokenRange functionArgs = consumeFunction(functionRange);

  if (functionArgs.peek().type() != StringToken)
    return nullptr;
  String pathString =
      functionArgs.consumeIncludingWhitespace().value().toString();

  std::unique_ptr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
  if (buildByteStreamFromString(pathString, *byteStream) !=
          SVGParseStatus::NoError ||
      !functionArgs.atEnd())
    return nullptr;

  range = functionRange;
  if (byteStream->isEmpty())
    return CSSIdentifierValue::create(CSSValueNone);
  return CSSPathValue::create(std::move(byteStream));
}

static CSSValue* consumePathOrNone(CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return consumeIdent(range);

  return consumePath(range);
}

static CSSValue* consumeOffsetRotate(CSSParserTokenRange& range) {
  CSSValue* angle = consumeAngle(range);
  CSSValue* keyword = consumeIdent<CSSValueAuto, CSSValueReverse>(range);
  if (!angle && !keyword)
    return nullptr;

  if (!angle)
    angle = consumeAngle(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (keyword)
    list->append(*keyword);
  if (angle)
    list->append(*angle);
  return list;
}

static CSSValue* consumeOffsetPath(CSSParserTokenRange& range,
                                   const CSSParserContext* context,
                                   bool isMotionPath) {
  CSSValue* value = consumePathOrNone(range);

  // Count when we receive a valid path other than 'none'.
  if (value && !value->isIdentifierValue()) {
    if (isMotionPath) {
      context->count(UseCounter::CSSMotionInEffect);
    } else {
      context->count(UseCounter::CSSOffsetInEffect);
    }
  }
  return value;
}

// offset: <offset-path> <offset-distance> <offset-rotation>
bool CSSPropertyParser::consumeOffsetShorthand(bool important) {
  const CSSValue* offsetPath = consumeOffsetPath(m_range, m_context, false);
  const CSSValue* offsetDistance =
      consumeLengthOrPercent(m_range, m_context->mode(), ValueRangeAll);
  const CSSValue* offsetRotation = consumeOffsetRotate(m_range);
  if (!offsetPath || !offsetDistance || !offsetRotation || !m_range.atEnd())
    return false;

  addProperty(CSSPropertyOffsetPath, CSSPropertyOffset, *offsetPath, important);
  addProperty(CSSPropertyOffsetDistance, CSSPropertyOffset, *offsetDistance,
              important);
  addProperty(CSSPropertyOffsetRotation, CSSPropertyOffset, *offsetRotation,
              important);

  return true;
}

static CSSValue* consumeBorderWidth(CSSParserTokenRange& range,
                                    CSSParserMode cssParserMode,
                                    UnitlessQuirk unitless) {
  return consumeLineWidth(range, cssParserMode, unitless);
}

static bool consumeTranslate3d(CSSParserTokenRange& args,
                               CSSParserMode cssParserMode,
                               CSSFunctionValue*& transformValue) {
  unsigned numberOfArguments = 2;
  CSSValue* parsedValue = nullptr;
  do {
    parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
    if (!parsedValue)
      return false;
    transformValue->append(*parsedValue);
    if (!consumeCommaIncludingWhitespace(args))
      return false;
  } while (--numberOfArguments);
  parsedValue = consumeLength(args, cssParserMode, ValueRangeAll);
  if (!parsedValue)
    return false;
  transformValue->append(*parsedValue);
  return true;
}

static bool consumeNumbers(CSSParserTokenRange& args,
                           CSSFunctionValue*& transformValue,
                           unsigned numberOfArguments) {
  do {
    CSSValue* parsedValue = consumeNumber(args, ValueRangeAll);
    if (!parsedValue)
      return false;
    transformValue->append(*parsedValue);
    if (--numberOfArguments && !consumeCommaIncludingWhitespace(args))
      return false;
  } while (numberOfArguments);
  return true;
}

static bool consumePerspective(CSSParserTokenRange& args,
                               const CSSParserContext* context,
                               CSSFunctionValue*& transformValue,
                               bool useLegacyParsing) {
  CSSPrimitiveValue* parsedValue =
      consumeLength(args, context->mode(), ValueRangeNonNegative);
  if (!parsedValue && useLegacyParsing) {
    double perspective;
    if (!consumeNumberRaw(args, perspective) || perspective < 0)
      return false;
    context->count(UseCounter::UnitlessPerspectiveInTransformProperty);
    parsedValue = CSSPrimitiveValue::create(
        perspective, CSSPrimitiveValue::UnitType::Pixels);
  }
  if (!parsedValue)
    return false;
  transformValue->append(*parsedValue);
  return true;
}

static CSSValue* consumeTransformValue(CSSParserTokenRange& range,
                                       const CSSParserContext* context,
                                       bool useLegacyParsing) {
  CSSValueID functionId = range.peek().functionId();
  if (functionId == CSSValueInvalid)
    return nullptr;
  CSSParserTokenRange args = consumeFunction(range);
  if (args.atEnd())
    return nullptr;
  CSSFunctionValue* transformValue = CSSFunctionValue::create(functionId);
  CSSValue* parsedValue = nullptr;
  switch (functionId) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
      parsedValue = consumeAngle(args);
      if (!parsedValue)
        return nullptr;
      if (functionId == CSSValueSkew && consumeCommaIncludingWhitespace(args)) {
        transformValue->append(*parsedValue);
        parsedValue = consumeAngle(args);
        if (!parsedValue)
          return nullptr;
      }
      break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
      parsedValue = consumeNumber(args, ValueRangeAll);
      if (!parsedValue)
        return nullptr;
      if (functionId == CSSValueScale &&
          consumeCommaIncludingWhitespace(args)) {
        transformValue->append(*parsedValue);
        parsedValue = consumeNumber(args, ValueRangeAll);
        if (!parsedValue)
          return nullptr;
      }
      break;
    case CSSValuePerspective:
      if (!consumePerspective(args, context, transformValue, useLegacyParsing))
        return nullptr;
      break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
      parsedValue =
          consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
      if (!parsedValue)
        return nullptr;
      if (functionId == CSSValueTranslate &&
          consumeCommaIncludingWhitespace(args)) {
        transformValue->append(*parsedValue);
        parsedValue =
            consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
        if (!parsedValue)
          return nullptr;
      }
      break;
    case CSSValueTranslateZ:
      parsedValue = consumeLength(args, context->mode(), ValueRangeAll);
      break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
      if (!consumeNumbers(args, transformValue,
                          (functionId == CSSValueMatrix3d) ? 16 : 6))
        return nullptr;
      break;
    case CSSValueScale3d:
      if (!consumeNumbers(args, transformValue, 3))
        return nullptr;
      break;
    case CSSValueRotate3d:
      if (!consumeNumbers(args, transformValue, 3) ||
          !consumeCommaIncludingWhitespace(args))
        return nullptr;
      parsedValue = consumeAngle(args);
      if (!parsedValue)
        return nullptr;
      break;
    case CSSValueTranslate3d:
      if (!consumeTranslate3d(args, context->mode(), transformValue))
        return nullptr;
      break;
    default:
      return nullptr;
  }
  if (parsedValue)
    transformValue->append(*parsedValue);
  if (!args.atEnd())
    return nullptr;
  return transformValue;
}

static CSSValue* consumeTransform(CSSParserTokenRange& range,
                                  const CSSParserContext* context,
                                  bool useLegacyParsing) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  do {
    CSSValue* parsedTransformValue =
        consumeTransformValue(range, context, useLegacyParsing);
    if (!parsedTransformValue)
      return nullptr;
    list->append(*parsedTransformValue);
  } while (!range.atEnd());

  return list;
}

static CSSValue* consumeNoneOrURI(CSSParserTokenRange& range,
                                  const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);
  return consumeUrl(range, context);
}

static CSSValue* consumePerspective(CSSParserTokenRange& range,
                                    const CSSParserContext* context,
                                    CSSPropertyID unresolvedProperty) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);
  CSSPrimitiveValue* parsedValue =
      consumeLength(range, context->mode(), ValueRangeAll);
  if (!parsedValue &&
      (unresolvedProperty == CSSPropertyAliasWebkitPerspective)) {
    double perspective;
    if (!consumeNumberRaw(range, perspective))
      return nullptr;
    context->count(UseCounter::UnitlessPerspectiveInPerspectiveProperty);
    parsedValue = CSSPrimitiveValue::create(
        perspective, CSSPrimitiveValue::UnitType::Pixels);
  }
  if (parsedValue &&
      (parsedValue->isCalculated() || parsedValue->getDoubleValue() > 0))
    return parsedValue;
  return nullptr;
}

static CSSValue* consumeScrollSnapPoints(CSSParserTokenRange& range,
                                         CSSParserMode cssParserMode) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);
  if (range.peek().functionId() == CSSValueRepeat) {
    CSSParserTokenRange args = consumeFunction(range);
    CSSPrimitiveValue* parsedValue =
        consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
    if (args.atEnd() && parsedValue &&
        (parsedValue->isCalculated() || parsedValue->getDoubleValue() > 0)) {
      CSSFunctionValue* result = CSSFunctionValue::create(CSSValueRepeat);
      result->append(*parsedValue);
      return result;
    }
  }
  return nullptr;
}

static CSSValue* consumeContentDistributionOverflowPosition(
    CSSParserTokenRange& range) {
  if (identMatches<CSSValueNormal, CSSValueBaseline, CSSValueLastBaseline>(
          range.peek().id()))
    return CSSContentDistributionValue::create(
        CSSValueInvalid, range.consumeIncludingWhitespace().id(),
        CSSValueInvalid);

  CSSValueID distribution = CSSValueInvalid;
  CSSValueID position = CSSValueInvalid;
  CSSValueID overflow = CSSValueInvalid;
  do {
    CSSValueID id = range.peek().id();
    if (identMatches<CSSValueSpaceBetween, CSSValueSpaceAround,
                     CSSValueSpaceEvenly, CSSValueStretch>(id)) {
      if (distribution != CSSValueInvalid)
        return nullptr;
      distribution = id;
    } else if (identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter,
                            CSSValueFlexStart, CSSValueFlexEnd, CSSValueLeft,
                            CSSValueRight>(id)) {
      if (position != CSSValueInvalid)
        return nullptr;
      position = id;
    } else if (identMatches<CSSValueUnsafe, CSSValueSafe>(id)) {
      if (overflow != CSSValueInvalid)
        return nullptr;
      overflow = id;
    } else {
      return nullptr;
    }
    range.consumeIncludingWhitespace();
  } while (!range.atEnd());

  // The grammar states that we should have at least <content-distribution> or
  // <content-position>.
  if (position == CSSValueInvalid && distribution == CSSValueInvalid)
    return nullptr;

  // The grammar states that <overflow-position> must be associated to
  // <content-position>.
  if (overflow != CSSValueInvalid && position == CSSValueInvalid)
    return nullptr;

  return CSSContentDistributionValue::create(distribution, position, overflow);
}

static CSSIdentifierValue* consumeBorderImageRepeatKeyword(
    CSSParserTokenRange& range) {
  return consumeIdent<CSSValueStretch, CSSValueRepeat, CSSValueSpace,
                      CSSValueRound>(range);
}

static CSSValue* consumeBorderImageRepeat(CSSParserTokenRange& range) {
  CSSIdentifierValue* horizontal = consumeBorderImageRepeatKeyword(range);
  if (!horizontal)
    return nullptr;
  CSSIdentifierValue* vertical = consumeBorderImageRepeatKeyword(range);
  if (!vertical)
    vertical = horizontal;
  return CSSValuePair::create(horizontal, vertical,
                              CSSValuePair::DropIdenticalValues);
}

static CSSValue* consumeBorderImageSlice(CSSPropertyID property,
                                         CSSParserTokenRange& range) {
  bool fill = consumeIdent<CSSValueFill>(range);
  CSSValue* slices[4] = {0};

  for (size_t index = 0; index < 4; ++index) {
    CSSPrimitiveValue* value = consumePercent(range, ValueRangeNonNegative);
    if (!value)
      value = consumeNumber(range, ValueRangeNonNegative);
    if (!value)
      break;
    slices[index] = value;
  }
  if (!slices[0])
    return nullptr;
  if (consumeIdent<CSSValueFill>(range)) {
    if (fill)
      return nullptr;
    fill = true;
  }
  complete4Sides(slices);
  // FIXME: For backwards compatibility, -webkit-border-image,
  // -webkit-mask-box-image and -webkit-box-reflect have to do a fill by
  // default.
  // FIXME: What do we do with -webkit-box-reflect and -webkit-mask-box-image?
  // Probably just have to leave them filling...
  if (property == CSSPropertyWebkitBorderImage ||
      property == CSSPropertyWebkitMaskBoxImage ||
      property == CSSPropertyWebkitBoxReflect)
    fill = true;
  return CSSBorderImageSliceValue::create(
      CSSQuadValue::create(slices[0], slices[1], slices[2], slices[3],
                           CSSQuadValue::SerializeAsQuad),
      fill);
}

static CSSValue* consumeBorderImageOutset(CSSParserTokenRange& range) {
  CSSValue* outsets[4] = {0};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = consumeNumber(range, ValueRangeNonNegative);
    if (!value)
      value = consumeLength(range, HTMLStandardMode, ValueRangeNonNegative);
    if (!value)
      break;
    outsets[index] = value;
  }
  if (!outsets[0])
    return nullptr;
  complete4Sides(outsets);
  return CSSQuadValue::create(outsets[0], outsets[1], outsets[2], outsets[3],
                              CSSQuadValue::SerializeAsQuad);
}

static CSSValue* consumeBorderImageWidth(CSSParserTokenRange& range) {
  CSSValue* widths[4] = {0};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value = consumeNumber(range, ValueRangeNonNegative);
    if (!value)
      value =
          consumeLengthOrPercent(range, HTMLStandardMode, ValueRangeNonNegative,
                                 UnitlessQuirk::Forbid);
    if (!value)
      value = consumeIdent<CSSValueAuto>(range);
    if (!value)
      break;
    widths[index] = value;
  }
  if (!widths[0])
    return nullptr;
  complete4Sides(widths);
  return CSSQuadValue::create(widths[0], widths[1], widths[2], widths[3],
                              CSSQuadValue::SerializeAsQuad);
}

static bool consumeBorderImageComponents(CSSPropertyID property,
                                         CSSParserTokenRange& range,
                                         const CSSParserContext* context,
                                         CSSValue*& source,
                                         CSSValue*& slice,
                                         CSSValue*& width,
                                         CSSValue*& outset,
                                         CSSValue*& repeat) {
  do {
    if (!source) {
      source = consumeImageOrNone(range, context);
      if (source)
        continue;
    }
    if (!repeat) {
      repeat = consumeBorderImageRepeat(range);
      if (repeat)
        continue;
    }
    if (!slice) {
      slice = consumeBorderImageSlice(property, range);
      if (slice) {
        ASSERT(!width && !outset);
        if (consumeSlashIncludingWhitespace(range)) {
          width = consumeBorderImageWidth(range);
          if (consumeSlashIncludingWhitespace(range)) {
            outset = consumeBorderImageOutset(range);
            if (!outset)
              return false;
          } else if (!width) {
            return false;
          }
        }
      } else {
        return false;
      }
    } else {
      return false;
    }
  } while (!range.atEnd());
  return true;
}

static CSSValue* consumeWebkitBorderImage(CSSPropertyID property,
                                          CSSParserTokenRange& range,
                                          const CSSParserContext* context) {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;
  if (consumeBorderImageComponents(property, range, context, source, slice,
                                   width, outset, repeat))
    return createBorderImageValue(source, slice, width, outset, repeat);
  return nullptr;
}

static CSSValue* consumeReflect(CSSParserTokenRange& range,
                                const CSSParserContext* context) {
  CSSIdentifierValue* direction =
      consumeIdent<CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(
          range);
  if (!direction)
    return nullptr;

  CSSPrimitiveValue* offset = nullptr;
  if (range.atEnd()) {
    offset = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
  } else {
    offset = consumeLengthOrPercent(range, context->mode(), ValueRangeAll,
                                    UnitlessQuirk::Forbid);
    if (!offset)
      return nullptr;
  }

  CSSValue* mask = nullptr;
  if (!range.atEnd()) {
    mask =
        consumeWebkitBorderImage(CSSPropertyWebkitBoxReflect, range, context);
    if (!mask)
      return nullptr;
  }
  return CSSReflectValue::create(direction, offset, mask);
}

static CSSValue* consumeBackgroundBlendMode(CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueNormal || id == CSSValueOverlay ||
      (id >= CSSValueMultiply && id <= CSSValueLuminosity))
    return consumeIdent(range);
  return nullptr;
}

static CSSValue* consumeBackgroundAttachment(CSSParserTokenRange& range) {
  return consumeIdent<CSSValueScroll, CSSValueFixed, CSSValueLocal>(range);
}

static CSSValue* consumeBackgroundBox(CSSParserTokenRange& range) {
  return consumeIdent<CSSValueBorderBox, CSSValuePaddingBox,
                      CSSValueContentBox>(range);
}

static CSSValue* consumeBackgroundComposite(CSSParserTokenRange& range) {
  return consumeIdentRange(range, CSSValueClear, CSSValuePlusLighter);
}

static CSSValue* consumeMaskSourceType(CSSParserTokenRange& range) {
  ASSERT(RuntimeEnabledFeatures::cssMaskSourceTypeEnabled());
  return consumeIdent<CSSValueAuto, CSSValueAlpha, CSSValueLuminance>(range);
}

static CSSValue* consumePrefixedBackgroundBox(CSSPropertyID property,
                                              CSSParserTokenRange& range,
                                              const CSSParserContext* context) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value =
          consumeIdentRange(range, CSSValueBorder, CSSValuePaddingBox))
    return value;
  if ((property == CSSPropertyWebkitBackgroundClip ||
       property == CSSPropertyWebkitMaskClip) &&
      range.peek().id() == CSSValueText)
    return consumeIdent(range);
  return nullptr;
}

static CSSValue* consumeBackgroundSize(CSSPropertyID unresolvedProperty,
                                       CSSParserTokenRange& range,
                                       CSSParserMode cssParserMode) {
  if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
    return consumeIdent(range);

  CSSValue* horizontal = consumeIdent<CSSValueAuto>(range);
  if (!horizontal)
    horizontal = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll,
                                        UnitlessQuirk::Forbid);

  CSSValue* vertical = nullptr;
  if (!range.atEnd()) {
    if (range.peek().id() == CSSValueAuto)  // `auto' is the default
      range.consumeIncludingWhitespace();
    else
      vertical = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll,
                                        UnitlessQuirk::Forbid);
  } else if (unresolvedProperty == CSSPropertyAliasWebkitBackgroundSize) {
    // Legacy syntax: "-webkit-background-size: 10px" is equivalent to
    // "background-size: 10px 10px".
    vertical = horizontal;
  }
  if (!vertical)
    return horizontal;
  return CSSValuePair::create(horizontal, vertical,
                              CSSValuePair::KeepIdenticalValues);
}

static CSSValue* consumeBackgroundComponent(CSSPropertyID unresolvedProperty,
                                            CSSParserTokenRange& range,
                                            const CSSParserContext* context) {
  switch (unresolvedProperty) {
    case CSSPropertyBackgroundClip:
      return consumeBackgroundBox(range);
    case CSSPropertyBackgroundBlendMode:
      return consumeBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
      return consumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
      return consumeBackgroundBox(range);
    case CSSPropertyWebkitMaskComposite:
      return consumeBackgroundComposite(range);
    case CSSPropertyMaskSourceType:
      return consumeMaskSourceType(range);
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskOrigin:
      return consumePrefixedBackgroundBox(unresolvedProperty, range, context);
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
      return consumeImageOrNone(range, context);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
      return CSSPropertyPositionUtils::consumePositionLonghand<CSSValueLeft,
                                                               CSSValueRight>(
          range, context->mode());
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
      return CSSPropertyPositionUtils::consumePositionLonghand<CSSValueTop,
                                                               CSSValueBottom>(
          range, context->mode());
    case CSSPropertyBackgroundSize:
    case CSSPropertyAliasWebkitBackgroundSize:
    case CSSPropertyWebkitMaskSize:
      return consumeBackgroundSize(unresolvedProperty, range, context->mode());
    case CSSPropertyBackgroundColor:
      return consumeColor(range, context->mode());
    default:
      break;
  };
  return nullptr;
}

static void addBackgroundValue(CSSValue*& list, CSSValue* value) {
  if (list) {
    if (!list->isBaseValueList()) {
      CSSValue* firstValue = list;
      list = CSSValueList::createCommaSeparated();
      toCSSValueList(list)->append(*firstValue);
    }
    toCSSValueList(list)->append(*value);
  } else {
    // To conserve memory we don't actually wrap a single value in a list.
    list = value;
  }
}

static CSSValue* consumeCommaSeparatedBackgroundComponent(
    CSSPropertyID unresolvedProperty,
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValue* result = nullptr;
  do {
    CSSValue* value =
        consumeBackgroundComponent(unresolvedProperty, range, context);
    if (!value)
      return nullptr;
    addBackgroundValue(result, value);
  } while (consumeCommaIncludingWhitespace(range));
  return result;
}

static CSSValue* consumeFitContent(CSSParserTokenRange& range,
                                   CSSParserMode cssParserMode) {
  CSSParserTokenRange rangeCopy = range;
  CSSParserTokenRange args = consumeFunction(rangeCopy);
  CSSPrimitiveValue* length = consumeLengthOrPercent(
      args, cssParserMode, ValueRangeNonNegative, UnitlessQuirk::Allow);
  if (!length || !args.atEnd())
    return nullptr;
  range = rangeCopy;
  CSSFunctionValue* result = CSSFunctionValue::create(CSSValueFitContent);
  result->append(*length);
  return result;
}

static CSSCustomIdentValue* consumeCustomIdentForGridLine(
    CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueSpan)
    return nullptr;
  return consumeCustomIdent(range);
}

static CSSValue* consumeGridLine(CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueAuto)
    return consumeIdent(range);

  CSSIdentifierValue* spanValue = nullptr;
  CSSCustomIdentValue* gridLineName = nullptr;
  CSSPrimitiveValue* numericValue = consumeInteger(range);
  if (numericValue) {
    gridLineName = consumeCustomIdentForGridLine(range);
    spanValue = consumeIdent<CSSValueSpan>(range);
  } else {
    spanValue = consumeIdent<CSSValueSpan>(range);
    if (spanValue) {
      numericValue = consumeInteger(range);
      gridLineName = consumeCustomIdentForGridLine(range);
      if (!numericValue)
        numericValue = consumeInteger(range);
    } else {
      gridLineName = consumeCustomIdentForGridLine(range);
      if (gridLineName) {
        numericValue = consumeInteger(range);
        spanValue = consumeIdent<CSSValueSpan>(range);
        if (!spanValue && !numericValue)
          return gridLineName;
      } else {
        return nullptr;
      }
    }
  }

  if (spanValue && !numericValue && !gridLineName)
    return nullptr;  // "span" keyword alone is invalid.
  if (spanValue && numericValue && numericValue->getIntValue() < 0)
    return nullptr;  // Negative numbers are not allowed for span.
  if (numericValue && numericValue->getIntValue() == 0)
    return nullptr;  // An <integer> value of zero makes the declaration
                     // invalid.

  if (numericValue) {
    numericValue = CSSPrimitiveValue::create(
        clampTo(numericValue->getIntValue(), -kGridMaxTracks, kGridMaxTracks),
        CSSPrimitiveValue::UnitType::Integer);
  }

  CSSValueList* values = CSSValueList::createSpaceSeparated();
  if (spanValue)
    values->append(*spanValue);
  if (numericValue)
    values->append(*numericValue);
  if (gridLineName)
    values->append(*gridLineName);
  ASSERT(values->length());
  return values;
}

static bool isGridBreadthFixedSized(const CSSValue& value) {
  if (value.isIdentifierValue()) {
    CSSValueID valueID = toCSSIdentifierValue(value).getValueID();
    return !(valueID == CSSValueMinContent || valueID == CSSValueMaxContent ||
             valueID == CSSValueAuto);
  }

  if (value.isPrimitiveValue()) {
    return !toCSSPrimitiveValue(value).isFlex();
  }

  NOTREACHED();
  return true;
}

static bool isGridTrackFixedSized(const CSSValue& value) {
  if (value.isPrimitiveValue() || value.isIdentifierValue())
    return isGridBreadthFixedSized(value);

  DCHECK(value.isFunctionValue());
  auto& function = toCSSFunctionValue(value);
  if (function.functionType() == CSSValueFitContent)
    return false;

  const CSSValue& minValue = function.item(0);
  const CSSValue& maxValue = function.item(1);
  return isGridBreadthFixedSized(minValue) || isGridBreadthFixedSized(maxValue);
}

static Vector<String> parseGridTemplateAreasColumnNames(
    const String& gridRowNames) {
  ASSERT(!gridRowNames.isEmpty());
  Vector<String> columnNames;
  // Using StringImpl to avoid checks and indirection in every call to
  // String::operator[].
  StringImpl& text = *gridRowNames.impl();

  StringBuilder areaName;
  for (unsigned i = 0; i < text.length(); ++i) {
    if (isCSSSpace(text[i])) {
      if (!areaName.isEmpty()) {
        columnNames.push_back(areaName.toString());
        areaName.clear();
      }
      continue;
    }
    if (text[i] == '.') {
      if (areaName == ".")
        continue;
      if (!areaName.isEmpty()) {
        columnNames.push_back(areaName.toString());
        areaName.clear();
      }
    } else {
      if (!isNameCodePoint(text[i]))
        return Vector<String>();
      if (areaName == ".") {
        columnNames.push_back(areaName.toString());
        areaName.clear();
      }
    }

    areaName.append(text[i]);
  }

  if (!areaName.isEmpty())
    columnNames.push_back(areaName.toString());

  return columnNames;
}

static bool parseGridTemplateAreasRow(const String& gridRowNames,
                                      NamedGridAreaMap& gridAreaMap,
                                      const size_t rowCount,
                                      size_t& columnCount) {
  if (gridRowNames.isEmpty() || gridRowNames.containsOnlyWhitespace())
    return false;

  Vector<String> columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
  if (rowCount == 0) {
    columnCount = columnNames.size();
    if (columnCount == 0)
      return false;
  } else if (columnCount != columnNames.size()) {
    // The declaration is invalid if all the rows don't have the number of
    // columns.
    return false;
  }

  for (size_t currentColumn = 0; currentColumn < columnCount; ++currentColumn) {
    const String& gridAreaName = columnNames[currentColumn];

    // Unamed areas are always valid (we consider them to be 1x1).
    if (gridAreaName == ".")
      continue;

    size_t lookAheadColumn = currentColumn + 1;
    while (lookAheadColumn < columnCount &&
           columnNames[lookAheadColumn] == gridAreaName)
      lookAheadColumn++;

    NamedGridAreaMap::iterator gridAreaIt = gridAreaMap.find(gridAreaName);
    if (gridAreaIt == gridAreaMap.end()) {
      gridAreaMap.insert(
          gridAreaName,
          GridArea(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1),
                   GridSpan::translatedDefiniteGridSpan(currentColumn,
                                                        lookAheadColumn)));
    } else {
      GridArea& gridArea = gridAreaIt->value;

      // The following checks test that the grid area is a single filled-in
      // rectangle.
      // 1. The new row is adjacent to the previously parsed row.
      if (rowCount != gridArea.rows.endLine())
        return false;

      // 2. The new area starts at the same position as the previously parsed
      // area.
      if (currentColumn != gridArea.columns.startLine())
        return false;

      // 3. The new area ends at the same position as the previously parsed
      // area.
      if (lookAheadColumn != gridArea.columns.endLine())
        return false;

      gridArea.rows = GridSpan::translatedDefiniteGridSpan(
          gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
    }
    currentColumn = lookAheadColumn - 1;
  }

  return true;
}

static CSSValue* consumeGridBreadth(CSSParserTokenRange& range,
                                    CSSParserMode cssParserMode) {
  const CSSParserToken& token = range.peek();
  if (identMatches<CSSValueMinContent, CSSValueMaxContent, CSSValueAuto>(
          token.id()))
    return consumeIdent(range);
  if (token.type() == DimensionToken &&
      token.unitType() == CSSPrimitiveValue::UnitType::Fraction) {
    if (range.peek().numericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::create(
        range.consumeIncludingWhitespace().numericValue(),
        CSSPrimitiveValue::UnitType::Fraction);
  }
  return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative,
                                UnitlessQuirk::Allow);
}

static CSSValue* consumeGridTrackSize(CSSParserTokenRange& range,
                                      CSSParserMode cssParserMode) {
  const CSSParserToken& token = range.peek();
  if (identMatches<CSSValueAuto>(token.id()))
    return consumeIdent(range);

  if (token.functionId() == CSSValueMinmax) {
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    CSSValue* minTrackBreadth = consumeGridBreadth(args, cssParserMode);
    if (!minTrackBreadth || (minTrackBreadth->isPrimitiveValue() &&
                             toCSSPrimitiveValue(minTrackBreadth)->isFlex()) ||
        !consumeCommaIncludingWhitespace(args))
      return nullptr;
    CSSValue* maxTrackBreadth = consumeGridBreadth(args, cssParserMode);
    if (!maxTrackBreadth || !args.atEnd())
      return nullptr;
    range = rangeCopy;
    CSSFunctionValue* result = CSSFunctionValue::create(CSSValueMinmax);
    result->append(*minTrackBreadth);
    result->append(*maxTrackBreadth);
    return result;
  }

  if (token.functionId() == CSSValueFitContent)
    return consumeFitContent(range, cssParserMode);

  return consumeGridBreadth(range, cssParserMode);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a
// new one.
static CSSGridLineNamesValue* consumeGridLineNames(
    CSSParserTokenRange& range,
    CSSGridLineNamesValue* lineNames = nullptr) {
  CSSParserTokenRange rangeCopy = range;
  if (rangeCopy.consumeIncludingWhitespace().type() != LeftBracketToken)
    return nullptr;
  if (!lineNames)
    lineNames = CSSGridLineNamesValue::create();
  while (CSSCustomIdentValue* lineName =
             consumeCustomIdentForGridLine(rangeCopy))
    lineNames->append(*lineName);
  if (rangeCopy.consumeIncludingWhitespace().type() != RightBracketToken)
    return nullptr;
  range = rangeCopy;
  return lineNames;
}

static bool consumeGridTrackRepeatFunction(CSSParserTokenRange& range,
                                           CSSParserMode cssParserMode,
                                           CSSValueList& list,
                                           bool& isAutoRepeat,
                                           bool& allTracksAreFixedSized) {
  CSSParserTokenRange args = consumeFunction(range);
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  size_t repetitions = 1;
  isAutoRepeat =
      identMatches<CSSValueAutoFill, CSSValueAutoFit>(args.peek().id());
  CSSValueList* repeatedValues;
  if (isAutoRepeat) {
    repeatedValues =
        CSSGridAutoRepeatValue::create(args.consumeIncludingWhitespace().id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition = consumePositiveInteger(args);
    if (!repetition)
      return false;
    repetitions =
        clampTo<size_t>(repetition->getDoubleValue(), 0, kGridMaxTracks);
    repeatedValues = CSSValueList::createSpaceSeparated();
  }
  if (!consumeCommaIncludingWhitespace(args))
    return false;
  CSSGridLineNamesValue* lineNames = consumeGridLineNames(args);
  if (lineNames)
    repeatedValues->append(*lineNames);

  size_t numberOfTracks = 0;
  while (!args.atEnd()) {
    CSSValue* trackSize = consumeGridTrackSize(args, cssParserMode);
    if (!trackSize)
      return false;
    if (allTracksAreFixedSized)
      allTracksAreFixedSized = isGridTrackFixedSized(*trackSize);
    repeatedValues->append(*trackSize);
    ++numberOfTracks;
    lineNames = consumeGridLineNames(args);
    if (lineNames)
      repeatedValues->append(*lineNames);
  }
  // We should have found at least one <track-size> or else it is not a valid
  // <track-list>.
  if (!numberOfTracks)
    return false;

  if (isAutoRepeat) {
    list.append(*repeatedValues);
  } else {
    // We clamp the repetitions to a multiple of the repeat() track list's size,
    // while staying below the max grid size.
    repetitions = std::min(repetitions, kGridMaxTracks / numberOfTracks);
    for (size_t i = 0; i < repetitions; ++i) {
      for (size_t j = 0; j < repeatedValues->length(); ++j)
        list.append(repeatedValues->item(j));
    }
  }
  return true;
}

enum TrackListType { GridTemplate, GridTemplateNoRepeat, GridAuto };

static CSSValue* consumeGridTrackList(CSSParserTokenRange& range,
                                      CSSParserMode cssParserMode,
                                      TrackListType trackListType) {
  bool allowGridLineNames = trackListType != GridAuto;
  CSSValueList* values = CSSValueList::createSpaceSeparated();
  CSSGridLineNamesValue* lineNames = consumeGridLineNames(range);
  if (lineNames) {
    if (!allowGridLineNames)
      return nullptr;
    values->append(*lineNames);
  }

  bool allowRepeat = trackListType == GridTemplate;
  bool seenAutoRepeat = false;
  bool allTracksAreFixedSized = true;
  do {
    bool isAutoRepeat;
    if (range.peek().functionId() == CSSValueRepeat) {
      if (!allowRepeat)
        return nullptr;
      if (!consumeGridTrackRepeatFunction(range, cssParserMode, *values,
                                          isAutoRepeat, allTracksAreFixedSized))
        return nullptr;
      if (isAutoRepeat && seenAutoRepeat)
        return nullptr;
      seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
    } else if (CSSValue* value = consumeGridTrackSize(range, cssParserMode)) {
      if (allTracksAreFixedSized)
        allTracksAreFixedSized = isGridTrackFixedSized(*value);
      values->append(*value);
    } else {
      return nullptr;
    }
    if (seenAutoRepeat && !allTracksAreFixedSized)
      return nullptr;
    lineNames = consumeGridLineNames(range);
    if (lineNames) {
      if (!allowGridLineNames)
        return nullptr;
      values->append(*lineNames);
    }
  } while (!range.atEnd() && range.peek().type() != DelimiterToken);
  return values;
}

static CSSValue* consumeGridTemplatesRowsOrColumns(
    CSSParserTokenRange& range,
    CSSParserMode cssParserMode) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);
  return consumeGridTrackList(range, cssParserMode, GridTemplate);
}

static CSSValue* consumeGridTemplateAreas(CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueNone)
    return consumeIdent(range);

  NamedGridAreaMap gridAreaMap;
  size_t rowCount = 0;
  size_t columnCount = 0;

  while (range.peek().type() == StringToken) {
    if (!parseGridTemplateAreasRow(
            range.consumeIncludingWhitespace().value().toString(), gridAreaMap,
            rowCount, columnCount))
      return nullptr;
    ++rowCount;
  }

  if (rowCount == 0)
    return nullptr;
  ASSERT(columnCount);
  return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}

static void countKeywordOnlyPropertyUsage(CSSPropertyID property,
                                          const CSSParserContext* context,
                                          CSSValueID valueID) {
  if (!context->isUseCounterRecordingEnabled())
    return;
  switch (property) {
    case CSSPropertyWebkitAppearance: {
      UseCounter::Feature feature;
      if (valueID == CSSValueNone) {
        feature = UseCounter::CSSValueAppearanceNone;
      } else {
        feature = UseCounter::CSSValueAppearanceNotNone;
        if (valueID == CSSValueButton)
          feature = UseCounter::CSSValueAppearanceButton;
        else if (valueID == CSSValueCaret)
          feature = UseCounter::CSSValueAppearanceCaret;
        else if (valueID == CSSValueCheckbox)
          feature = UseCounter::CSSValueAppearanceCheckbox;
        else if (valueID == CSSValueMenulist)
          feature = UseCounter::CSSValueAppearanceMenulist;
        else if (valueID == CSSValueMenulistButton)
          feature = UseCounter::CSSValueAppearanceMenulistButton;
        else if (valueID == CSSValueListbox)
          feature = UseCounter::CSSValueAppearanceListbox;
        else if (valueID == CSSValueRadio)
          feature = UseCounter::CSSValueAppearanceRadio;
        else if (valueID == CSSValueSearchfield)
          feature = UseCounter::CSSValueAppearanceSearchField;
        else if (valueID == CSSValueTextfield)
          feature = UseCounter::CSSValueAppearanceTextField;
        else
          feature = UseCounter::CSSValueAppearanceOthers;
      }
      context->count(feature);
      break;
    }

    case CSSPropertyWebkitUserModify: {
      switch (valueID) {
        case CSSValueReadOnly:
          context->count(UseCounter::CSSValueUserModifyReadOnly);
          break;
        case CSSValueReadWrite:
          context->count(UseCounter::CSSValueUserModifyReadWrite);
          break;
        case CSSValueReadWritePlaintextOnly:
          context->count(UseCounter::CSSValueUserModifyReadWritePlaintextOnly);
          break;
        default:
          NOTREACHED();
      }
      break;
    }

    default:
      break;
  }
}

const CSSValue* CSSPropertyParser::parseSingleValue(
    CSSPropertyID unresolvedProperty,
    CSSPropertyID currentShorthand) {
  CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);
  if (CSSParserFastPaths::isKeywordPropertyID(property)) {
    if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(
            property, m_range.peek().id(), m_context->mode()))
      return nullptr;
    countKeywordOnlyPropertyUsage(property, m_context, m_range.peek().id());
    return consumeIdent(m_range);
  }

  // Gets the parsing method for our current property from the property API.
  // If it has been implemented, we call this method, otherwise we manually
  // parse this value in the switch statement below. As we implement APIs for
  // other properties, those properties will be taken out of the switch
  // statement.
  const CSSPropertyDescriptor& cssPropertyDesc =
      CSSPropertyDescriptor::get(property);
  if (cssPropertyDesc.parseSingleValue)
    return cssPropertyDesc.parseSingleValue(m_range, m_context);

  switch (property) {
    case CSSPropertyFontFeatureSettings:
      return consumeFontFeatureSettings(m_range);
    case CSSPropertyFontFamily:
      return consumeFontFamily(m_range);
    case CSSPropertyFontWeight:
      return consumeFontWeight(m_range);
    case CSSPropertyFontSize:
      return consumeFontSize(m_range, m_context->mode(), UnitlessQuirk::Allow);
    case CSSPropertyLineHeight:
      return consumeLineHeight(m_range, m_context->mode());
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
      return consumeLength(m_range, m_context->mode(), ValueRangeNonNegative);
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
      return consumeCounter(m_range,
                            property == CSSPropertyCounterIncrement ? 1 : 0);
    case CSSPropertyMaxWidth:
    case CSSPropertyMaxHeight:
      return CSSPropertyLengthUtils::consumeMaxWidthOrHeight(
          m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyMinWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWidth:
    case CSSPropertyHeight:
      return CSSPropertyLengthUtils::consumeWidthOrHeight(m_range, m_context,
                                                          UnitlessQuirk::Allow);
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
      return CSSPropertyLengthUtils::consumeWidthOrHeight(m_range, m_context);
    case CSSPropertyScrollSnapDestination:
    case CSSPropertyObjectPosition:
    case CSSPropertyPerspectiveOrigin:
      return consumePosition(m_range, m_context->mode(), UnitlessQuirk::Forbid);
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLocale:
      return consumeLocale(m_range);
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyTransitionProperty:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
      return consumeAnimationPropertyList(
          property, m_range, m_context,
          unresolvedProperty == CSSPropertyAliasWebkitAnimationName);
    case CSSPropertyGridColumnGap:
    case CSSPropertyGridRowGap:
      return consumeLengthOrPercent(m_range, m_context->mode(),
                                    ValueRangeNonNegative);
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
      return consumePositiveInteger(m_range);
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
      return consumeColor(m_range, m_context->mode(), inQuirksMode());
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterWidth:
      return consumeBorderWidth(m_range, m_context->mode(),
                                UnitlessQuirk::Forbid);
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor: {
      bool allowQuirkyColors =
          inQuirksMode() && (currentShorthand == CSSPropertyInvalid ||
                             currentShorthand == CSSPropertyBorderColor);
      return consumeColor(m_range, m_context->mode(), allowQuirkyColors);
    }
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth: {
      bool allowQuirkyLengths =
          inQuirksMode() && (currentShorthand == CSSPropertyInvalid ||
                             currentShorthand == CSSPropertyBorderWidth);
      UnitlessQuirk unitless =
          allowQuirkyLengths ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
      return consumeBorderWidth(m_range, m_context->mode(), unitless);
    }
    case CSSPropertyTextShadow:
    case CSSPropertyBoxShadow:
      return consumeShadow(m_range, m_context->mode(),
                           property == CSSPropertyBoxShadow);
    case CSSPropertyFilter:
    case CSSPropertyBackdropFilter:
      return consumeFilter(m_range, m_context);
    case CSSPropertyTextDecoration:
      ASSERT(!RuntimeEnabledFeatures::css3TextDecorationsEnabled());
    // fallthrough
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyTextDecorationLine:
      return consumeTextDecorationLine(m_range);
    case CSSPropertyD:
      return consumePathOrNone(m_range);
    case CSSPropertyOffsetPath:
      return consumeOffsetPath(
          m_range, m_context,
          currentShorthand == CSSPropertyMotion ||
              unresolvedProperty == CSSPropertyAliasMotionPath);
    case CSSPropertyOffsetDistance:
      return consumeLengthOrPercent(m_range, m_context->mode(), ValueRangeAll);
    case CSSPropertyOffsetRotate:
    case CSSPropertyOffsetRotation:
      return consumeOffsetRotate(m_range);
    case CSSPropertyTransform:
      return consumeTransform(
          m_range, m_context,
          unresolvedProperty == CSSPropertyAliasWebkitTransform);
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitPerspectiveOriginX:
      return CSSPropertyPositionUtils::consumePositionLonghand<CSSValueLeft,
                                                               CSSValueRight>(
          m_range, m_context->mode());
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitPerspectiveOriginY:
      return CSSPropertyPositionUtils::consumePositionLonghand<CSSValueTop,
                                                               CSSValueBottom>(
          m_range, m_context->mode());
    case CSSPropertyMarkerStart:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMask:
      return consumeNoneOrURI(m_range, m_context);
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
      return consumeNumber(m_range, ValueRangeNonNegative);
    case CSSPropertyWebkitBoxFlex:
      return consumeNumber(m_range, ValueRangeAll);
    case CSSPropertyStrokeWidth:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyX:
    case CSSPropertyY:
    case CSSPropertyR:
      return consumeLengthOrPercent(m_range, SVGAttributeMode, ValueRangeAll,
                                    UnitlessQuirk::Forbid);
    case CSSPropertyPerspective:
      return consumePerspective(m_range, m_context, unresolvedProperty);
    case CSSPropertyScrollSnapPointsX:
    case CSSPropertyScrollSnapPointsY:
      return consumeScrollSnapPoints(m_range, m_context->mode());
    case CSSPropertyJustifyContent:
    case CSSPropertyAlignContent:
      ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
      return consumeContentDistributionOverflowPosition(m_range);
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyWebkitMaskBoxImageRepeat:
      return consumeBorderImageRepeat(m_range);
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice:
      return consumeBorderImageSlice(property, m_range);
    case CSSPropertyBorderImageOutset:
    case CSSPropertyWebkitMaskBoxImageOutset:
      return consumeBorderImageOutset(m_range);
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageWidth:
      return consumeBorderImageWidth(m_range);
    case CSSPropertyWebkitBorderImage:
      return consumeWebkitBorderImage(property, m_range, m_context);
    case CSSPropertyWebkitBoxReflect:
      return consumeReflect(m_range, m_context);
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyMaskSourceType:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
      return consumeCommaSeparatedBackgroundComponent(unresolvedProperty,
                                                      m_range, m_context);
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
      return nullptr;
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
      ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
      return consumeGridLine(m_range);
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
      ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
      return consumeGridTrackList(m_range, m_context->mode(), GridAuto);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
      ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
      return consumeGridTemplatesRowsOrColumns(m_range, m_context->mode());
    case CSSPropertyGridTemplateAreas:
      ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
      return consumeGridTemplateAreas(m_range);
    default:
      return nullptr;
  }
}

static CSSIdentifierValue* consumeFontDisplay(CSSParserTokenRange& range) {
  return consumeIdent<CSSValueAuto, CSSValueBlock, CSSValueSwap,
                      CSSValueFallback, CSSValueOptional>(range);
}

static CSSValueList* consumeFontFaceUnicodeRange(CSSParserTokenRange& range) {
  CSSValueList* values = CSSValueList::createCommaSeparated();

  do {
    const CSSParserToken& token = range.consumeIncludingWhitespace();
    if (token.type() != UnicodeRangeToken)
      return nullptr;

    UChar32 start = token.unicodeRangeStart();
    UChar32 end = token.unicodeRangeEnd();
    if (start > end)
      return nullptr;
    values->append(*CSSUnicodeRangeValue::create(start, end));
  } while (consumeCommaIncludingWhitespace(range));

  return values;
}

static CSSValue* consumeFontFaceSrcURI(CSSParserTokenRange& range,
                                       const CSSParserContext* context) {
  String url = consumeUrlAsStringView(range).toString();
  if (url.isNull())
    return nullptr;
  CSSFontFaceSrcValue* uriValue(
      CSSFontFaceSrcValue::create(url, context->completeURL(url),
                                  context->shouldCheckContentSecurityPolicy()));
  uriValue->setReferrer(context->referrer());

  if (range.peek().functionId() != CSSValueFormat)
    return uriValue;

  // FIXME: https://drafts.csswg.org/css-fonts says that format() contains a
  // comma-separated list of strings, but CSSFontFaceSrcValue stores only one
  // format. Allowing one format for now.
  CSSParserTokenRange args = consumeFunction(range);
  const CSSParserToken& arg = args.consumeIncludingWhitespace();
  if ((arg.type() != StringToken) || !args.atEnd())
    return nullptr;
  uriValue->setFormat(arg.value().toString());
  return uriValue;
}

static CSSValue* consumeFontFaceSrcLocal(CSSParserTokenRange& range,
                                         const CSSParserContext* context) {
  CSSParserTokenRange args = consumeFunction(range);
  ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy =
      context->shouldCheckContentSecurityPolicy();
  if (args.peek().type() == StringToken) {
    const CSSParserToken& arg = args.consumeIncludingWhitespace();
    if (!args.atEnd())
      return nullptr;
    return CSSFontFaceSrcValue::createLocal(arg.value().toString(),
                                            shouldCheckContentSecurityPolicy);
  }
  if (args.peek().type() == IdentToken) {
    String familyName = concatenateFamilyName(args);
    if (!args.atEnd())
      return nullptr;
    return CSSFontFaceSrcValue::createLocal(familyName,
                                            shouldCheckContentSecurityPolicy);
  }
  return nullptr;
}

static CSSValueList* consumeFontFaceSrc(CSSParserTokenRange& range,
                                        const CSSParserContext* context) {
  CSSValueList* values = CSSValueList::createCommaSeparated();

  do {
    const CSSParserToken& token = range.peek();
    CSSValue* parsedValue = nullptr;
    if (token.functionId() == CSSValueLocal)
      parsedValue = consumeFontFaceSrcLocal(range, context);
    else
      parsedValue = consumeFontFaceSrcURI(range, context);
    if (!parsedValue)
      return nullptr;
    values->append(*parsedValue);
  } while (consumeCommaIncludingWhitespace(range));
  return values;
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID propId) {
  CSSValue* parsedValue = nullptr;
  switch (propId) {
    case CSSPropertyFontFamily:
      if (consumeGenericFamily(m_range))
        return false;
      parsedValue = consumeFamilyName(m_range);
      break;
    case CSSPropertySrc:  // This is a list of urls or local references.
      parsedValue = consumeFontFaceSrc(m_range, m_context);
      break;
    case CSSPropertyUnicodeRange:
      parsedValue = consumeFontFaceUnicodeRange(m_range);
      break;
    case CSSPropertyFontDisplay:
      parsedValue = consumeFontDisplay(m_range);
      break;
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle: {
      CSSValueID id = m_range.consumeIncludingWhitespace().id();
      if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(
              propId, id, m_context->mode()))
        return false;
      parsedValue = CSSIdentifierValue::create(id);
      break;
    }
    case CSSPropertyFontVariant:
      parsedValue = consumeFontVariantList(m_range);
      break;
    case CSSPropertyFontWeight:
      parsedValue = consumeFontWeight(m_range);
      break;
    case CSSPropertyFontFeatureSettings:
      parsedValue = consumeFontFeatureSettings(m_range);
      break;
    default:
      break;
  }

  if (!parsedValue || !m_range.atEnd())
    return false;

  addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
  return true;
}

bool CSSPropertyParser::consumeSystemFont(bool important) {
  CSSValueID systemFontID = m_range.consumeIncludingWhitespace().id();
  ASSERT(systemFontID >= CSSValueCaption && systemFontID <= CSSValueStatusBar);
  if (!m_range.atEnd())
    return false;

  FontStyle fontStyle = FontStyleNormal;
  FontWeight fontWeight = FontWeightNormal;
  float fontSize = 0;
  AtomicString fontFamily;
  LayoutTheme::theme().systemFont(systemFontID, fontStyle, fontWeight, fontSize,
                                  fontFamily);

  addProperty(CSSPropertyFontStyle, CSSPropertyFont,
              *CSSIdentifierValue::create(fontStyle == FontStyleItalic
                                              ? CSSValueItalic
                                              : CSSValueNormal),
              important);
  addProperty(CSSPropertyFontWeight, CSSPropertyFont,
              *CSSIdentifierValue::create(fontWeight), important);
  addProperty(
      CSSPropertyFontSize, CSSPropertyFont,
      *CSSPrimitiveValue::create(fontSize, CSSPrimitiveValue::UnitType::Pixels),
      important);
  CSSValueList* fontFamilyList = CSSValueList::createCommaSeparated();
  fontFamilyList->append(*CSSFontFamilyValue::create(fontFamily));
  addProperty(CSSPropertyFontFamily, CSSPropertyFont, *fontFamilyList,
              important);

  addProperty(CSSPropertyFontStretch, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);
  addProperty(CSSPropertyFontVariantCaps, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);
  addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);
  addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);
  addProperty(CSSPropertyLineHeight, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);
  return true;
}

bool CSSPropertyParser::consumeFont(bool important) {
  // Let's check if there is an inherit or initial somewhere in the shorthand.
  CSSParserTokenRange range = m_range;
  while (!range.atEnd()) {
    CSSValueID id = range.consumeIncludingWhitespace().id();
    if (id == CSSValueInherit || id == CSSValueInitial)
      return false;
  }
  // Optional font-style, font-variant, font-stretch and font-weight.
  CSSIdentifierValue* fontStyle = nullptr;
  CSSIdentifierValue* fontVariantCaps = nullptr;
  CSSIdentifierValue* fontWeight = nullptr;
  CSSIdentifierValue* fontStretch = nullptr;
  while (!m_range.atEnd()) {
    CSSValueID id = m_range.peek().id();
    if (!fontStyle && CSSParserFastPaths::isValidKeywordPropertyAndValue(
                          CSSPropertyFontStyle, id, m_context->mode())) {
      fontStyle = consumeIdent(m_range);
      continue;
    }
    if (!fontVariantCaps && (id == CSSValueNormal || id == CSSValueSmallCaps)) {
      // Font variant in the shorthand is particular, it only accepts normal or
      // small-caps.
      // See https://drafts.csswg.org/css-fonts/#propdef-font
      fontVariantCaps = consumeFontVariantCSS21(m_range);
      if (fontVariantCaps)
        continue;
    }
    if (!fontWeight) {
      fontWeight = consumeFontWeight(m_range);
      if (fontWeight)
        continue;
    }
    if (!fontStretch && CSSParserFastPaths::isValidKeywordPropertyAndValue(
                            CSSPropertyFontStretch, id, m_context->mode()))
      fontStretch = consumeIdent(m_range);
    else
      break;
  }

  if (m_range.atEnd())
    return false;

  addProperty(
      CSSPropertyFontStyle, CSSPropertyFont,
      fontStyle ? *fontStyle : *CSSIdentifierValue::create(CSSValueNormal),
      important);
  addProperty(CSSPropertyFontVariantCaps, CSSPropertyFont,
              fontVariantCaps ? *fontVariantCaps
                              : *CSSIdentifierValue::create(CSSValueNormal),
              important);
  addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);
  addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFont,
              *CSSIdentifierValue::create(CSSValueNormal), important);

  addProperty(
      CSSPropertyFontWeight, CSSPropertyFont,
      fontWeight ? *fontWeight : *CSSIdentifierValue::create(CSSValueNormal),
      important);
  addProperty(
      CSSPropertyFontStretch, CSSPropertyFont,
      fontStretch ? *fontStretch : *CSSIdentifierValue::create(CSSValueNormal),
      important);

  // Now a font size _must_ come.
  CSSValue* fontSize = consumeFontSize(m_range, m_context->mode());
  if (!fontSize || m_range.atEnd())
    return false;

  addProperty(CSSPropertyFontSize, CSSPropertyFont, *fontSize, important);

  if (consumeSlashIncludingWhitespace(m_range)) {
    CSSValue* lineHeight = consumeLineHeight(m_range, m_context->mode());
    if (!lineHeight)
      return false;
    addProperty(CSSPropertyLineHeight, CSSPropertyFont, *lineHeight, important);
  } else {
    addProperty(CSSPropertyLineHeight, CSSPropertyFont,
                *CSSIdentifierValue::create(CSSValueNormal), important);
  }

  // Font family must come now.
  CSSValue* parsedFamilyValue = consumeFontFamily(m_range);
  if (!parsedFamilyValue)
    return false;

  addProperty(CSSPropertyFontFamily, CSSPropertyFont, *parsedFamilyValue,
              important);

  // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires
  // that "font-stretch", "font-size-adjust", and "font-kerning" be reset to
  // their initial values but we don't seem to support them at the moment. They
  // should also be added here once implemented.
  return m_range.atEnd();
}

bool CSSPropertyParser::consumeFontVariantShorthand(bool important) {
  if (identMatches<CSSValueNormal, CSSValueNone>(m_range.peek().id())) {
    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant,
                *consumeIdent(m_range), important);
    addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant,
                *CSSIdentifierValue::create(CSSValueNormal), important);
    return m_range.atEnd();
  }

  CSSIdentifierValue* capsValue = nullptr;
  FontVariantLigaturesParser ligaturesParser;
  FontVariantNumericParser numericParser;
  do {
    FontVariantLigaturesParser::ParseResult ligaturesParseResult =
        ligaturesParser.consumeLigature(m_range);
    FontVariantNumericParser::ParseResult numericParseResult =
        numericParser.consumeNumeric(m_range);
    if (ligaturesParseResult ==
            FontVariantLigaturesParser::ParseResult::ConsumedValue ||
        numericParseResult ==
            FontVariantNumericParser::ParseResult::ConsumedValue)
      continue;

    if (ligaturesParseResult ==
            FontVariantLigaturesParser::ParseResult::DisallowedValue ||
        numericParseResult ==
            FontVariantNumericParser::ParseResult::DisallowedValue)
      return false;

    CSSValueID id = m_range.peek().id();
    switch (id) {
      case CSSValueSmallCaps:
      case CSSValueAllSmallCaps:
      case CSSValuePetiteCaps:
      case CSSValueAllPetiteCaps:
      case CSSValueUnicase:
      case CSSValueTitlingCaps:
        // Only one caps value permitted in font-variant grammar.
        if (capsValue)
          return false;
        capsValue = consumeIdent(m_range);
        break;
      default:
        return false;
    }
  } while (!m_range.atEnd());

  addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant,
              *ligaturesParser.finalizeValue(), important);
  addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant,
              *numericParser.finalizeValue(), important);
  addProperty(
      CSSPropertyFontVariantCaps, CSSPropertyFontVariant,
      capsValue ? *capsValue : *CSSIdentifierValue::create(CSSValueNormal),
      important);
  return true;
}

bool CSSPropertyParser::consumeBorderSpacing(bool important) {
  CSSValue* horizontalSpacing = consumeLength(
      m_range, m_context->mode(), ValueRangeNonNegative, UnitlessQuirk::Allow);
  if (!horizontalSpacing)
    return false;
  CSSValue* verticalSpacing = horizontalSpacing;
  if (!m_range.atEnd()) {
    verticalSpacing =
        consumeLength(m_range, m_context->mode(), ValueRangeNonNegative,
                      UnitlessQuirk::Allow);
  }
  if (!verticalSpacing || !m_range.atEnd())
    return false;
  addProperty(CSSPropertyWebkitBorderHorizontalSpacing,
              CSSPropertyBorderSpacing, *horizontalSpacing, important);
  addProperty(CSSPropertyWebkitBorderVerticalSpacing, CSSPropertyBorderSpacing,
              *verticalSpacing, important);
  return true;
}

static CSSValue* consumeSingleViewportDescriptor(CSSParserTokenRange& range,
                                                 CSSPropertyID propId,
                                                 CSSParserMode cssParserMode) {
  CSSValueID id = range.peek().id();
  switch (propId) {
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
      if (id == CSSValueAuto || id == CSSValueInternalExtendToZoom)
        return consumeIdent(range);
      return consumeLengthOrPercent(range, cssParserMode,
                                    ValueRangeNonNegative);
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom: {
      if (id == CSSValueAuto)
        return consumeIdent(range);
      CSSValue* parsedValue = consumeNumber(range, ValueRangeNonNegative);
      if (parsedValue)
        return parsedValue;
      return consumePercent(range, ValueRangeNonNegative);
    }
    case CSSPropertyUserZoom:
      return consumeIdent<CSSValueZoom, CSSValueFixed>(range);
    case CSSPropertyOrientation:
      return consumeIdent<CSSValueAuto, CSSValuePortrait, CSSValueLandscape>(
          range);
    default:
      ASSERT_NOT_REACHED();
      break;
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

bool CSSPropertyParser::parseViewportDescriptor(CSSPropertyID propId,
                                                bool important) {
  ASSERT(RuntimeEnabledFeatures::cssViewportEnabled() ||
         isUASheetBehavior(m_context->mode()));

  switch (propId) {
    case CSSPropertyWidth: {
      CSSValue* minWidth = consumeSingleViewportDescriptor(
          m_range, CSSPropertyMinWidth, m_context->mode());
      if (!minWidth)
        return false;
      CSSValue* maxWidth = minWidth;
      if (!m_range.atEnd()) {
        maxWidth = consumeSingleViewportDescriptor(m_range, CSSPropertyMaxWidth,
                                                   m_context->mode());
      }
      if (!maxWidth || !m_range.atEnd())
        return false;
      addProperty(CSSPropertyMinWidth, CSSPropertyInvalid, *minWidth,
                  important);
      addProperty(CSSPropertyMaxWidth, CSSPropertyInvalid, *maxWidth,
                  important);
      return true;
    }
    case CSSPropertyHeight: {
      CSSValue* minHeight = consumeSingleViewportDescriptor(
          m_range, CSSPropertyMinHeight, m_context->mode());
      if (!minHeight)
        return false;
      CSSValue* maxHeight = minHeight;
      if (!m_range.atEnd()) {
        maxHeight = consumeSingleViewportDescriptor(
            m_range, CSSPropertyMaxHeight, m_context->mode());
      }
      if (!maxHeight || !m_range.atEnd())
        return false;
      addProperty(CSSPropertyMinHeight, CSSPropertyInvalid, *minHeight,
                  important);
      addProperty(CSSPropertyMaxHeight, CSSPropertyInvalid, *maxHeight,
                  important);
      return true;
    }
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom:
    case CSSPropertyUserZoom:
    case CSSPropertyOrientation: {
      CSSValue* parsedValue =
          consumeSingleViewportDescriptor(m_range, propId, m_context->mode());
      if (!parsedValue || !m_range.atEnd())
        return false;
      addProperty(propId, CSSPropertyInvalid, *parsedValue, important);
      return true;
    }
    default:
      return false;
  }
}

bool CSSPropertyParser::consumeColumns(bool important) {
  CSSValue* columnWidth = nullptr;
  CSSValue* columnCount = nullptr;
  if (!CSSPropertyColumnUtils::consumeColumnWidthOrCount(m_range, columnWidth,
                                                         columnCount))
    return false;
  CSSPropertyColumnUtils::consumeColumnWidthOrCount(m_range, columnWidth,
                                                    columnCount);
  if (!m_range.atEnd())
    return false;
  if (!columnWidth)
    columnWidth = CSSIdentifierValue::create(CSSValueAuto);
  if (!columnCount)
    columnCount = CSSIdentifierValue::create(CSSValueAuto);
  addProperty(CSSPropertyColumnWidth, CSSPropertyInvalid, *columnWidth,
              important);
  addProperty(CSSPropertyColumnCount, CSSPropertyInvalid, *columnCount,
              important);
  return true;
}

bool CSSPropertyParser::consumeShorthandGreedily(
    const StylePropertyShorthand& shorthand,
    bool important) {
  // Existing shorthands have at most 6 longhands.
  DCHECK_LE(shorthand.length(), 6u);
  const CSSValue* longhands[6] = {nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr};
  const CSSPropertyID* shorthandProperties = shorthand.properties();
  do {
    bool foundLonghand = false;
    for (size_t i = 0; !foundLonghand && i < shorthand.length(); ++i) {
      if (longhands[i])
        continue;
      longhands[i] = parseSingleValue(shorthandProperties[i], shorthand.id());
      if (longhands[i])
        foundLonghand = true;
    }
    if (!foundLonghand)
      return false;
  } while (!m_range.atEnd());

  for (size_t i = 0; i < shorthand.length(); ++i) {
    if (longhands[i])
      addProperty(shorthandProperties[i], shorthand.id(), *longhands[i],
                  important);
    else
      addProperty(shorthandProperties[i], shorthand.id(),
                  *CSSInitialValue::create(), important);
  }
  return true;
}

bool CSSPropertyParser::consumeFlex(bool important) {
  static const double unsetValue = -1;
  double flexGrow = unsetValue;
  double flexShrink = unsetValue;
  CSSValue* flexBasis = nullptr;

  if (m_range.peek().id() == CSSValueNone) {
    flexGrow = 0;
    flexShrink = 0;
    flexBasis = CSSIdentifierValue::create(CSSValueAuto);
    m_range.consumeIncludingWhitespace();
  } else {
    unsigned index = 0;
    while (!m_range.atEnd() && index++ < 3) {
      double num;
      if (consumeNumberRaw(m_range, num)) {
        if (num < 0)
          return false;
        if (flexGrow == unsetValue)
          flexGrow = num;
        else if (flexShrink == unsetValue)
          flexShrink = num;
        else if (!num)  // flex only allows a basis of 0 (sans units) if
                        // flex-grow and flex-shrink values have already been
                        // set.
          flexBasis =
              CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
        else
          return false;
      } else if (!flexBasis) {
        if (m_range.peek().id() == CSSValueAuto)
          flexBasis = consumeIdent(m_range);
        if (!flexBasis)
          flexBasis = consumeLengthOrPercent(m_range, m_context->mode(),
                                             ValueRangeNonNegative);
        if (index == 2 && !m_range.atEnd())
          return false;
      }
    }
    if (index == 0)
      return false;
    if (flexGrow == unsetValue)
      flexGrow = 1;
    if (flexShrink == unsetValue)
      flexShrink = 1;
    if (!flexBasis)
      flexBasis =
          CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
  }

  if (!m_range.atEnd())
    return false;
  addProperty(CSSPropertyFlexGrow, CSSPropertyFlex,
              *CSSPrimitiveValue::create(clampTo<float>(flexGrow),
                                         CSSPrimitiveValue::UnitType::Number),
              important);
  addProperty(CSSPropertyFlexShrink, CSSPropertyFlex,
              *CSSPrimitiveValue::create(clampTo<float>(flexShrink),
                                         CSSPrimitiveValue::UnitType::Number),
              important);
  addProperty(CSSPropertyFlexBasis, CSSPropertyFlex, *flexBasis, important);
  return true;
}

bool CSSPropertyParser::consumeBorder(bool important) {
  CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  CSSValue* color = nullptr;

  while (!width || !style || !color) {
    if (!width) {
      width =
          consumeLineWidth(m_range, m_context->mode(), UnitlessQuirk::Forbid);
      if (width)
        continue;
    }
    if (!style) {
      style = parseSingleValue(CSSPropertyBorderLeftStyle, CSSPropertyBorder);
      if (style)
        continue;
    }
    if (!color) {
      color = consumeColor(m_range, m_context->mode());
      if (color)
        continue;
    }
    break;
  }

  if (!width && !style && !color)
    return false;

  if (!width)
    width = CSSInitialValue::create();
  if (!style)
    style = CSSInitialValue::create();
  if (!color)
    color = CSSInitialValue::create();

  addExpandedPropertyForValue(CSSPropertyBorderWidth, *width, important);
  addExpandedPropertyForValue(CSSPropertyBorderStyle, *style, important);
  addExpandedPropertyForValue(CSSPropertyBorderColor, *color, important);
  addExpandedPropertyForValue(CSSPropertyBorderImage,
                              *CSSInitialValue::create(), important);

  return m_range.atEnd();
}

bool CSSPropertyParser::consume4Values(const StylePropertyShorthand& shorthand,
                                       bool important) {
  ASSERT(shorthand.length() == 4);
  const CSSPropertyID* longhands = shorthand.properties();
  const CSSValue* top = parseSingleValue(longhands[0], shorthand.id());
  if (!top)
    return false;

  const CSSValue* right = parseSingleValue(longhands[1], shorthand.id());
  const CSSValue* bottom = nullptr;
  const CSSValue* left = nullptr;
  if (right) {
    bottom = parseSingleValue(longhands[2], shorthand.id());
    if (bottom)
      left = parseSingleValue(longhands[3], shorthand.id());
  }

  if (!right)
    right = top;
  if (!bottom)
    bottom = top;
  if (!left)
    left = right;

  addProperty(longhands[0], shorthand.id(), *top, important);
  addProperty(longhands[1], shorthand.id(), *right, important);
  addProperty(longhands[2], shorthand.id(), *bottom, important);
  addProperty(longhands[3], shorthand.id(), *left, important);

  return m_range.atEnd();
}

bool CSSPropertyParser::consumeBorderImage(CSSPropertyID property,
                                           bool important) {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;
  if (consumeBorderImageComponents(property, m_range, m_context, source, slice,
                                   width, outset, repeat)) {
    switch (property) {
      case CSSPropertyWebkitMaskBoxImage:
        addProperty(CSSPropertyWebkitMaskBoxImageSource,
                    CSSPropertyWebkitMaskBoxImage,
                    source ? *source : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyWebkitMaskBoxImageSlice,
                    CSSPropertyWebkitMaskBoxImage,
                    slice ? *slice : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyWebkitMaskBoxImageWidth,
                    CSSPropertyWebkitMaskBoxImage,
                    width ? *width : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyWebkitMaskBoxImageOutset,
                    CSSPropertyWebkitMaskBoxImage,
                    outset ? *outset : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyWebkitMaskBoxImageRepeat,
                    CSSPropertyWebkitMaskBoxImage,
                    repeat ? *repeat : *CSSInitialValue::create(), important);
        return true;
      case CSSPropertyBorderImage:
        addProperty(CSSPropertyBorderImageSource, CSSPropertyBorderImage,
                    source ? *source : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyBorderImageSlice, CSSPropertyBorderImage,
                    slice ? *slice : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyBorderImageWidth, CSSPropertyBorderImage,
                    width ? *width : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyBorderImageOutset, CSSPropertyBorderImage,
                    outset ? *outset : *CSSInitialValue::create(), important);
        addProperty(CSSPropertyBorderImageRepeat, CSSPropertyBorderImage,
                    repeat ? *repeat : *CSSInitialValue::create(), important);
        return true;
      default:
        ASSERT_NOT_REACHED();
        return false;
    }
  }
  return false;
}

static inline CSSValueID mapFromPageBreakBetween(CSSValueID value) {
  if (value == CSSValueAlways)
    return CSSValuePage;
  if (value == CSSValueAuto || value == CSSValueAvoid ||
      value == CSSValueLeft || value == CSSValueRight)
    return value;
  return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnBreakBetween(CSSValueID value) {
  if (value == CSSValueAlways)
    return CSSValueColumn;
  if (value == CSSValueAuto || value == CSSValueAvoid)
    return value;
  return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnOrPageBreakInside(CSSValueID value) {
  if (value == CSSValueAuto || value == CSSValueAvoid)
    return value;
  return CSSValueInvalid;
}

static inline CSSPropertyID mapFromLegacyBreakProperty(CSSPropertyID property) {
  if (property == CSSPropertyPageBreakAfter ||
      property == CSSPropertyWebkitColumnBreakAfter)
    return CSSPropertyBreakAfter;
  if (property == CSSPropertyPageBreakBefore ||
      property == CSSPropertyWebkitColumnBreakBefore)
    return CSSPropertyBreakBefore;
  ASSERT(property == CSSPropertyPageBreakInside ||
         property == CSSPropertyWebkitColumnBreakInside);
  return CSSPropertyBreakInside;
}

bool CSSPropertyParser::consumeLegacyBreakProperty(CSSPropertyID property,
                                                   bool important) {
  // The fragmentation spec says that page-break-(after|before|inside) are to be
  // treated as shorthands for their break-(after|before|inside) counterparts.
  // We'll do the same for the non-standard properties
  // -webkit-column-break-(after|before|inside).
  CSSIdentifierValue* keyword = consumeIdent(m_range);
  if (!keyword)
    return false;
  if (!m_range.atEnd())
    return false;
  CSSValueID value = keyword->getValueID();
  switch (property) {
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
      value = mapFromPageBreakBetween(value);
      break;
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
      value = mapFromColumnBreakBetween(value);
      break;
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakInside:
      value = mapFromColumnOrPageBreakInside(value);
      break;
    default:
      ASSERT_NOT_REACHED();
  }
  if (value == CSSValueInvalid)
    return false;

  CSSPropertyID genericBreakProperty = mapFromLegacyBreakProperty(property);
  addProperty(genericBreakProperty, property,
              *CSSIdentifierValue::create(value), important);
  return true;
}

static bool consumeBackgroundPosition(CSSParserTokenRange& range,
                                      const CSSParserContext* context,
                                      UnitlessQuirk unitless,
                                      CSSValue*& resultX,
                                      CSSValue*& resultY) {
  do {
    CSSValue* positionX = nullptr;
    CSSValue* positionY = nullptr;
    if (!consumePosition(range, context->mode(), unitless, positionX,
                         positionY))
      return false;
    addBackgroundValue(resultX, positionX);
    addBackgroundValue(resultY, positionY);
  } while (consumeCommaIncludingWhitespace(range));
  return true;
}

static bool consumeRepeatStyleComponent(CSSParserTokenRange& range,
                                        CSSValue*& value1,
                                        CSSValue*& value2,
                                        bool& implicit) {
  if (consumeIdent<CSSValueRepeatX>(range)) {
    value1 = CSSIdentifierValue::create(CSSValueRepeat);
    value2 = CSSIdentifierValue::create(CSSValueNoRepeat);
    implicit = true;
    return true;
  }
  if (consumeIdent<CSSValueRepeatY>(range)) {
    value1 = CSSIdentifierValue::create(CSSValueNoRepeat);
    value2 = CSSIdentifierValue::create(CSSValueRepeat);
    implicit = true;
    return true;
  }
  value1 = consumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound,
                        CSSValueSpace>(range);
  if (!value1)
    return false;

  value2 = consumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound,
                        CSSValueSpace>(range);
  if (!value2) {
    value2 = value1;
    implicit = true;
  }
  return true;
}

static bool consumeRepeatStyle(CSSParserTokenRange& range,
                               CSSValue*& resultX,
                               CSSValue*& resultY,
                               bool& implicit) {
  do {
    CSSValue* repeatX = nullptr;
    CSSValue* repeatY = nullptr;
    if (!consumeRepeatStyleComponent(range, repeatX, repeatY, implicit))
      return false;
    addBackgroundValue(resultX, repeatX);
    addBackgroundValue(resultY, repeatY);
  } while (consumeCommaIncludingWhitespace(range));
  return true;
}

// Note: consumeBackgroundShorthand assumes y properties (for example
// background-position-y) follow the x properties in the shorthand array.
bool CSSPropertyParser::consumeBackgroundShorthand(
    const StylePropertyShorthand& shorthand,
    bool important) {
  const unsigned longhandCount = shorthand.length();
  CSSValue* longhands[10] = {0};
  ASSERT(longhandCount <= 10);

  bool implicit = false;
  do {
    bool parsedLonghand[10] = {false};
    CSSValue* originValue = nullptr;
    do {
      bool foundProperty = false;
      for (size_t i = 0; i < longhandCount; ++i) {
        if (parsedLonghand[i])
          continue;

        CSSValue* value = nullptr;
        CSSValue* valueY = nullptr;
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundRepeatX ||
            property == CSSPropertyWebkitMaskRepeatX) {
          consumeRepeatStyleComponent(m_range, value, valueY, implicit);
        } else if (property == CSSPropertyBackgroundPositionX ||
                   property == CSSPropertyWebkitMaskPositionX) {
          if (!consumePosition(m_range, m_context->mode(),
                               UnitlessQuirk::Forbid, value, valueY))
            continue;
        } else if (property == CSSPropertyBackgroundSize ||
                   property == CSSPropertyWebkitMaskSize) {
          if (!consumeSlashIncludingWhitespace(m_range))
            continue;
          value = consumeBackgroundSize(property, m_range, m_context->mode());
          if (!value || !parsedLonghand[i - 1])  // Position must have been
                                                 // parsed in the current layer.
            return false;
        } else if (property == CSSPropertyBackgroundPositionY ||
                   property == CSSPropertyBackgroundRepeatY ||
                   property == CSSPropertyWebkitMaskPositionY ||
                   property == CSSPropertyWebkitMaskRepeatY) {
          continue;
        } else {
          value = consumeBackgroundComponent(property, m_range, m_context);
        }
        if (value) {
          if (property == CSSPropertyBackgroundOrigin ||
              property == CSSPropertyWebkitMaskOrigin)
            originValue = value;
          parsedLonghand[i] = true;
          foundProperty = true;
          addBackgroundValue(longhands[i], value);
          if (valueY) {
            parsedLonghand[i + 1] = true;
            addBackgroundValue(longhands[i + 1], valueY);
          }
        }
      }
      if (!foundProperty)
        return false;
    } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (size_t i = 0; i < longhandCount; ++i) {
      CSSPropertyID property = shorthand.properties()[i];
      if (property == CSSPropertyBackgroundColor && !m_range.atEnd()) {
        if (parsedLonghand[i])
          return false;  // Colors are only allowed in the last layer.
        continue;
      }
      if ((property == CSSPropertyBackgroundClip ||
           property == CSSPropertyWebkitMaskClip) &&
          !parsedLonghand[i] && originValue) {
        addBackgroundValue(longhands[i], originValue);
        continue;
      }
      if (!parsedLonghand[i])
        addBackgroundValue(longhands[i], CSSInitialValue::create());
    }
  } while (consumeCommaIncludingWhitespace(m_range));
  if (!m_range.atEnd())
    return false;

  for (size_t i = 0; i < longhandCount; ++i) {
    CSSPropertyID property = shorthand.properties()[i];
    if (property == CSSPropertyBackgroundSize && longhands[i] &&
        m_context->useLegacyBackgroundSizeShorthandBehavior())
      continue;
    addProperty(property, shorthand.id(), *longhands[i], important, implicit);
  }
  return true;
}

bool CSSPropertyParser::consumeGridItemPositionShorthand(
    CSSPropertyID shorthandId,
    bool important) {
  ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
  ASSERT(shorthand.length() == 2);
  CSSValue* startValue = consumeGridLine(m_range);
  if (!startValue)
    return false;

  CSSValue* endValue = nullptr;
  if (consumeSlashIncludingWhitespace(m_range)) {
    endValue = consumeGridLine(m_range);
    if (!endValue)
      return false;
  } else {
    endValue = startValue->isCustomIdentValue()
                   ? startValue
                   : CSSIdentifierValue::create(CSSValueAuto);
  }
  if (!m_range.atEnd())
    return false;
  addProperty(shorthand.properties()[0], shorthandId, *startValue, important);
  addProperty(shorthand.properties()[1], shorthandId, *endValue, important);
  return true;
}

bool CSSPropertyParser::consumeGridAreaShorthand(bool important) {
  ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  ASSERT(gridAreaShorthand().length() == 4);
  CSSValue* rowStartValue = consumeGridLine(m_range);
  if (!rowStartValue)
    return false;
  CSSValue* columnStartValue = nullptr;
  CSSValue* rowEndValue = nullptr;
  CSSValue* columnEndValue = nullptr;
  if (consumeSlashIncludingWhitespace(m_range)) {
    columnStartValue = consumeGridLine(m_range);
    if (!columnStartValue)
      return false;
    if (consumeSlashIncludingWhitespace(m_range)) {
      rowEndValue = consumeGridLine(m_range);
      if (!rowEndValue)
        return false;
      if (consumeSlashIncludingWhitespace(m_range)) {
        columnEndValue = consumeGridLine(m_range);
        if (!columnEndValue)
          return false;
      }
    }
  }
  if (!m_range.atEnd())
    return false;
  if (!columnStartValue) {
    columnStartValue = rowStartValue->isCustomIdentValue()
                           ? rowStartValue
                           : CSSIdentifierValue::create(CSSValueAuto);
  }
  if (!rowEndValue) {
    rowEndValue = rowStartValue->isCustomIdentValue()
                      ? rowStartValue
                      : CSSIdentifierValue::create(CSSValueAuto);
  }
  if (!columnEndValue) {
    columnEndValue = columnStartValue->isCustomIdentValue()
                         ? columnStartValue
                         : CSSIdentifierValue::create(CSSValueAuto);
  }

  addProperty(CSSPropertyGridRowStart, CSSPropertyGridArea, *rowStartValue,
              important);
  addProperty(CSSPropertyGridColumnStart, CSSPropertyGridArea,
              *columnStartValue, important);
  addProperty(CSSPropertyGridRowEnd, CSSPropertyGridArea, *rowEndValue,
              important);
  addProperty(CSSPropertyGridColumnEnd, CSSPropertyGridArea, *columnEndValue,
              important);
  return true;
}

bool CSSPropertyParser::consumeGridTemplateRowsAndAreasAndColumns(
    CSSPropertyID shorthandId,
    bool important) {
  NamedGridAreaMap gridAreaMap;
  size_t rowCount = 0;
  size_t columnCount = 0;
  CSSValueList* templateRows = CSSValueList::createSpaceSeparated();

  // Persists between loop iterations so we can use the same value for
  // consecutive <line-names> values
  CSSGridLineNamesValue* lineNames = nullptr;

  do {
    // Handle leading <custom-ident>*.
    bool hasPreviousLineNames = lineNames;
    lineNames = consumeGridLineNames(m_range, lineNames);
    if (lineNames && !hasPreviousLineNames)
      templateRows->append(*lineNames);

    // Handle a template-area's row.
    if (m_range.peek().type() != StringToken ||
        !parseGridTemplateAreasRow(
            m_range.consumeIncludingWhitespace().value().toString(),
            gridAreaMap, rowCount, columnCount))
      return false;
    ++rowCount;

    // Handle template-rows's track-size.
    CSSValue* value = consumeGridTrackSize(m_range, m_context->mode());
    if (!value)
      value = CSSIdentifierValue::create(CSSValueAuto);
    templateRows->append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    lineNames = consumeGridLineNames(m_range);
    if (lineNames)
      templateRows->append(*lineNames);
  } while (!m_range.atEnd() &&
           !(m_range.peek().type() == DelimiterToken &&
             m_range.peek().delimiter() == '/'));

  CSSValue* columnsValue = nullptr;
  if (!m_range.atEnd()) {
    if (!consumeSlashIncludingWhitespace(m_range))
      return false;
    columnsValue =
        consumeGridTrackList(m_range, m_context->mode(), GridTemplateNoRepeat);
    if (!columnsValue || !m_range.atEnd())
      return false;
  } else {
    columnsValue = CSSIdentifierValue::create(CSSValueNone);
  }
  addProperty(CSSPropertyGridTemplateRows, shorthandId, *templateRows,
              important);
  addProperty(CSSPropertyGridTemplateColumns, shorthandId, *columnsValue,
              important);
  addProperty(
      CSSPropertyGridTemplateAreas, shorthandId,
      *CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount),
      important);
  return true;
}

bool CSSPropertyParser::consumeGridTemplateShorthand(CSSPropertyID shorthandId,
                                                     bool important) {
  ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  ASSERT(gridTemplateShorthand().length() == 3);

  CSSParserTokenRange rangeCopy = m_range;
  CSSValue* rowsValue = consumeIdent<CSSValueNone>(m_range);

  // 1- 'none' case.
  if (rowsValue && m_range.atEnd()) {
    addProperty(CSSPropertyGridTemplateRows, shorthandId,
                *CSSIdentifierValue::create(CSSValueNone), important);
    addProperty(CSSPropertyGridTemplateColumns, shorthandId,
                *CSSIdentifierValue::create(CSSValueNone), important);
    addProperty(CSSPropertyGridTemplateAreas, shorthandId,
                *CSSIdentifierValue::create(CSSValueNone), important);
    return true;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!rowsValue)
    rowsValue = consumeGridTrackList(m_range, m_context->mode(), GridTemplate);

  if (rowsValue) {
    if (!consumeSlashIncludingWhitespace(m_range))
      return false;
    CSSValue* columnsValue =
        consumeGridTemplatesRowsOrColumns(m_range, m_context->mode());
    if (!columnsValue || !m_range.atEnd())
      return false;

    addProperty(CSSPropertyGridTemplateRows, shorthandId, *rowsValue,
                important);
    addProperty(CSSPropertyGridTemplateColumns, shorthandId, *columnsValue,
                important);
    addProperty(CSSPropertyGridTemplateAreas, shorthandId,
                *CSSIdentifierValue::create(CSSValueNone), important);
    return true;
  }

  // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+
  // [ / <track-list> ]?
  m_range = rangeCopy;
  return consumeGridTemplateRowsAndAreasAndColumns(shorthandId, important);
}

static CSSValueList* consumeImplicitAutoFlow(CSSParserTokenRange& range,
                                             const CSSValue& flowDirection) {
  // [ auto-flow && dense? ]
  CSSValue* denseAlgorithm = nullptr;
  if ((consumeIdent<CSSValueAutoFlow>(range))) {
    denseAlgorithm = consumeIdent<CSSValueDense>(range);
  } else {
    denseAlgorithm = consumeIdent<CSSValueDense>(range);
    if (!denseAlgorithm)
      return nullptr;
    if (!consumeIdent<CSSValueAutoFlow>(range))
      return nullptr;
  }
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(flowDirection);
  if (denseAlgorithm)
    list->append(*denseAlgorithm);
  return list;
}

bool CSSPropertyParser::consumeGridShorthand(bool important) {
  ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 8);

  CSSParserTokenRange rangeCopy = m_range;

  // 1- <grid-template>
  if (consumeGridTemplateShorthand(CSSPropertyGrid, important)) {
    // It can only be specified the explicit or the implicit grid properties in
    // a single grid declaration. The sub-properties not specified are set to
    // their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid,
                *CSSInitialValue::create(), important);
    addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid,
                *CSSInitialValue::create(), important);
    addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid,
                *CSSInitialValue::create(), important);
    addProperty(CSSPropertyGridColumnGap, CSSPropertyGrid,
                *CSSInitialValue::create(), important);
    addProperty(CSSPropertyGridRowGap, CSSPropertyGrid,
                *CSSInitialValue::create(), important);
    return true;
  }

  m_range = rangeCopy;

  CSSValue* autoColumnsValue = nullptr;
  CSSValue* autoRowsValue = nullptr;
  CSSValue* templateRows = nullptr;
  CSSValue* templateColumns = nullptr;
  CSSValueList* gridAutoFlow = nullptr;
  if (identMatches<CSSValueDense, CSSValueAutoFlow>(m_range.peek().id())) {
    // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
    gridAutoFlow = consumeImplicitAutoFlow(
        m_range, *CSSIdentifierValue::create(CSSValueRow));
    if (!gridAutoFlow)
      return false;
    if (consumeSlashIncludingWhitespace(m_range)) {
      autoRowsValue = CSSInitialValue::create();
    } else {
      autoRowsValue =
          consumeGridTrackList(m_range, m_context->mode(), GridAuto);
      if (!autoRowsValue)
        return false;
      if (!consumeSlashIncludingWhitespace(m_range))
        return false;
    }
    if (!(templateColumns =
              consumeGridTemplatesRowsOrColumns(m_range, m_context->mode())))
      return false;
    templateRows = CSSInitialValue::create();
    autoColumnsValue = CSSInitialValue::create();
  } else {
    // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
    templateRows =
        consumeGridTemplatesRowsOrColumns(m_range, m_context->mode());
    if (!templateRows)
      return false;
    if (!consumeSlashIncludingWhitespace(m_range))
      return false;
    gridAutoFlow = consumeImplicitAutoFlow(
        m_range, *CSSIdentifierValue::create(CSSValueColumn));
    if (!gridAutoFlow)
      return false;
    if (m_range.atEnd()) {
      autoColumnsValue = CSSInitialValue::create();
    } else {
      autoColumnsValue =
          consumeGridTrackList(m_range, m_context->mode(), GridAuto);
      if (!autoColumnsValue)
        return false;
    }
    templateColumns = CSSInitialValue::create();
    autoRowsValue = CSSInitialValue::create();
  }

  if (!m_range.atEnd())
    return false;

  // It can only be specified the explicit or the implicit grid properties in a
  // single grid declaration. The sub-properties not specified are set to their
  // initial value, as normal for shorthands.
  addProperty(CSSPropertyGridTemplateColumns, CSSPropertyGrid, *templateColumns,
              important);
  addProperty(CSSPropertyGridTemplateRows, CSSPropertyGrid, *templateRows,
              important);
  addProperty(CSSPropertyGridTemplateAreas, CSSPropertyGrid,
              *CSSInitialValue::create(), important);
  addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, *gridAutoFlow,
              important);
  addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, *autoColumnsValue,
              important);
  addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, *autoRowsValue,
              important);
  addProperty(CSSPropertyGridColumnGap, CSSPropertyGrid,
              *CSSInitialValue::create(), important);
  addProperty(CSSPropertyGridRowGap, CSSPropertyGrid,
              *CSSInitialValue::create(), important);
  return true;
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID unresolvedProperty,
                                       bool important) {
  CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);

  // Gets the parsing method for our current property from the property API.
  // If it has been implemented, we call this method, otherwise we manually
  // parse this value in the switch statement below. As we implement APIs for
  // other properties, those properties will be taken out of the switch
  // statement.
  const CSSPropertyDescriptor& cssPropertyDesc =
      CSSPropertyDescriptor::get(property);
  if (cssPropertyDesc.parseShorthand)
    return cssPropertyDesc.parseShorthand(important, m_range, m_context);

  switch (property) {
    case CSSPropertyWebkitMarginCollapse: {
      CSSValueID id = m_range.consumeIncludingWhitespace().id();
      if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(
              CSSPropertyWebkitMarginBeforeCollapse, id, m_context->mode()))
        return false;
      CSSValue* beforeCollapse = CSSIdentifierValue::create(id);
      addProperty(CSSPropertyWebkitMarginBeforeCollapse,
                  CSSPropertyWebkitMarginCollapse, *beforeCollapse, important);
      if (m_range.atEnd()) {
        addProperty(CSSPropertyWebkitMarginAfterCollapse,
                    CSSPropertyWebkitMarginCollapse, *beforeCollapse,
                    important);
        if (m_range.atEnd()) {
          addProperty(CSSPropertyWebkitMarginAfterCollapse,
                      CSSPropertyWebkitMarginCollapse, *beforeCollapse,
                      important);
          return true;
        }
        id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(
                CSSPropertyWebkitMarginAfterCollapse, id, m_context->mode()))
          return false;
        addProperty(CSSPropertyWebkitMarginAfterCollapse,
                    CSSPropertyWebkitMarginCollapse, *beforeCollapse,
                    important);
        return true;
      }
      id = m_range.consumeIncludingWhitespace().id();
      if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(
              CSSPropertyWebkitMarginAfterCollapse, id, m_context->mode()))
        return false;
      addProperty(CSSPropertyWebkitMarginAfterCollapse,
                  CSSPropertyWebkitMarginCollapse,
                  *CSSIdentifierValue::create(id), important);
      return true;
    }
    case CSSPropertyOverflow: {
      CSSValueID id = m_range.consumeIncludingWhitespace().id();
      if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(
              CSSPropertyOverflowY, id, m_context->mode()))
        return false;
      if (!m_range.atEnd())
        return false;
      CSSValue* overflowYValue = CSSIdentifierValue::create(id);

      CSSValue* overflowXValue = nullptr;

      // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y.
      // If
      // this value has been set using the shorthand, then for now overflow-x
      // will default to auto, but once we implement pagination controls, it
      // should default to hidden. If the overflow-y value is anything but
      // paged-x or paged-y, then overflow-x and overflow-y should have the
      // same
      // value.
      if (id == CSSValueWebkitPagedX || id == CSSValueWebkitPagedY)
        overflowXValue = CSSIdentifierValue::create(CSSValueAuto);
      else
        overflowXValue = overflowYValue;
      addProperty(CSSPropertyOverflowX, CSSPropertyOverflow, *overflowXValue,
                  important);
      addProperty(CSSPropertyOverflowY, CSSPropertyOverflow, *overflowYValue,
                  important);
      return true;
    }
    case CSSPropertyFont: {
      const CSSParserToken& token = m_range.peek();
      if (token.id() >= CSSValueCaption && token.id() <= CSSValueStatusBar)
        return consumeSystemFont(important);
      return consumeFont(important);
    }
    case CSSPropertyFontVariant:
      return consumeFontVariantShorthand(important);
    case CSSPropertyBorderSpacing:
      return consumeBorderSpacing(important);
    case CSSPropertyColumns:
      return consumeColumns(important);
    case CSSPropertyAnimation:
      return consumeAnimationShorthand(
          animationShorthandForParsing(),
          unresolvedProperty == CSSPropertyAliasWebkitAnimation, important);
    case CSSPropertyTransition:
      return consumeAnimationShorthand(transitionShorthandForParsing(), false,
                                       important);
    case CSSPropertyTextDecoration:
      ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
      return consumeShorthandGreedily(textDecorationShorthand(), important);
    case CSSPropertyMargin:
      return consume4Values(marginShorthand(), important);
    case CSSPropertyPadding:
      return consume4Values(paddingShorthand(), important);
    case CSSPropertyMotion:
      return consumeShorthandGreedily(motionShorthand(), important);
    case CSSPropertyOffset:
      return consumeOffsetShorthand(important);
    case CSSPropertyWebkitTextEmphasis:
      return consumeShorthandGreedily(webkitTextEmphasisShorthand(), important);
    case CSSPropertyOutline:
      return consumeShorthandGreedily(outlineShorthand(), important);
    case CSSPropertyWebkitBorderStart:
      return consumeShorthandGreedily(webkitBorderStartShorthand(), important);
    case CSSPropertyWebkitBorderEnd:
      return consumeShorthandGreedily(webkitBorderEndShorthand(), important);
    case CSSPropertyWebkitBorderBefore:
      return consumeShorthandGreedily(webkitBorderBeforeShorthand(), important);
    case CSSPropertyWebkitBorderAfter:
      return consumeShorthandGreedily(webkitBorderAfterShorthand(), important);
    case CSSPropertyWebkitTextStroke:
      return consumeShorthandGreedily(webkitTextStrokeShorthand(), important);
    case CSSPropertyMarker: {
      const CSSValue* marker = parseSingleValue(CSSPropertyMarkerStart);
      if (!marker || !m_range.atEnd())
        return false;
      addProperty(CSSPropertyMarkerStart, CSSPropertyMarker, *marker,
                  important);
      addProperty(CSSPropertyMarkerMid, CSSPropertyMarker, *marker, important);
      addProperty(CSSPropertyMarkerEnd, CSSPropertyMarker, *marker, important);
      return true;
    }
    case CSSPropertyFlex:
      return consumeFlex(important);
    case CSSPropertyFlexFlow:
      return consumeShorthandGreedily(flexFlowShorthand(), important);
    case CSSPropertyColumnRule:
      return consumeShorthandGreedily(columnRuleShorthand(), important);
    case CSSPropertyListStyle:
      return consumeShorthandGreedily(listStyleShorthand(), important);
    case CSSPropertyBorderRadius: {
      CSSValue* horizontalRadii[4] = {0};
      CSSValue* verticalRadii[4] = {0};
      if (!CSSPropertyShapeUtils::consumeRadii(
              horizontalRadii, verticalRadii, m_range, m_context->mode(),
              unresolvedProperty == CSSPropertyAliasWebkitBorderRadius))
        return false;
      addProperty(CSSPropertyBorderTopLeftRadius, CSSPropertyBorderRadius,
                  *CSSValuePair::create(horizontalRadii[0], verticalRadii[0],
                                        CSSValuePair::DropIdenticalValues),
                  important);
      addProperty(CSSPropertyBorderTopRightRadius, CSSPropertyBorderRadius,
                  *CSSValuePair::create(horizontalRadii[1], verticalRadii[1],
                                        CSSValuePair::DropIdenticalValues),
                  important);
      addProperty(CSSPropertyBorderBottomRightRadius, CSSPropertyBorderRadius,
                  *CSSValuePair::create(horizontalRadii[2], verticalRadii[2],
                                        CSSValuePair::DropIdenticalValues),
                  important);
      addProperty(CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderRadius,
                  *CSSValuePair::create(horizontalRadii[3], verticalRadii[3],
                                        CSSValuePair::DropIdenticalValues),
                  important);
      return true;
    }
    case CSSPropertyBorderColor:
      return consume4Values(borderColorShorthand(), important);
    case CSSPropertyBorderStyle:
      return consume4Values(borderStyleShorthand(), important);
    case CSSPropertyBorderWidth:
      return consume4Values(borderWidthShorthand(), important);
    case CSSPropertyBorderTop:
      return consumeShorthandGreedily(borderTopShorthand(), important);
    case CSSPropertyBorderRight:
      return consumeShorthandGreedily(borderRightShorthand(), important);
    case CSSPropertyBorderBottom:
      return consumeShorthandGreedily(borderBottomShorthand(), important);
    case CSSPropertyBorderLeft:
      return consumeShorthandGreedily(borderLeftShorthand(), important);
    case CSSPropertyBorder:
      return consumeBorder(important);
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
      return consumeBorderImage(property, important);
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
      return consumeLegacyBreakProperty(property, important);
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyBackgroundPosition: {
      CSSValue* resultX = nullptr;
      CSSValue* resultY = nullptr;
      if (!consumeBackgroundPosition(m_range, m_context, UnitlessQuirk::Allow,
                                     resultX, resultY) ||
          !m_range.atEnd())
        return false;
      addProperty(property == CSSPropertyBackgroundPosition
                      ? CSSPropertyBackgroundPositionX
                      : CSSPropertyWebkitMaskPositionX,
                  property, *resultX, important);
      addProperty(property == CSSPropertyBackgroundPosition
                      ? CSSPropertyBackgroundPositionY
                      : CSSPropertyWebkitMaskPositionY,
                  property, *resultY, important);
      return true;
    }
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
      CSSValue* resultX = nullptr;
      CSSValue* resultY = nullptr;
      bool implicit = false;
      if (!consumeRepeatStyle(m_range, resultX, resultY, implicit) ||
          !m_range.atEnd())
        return false;
      addProperty(property == CSSPropertyBackgroundRepeat
                      ? CSSPropertyBackgroundRepeatX
                      : CSSPropertyWebkitMaskRepeatX,
                  property, *resultX, important, implicit);
      addProperty(property == CSSPropertyBackgroundRepeat
                      ? CSSPropertyBackgroundRepeatY
                      : CSSPropertyWebkitMaskRepeatY,
                  property, *resultY, important, implicit);
      return true;
    }
    case CSSPropertyBackground:
      return consumeBackgroundShorthand(backgroundShorthand(), important);
    case CSSPropertyWebkitMask:
      return consumeBackgroundShorthand(webkitMaskShorthand(), important);
    case CSSPropertyGridGap: {
      ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled() &&
             shorthandForProperty(CSSPropertyGridGap).length() == 2);
      CSSValue* rowGap = consumeLengthOrPercent(m_range, m_context->mode(),
                                                ValueRangeNonNegative);
      CSSValue* columnGap = consumeLengthOrPercent(m_range, m_context->mode(),
                                                   ValueRangeNonNegative);
      if (!rowGap || !m_range.atEnd())
        return false;
      if (!columnGap)
        columnGap = rowGap;
      addProperty(CSSPropertyGridRowGap, CSSPropertyGridGap, *rowGap,
                  important);
      addProperty(CSSPropertyGridColumnGap, CSSPropertyGridGap, *columnGap,
                  important);
      return true;
    }
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
      return consumeGridItemPositionShorthand(property, important);
    case CSSPropertyGridArea:
      return consumeGridAreaShorthand(important);
    case CSSPropertyGridTemplate:
      return consumeGridTemplateShorthand(CSSPropertyGridTemplate, important);
    case CSSPropertyGrid:
      return consumeGridShorthand(important);
    default:
      return false;
  }
}

}  // namespace blink
