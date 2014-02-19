/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSPropertyParser_h
#define CSSPropertyParser_h

// FIXME: Way too many.
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSFilterValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSParserMode.h"
#include "core/css/CSSParserValues.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSSelector.h"
#include "platform/graphics/Color.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

// FIXME: Many of these may not be used.
class AnimationParseContext;
class CSSArrayFunctionValue;
class CSSBorderImageSliceValue;
class CSSPrimitiveValue;
class CSSSelectorList;
class CSSValue;
class CSSValueList;
class CSSBasicShape;
class CSSBasicShapeInset;
class Document;
class Element;
class ImmutableStylePropertySet;
class StyleKeyframe;
class StylePropertyShorthand;
class StyleKeyframe;
class UseCounter;

// Inputs: PropertyID, isImportant bool, CSSParserValueList.
// Outputs: Vector of CSSProperties

class CSSPropertyParser {
public:
    typedef Vector<CSSProperty, 256> ParsedPropertyVector;

    CSSPropertyParser(OwnPtr<CSSParserValueList>&,
        const CSSParserContext&, bool inViewport, bool savedImportant,
        ParsedPropertyVector&, bool& hasFontFaceOnlyValues);
    ~CSSPropertyParser();

    // FIXME: Should this be on a separate ColorParser object?
    template<typename StringType>
    static bool fastParseColor(RGBA32&, const StringType&, bool strict);

    bool parseValue(CSSPropertyID, bool important);

private:
    bool inShorthand() const { return m_inParseShorthand; }
    bool inQuirksMode() const { return isQuirksModeBehavior(m_context.mode()); }

    bool inViewport() const { return m_inViewport; }
    bool parseViewportProperty(CSSPropertyID propId, bool important);
    bool parseViewportShorthand(CSSPropertyID propId, CSSPropertyID first, CSSPropertyID second, bool important);

    KURL completeURL(const String& url) const;

    void addPropertyWithPrefixingVariant(CSSPropertyID, PassRefPtr<CSSValue>, bool important, bool implicit = false);
    void addProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important, bool implicit = false);
    void rollbackLastProperties(int num);
    bool hasProperties() const { return !m_parsedProperties.isEmpty(); }
    void addExpandedPropertyForValue(CSSPropertyID propId, PassRefPtr<CSSValue>, bool);

    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseValidPrimitive(CSSValueID ident, CSSParserValue*);

    bool parseShorthand(CSSPropertyID, const StylePropertyShorthand&, bool important);
    bool parse4Values(CSSPropertyID, const CSSPropertyID* properties, bool important);
    bool parseContent(CSSPropertyID, bool important);
    bool parseQuotes(CSSPropertyID, bool important);

    PassRefPtr<CSSValue> parseAttr(CSSParserValueList* args);

    PassRefPtr<CSSValue> parseBackgroundColor();

    bool parseFillImage(CSSParserValueList*, RefPtr<CSSValue>&);

    enum FillPositionFlag { InvalidFillPosition = 0, AmbiguousFillPosition = 1, XFillPosition = 2, YFillPosition = 4 };
    enum FillPositionParsingMode { ResolveValuesAsPercent = 0, ResolveValuesAsKeyword = 1 };
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseFillPositionComponent(CSSParserValueList*, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode = ResolveValuesAsPercent);
    PassRefPtr<CSSValue> parseFillPositionX(CSSParserValueList*);
    PassRefPtr<CSSValue> parseFillPositionY(CSSParserValueList*);
    void parse2ValuesFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool isPotentialPositionValue(CSSParserValue*);
    void parseFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    void parse3ValuesFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>);
    void parse4ValuesFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>);

    void parseFillRepeat(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    PassRefPtr<CSSValue> parseFillSize(CSSPropertyID, bool &allowComma);

    bool parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseFillShorthand(CSSPropertyID, const CSSPropertyID* properties, int numProperties, bool important);

    void addFillValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

    void addAnimationValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

    PassRefPtr<CSSValue> parseAnimationDelay();
    PassRefPtr<CSSValue> parseAnimationDirection();
    PassRefPtr<CSSValue> parseAnimationDuration();
    PassRefPtr<CSSValue> parseAnimationFillMode();
    PassRefPtr<CSSValue> parseAnimationIterationCount();
    PassRefPtr<CSSValue> parseAnimationName();
    PassRefPtr<CSSValue> parseAnimationPlayState();
    PassRefPtr<CSSValue> parseAnimationProperty(AnimationParseContext&);
    PassRefPtr<CSSValue> parseAnimationTimingFunction();

    bool parseTransformOriginShorthand(RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseCubicBezierTimingFunctionValue(CSSParserValueList*& args, double& result);
    bool parseAnimationProperty(CSSPropertyID, RefPtr<CSSValue>&, AnimationParseContext&);
    bool parseTransitionShorthand(CSSPropertyID, bool important);
    bool parseAnimationShorthand(CSSPropertyID, bool important);

    PassRefPtr<CSSValue> parseColumnWidth();
    PassRefPtr<CSSValue> parseColumnCount();
    bool parseColumnsShorthand(bool important);

    PassRefPtr<CSSValue> parseGridPosition();
    bool parseIntegerOrStringFromGridPosition(RefPtrWillBeRawPtr<CSSPrimitiveValue>& numericValue, RefPtrWillBeRawPtr<CSSPrimitiveValue>& gridLineName);
    bool parseGridItemPositionShorthand(CSSPropertyID, bool important);
    bool parseGridAreaShorthand(bool important);
    bool parseSingleGridAreaLonghand(RefPtr<CSSValue>&);
    bool parseGridTrackList(CSSPropertyID, bool important);
    bool parseGridTrackRepeatFunction(CSSValueList&);
    PassRefPtr<CSSValue> parseGridTrackSize(CSSParserValueList& inputList);
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseGridBreadth(CSSParserValue*);
    PassRefPtr<CSSValue> parseGridTemplateAreas();
    void parseGridLineNames(CSSParserValueList* inputList, CSSValueList&);

    bool parseClipShape(CSSPropertyID, bool important);

    bool parseItemPositionOverflowPosition(CSSPropertyID, bool important);

    PassRefPtr<CSSValue> parseShapeProperty(CSSPropertyID propId);
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseBasicShape();
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseShapeRadius(CSSParserValue*);

    PassRefPtrWillBeRawPtr<CSSBasicShape> parseBasicShapeRectangle(CSSParserValueList* args);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseBasicShapeCircle(CSSParserValueList* args);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseDeprecatedBasicShapeCircle(CSSParserValueList* args);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseBasicShapeEllipse(CSSParserValueList* args);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseDeprecatedBasicShapeEllipse(CSSParserValueList*);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseBasicShapePolygon(CSSParserValueList* args);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseBasicShapeInsetRectangle(CSSParserValueList* args);
    PassRefPtrWillBeRawPtr<CSSBasicShape> parseBasicShapeInset(CSSParserValueList* args);

    bool parseFont(bool important);
    PassRefPtrWillBeRawPtr<CSSValueList> parseFontFamily();

    bool parseCounter(CSSPropertyID, int defaultValue, bool important);
    PassRefPtr<CSSValue> parseCounterContent(CSSParserValueList* args, bool counters);

    bool parseColorParameters(CSSParserValue*, int* colorValues, bool parseAlpha);
    bool parseHSLParameters(CSSParserValue*, double* colorValues, bool parseAlpha);
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseColor(CSSParserValue* = 0);
    bool parseColorFromValue(CSSParserValue*, RGBA32&);

    bool parseLineHeight(bool important);
    bool parseFontSize(bool important);
    bool parseFontVariant(bool important);
    bool parseFontWeight(bool important);
    bool parseFontFaceSrc();
    bool parseFontFaceUnicodeRange();

    bool parseSVGValue(CSSPropertyID propId, bool important);
    PassRefPtr<CSSValue> parseSVGPaint();
    PassRefPtr<CSSValue> parseSVGColor();
    PassRefPtr<CSSValue> parseSVGStrokeDasharray();

    PassRefPtr<CSSValue> parsePaintOrder() const;

    // CSS3 Parsing Routines (for properties specific to CSS3)
    PassRefPtrWillBeRawPtr<CSSValueList> parseShadow(CSSParserValueList*, CSSPropertyID);
    bool parseBorderImageShorthand(CSSPropertyID, bool important);
    PassRefPtr<CSSValue> parseBorderImage(CSSPropertyID);
    bool parseBorderImageRepeat(RefPtr<CSSValue>&);
    bool parseBorderImageSlice(CSSPropertyID, RefPtrWillBeRawPtr<CSSBorderImageSliceValue>&);
    bool parseBorderImageWidth(RefPtrWillBeRawPtr<CSSPrimitiveValue>&);
    bool parseBorderImageOutset(RefPtrWillBeRawPtr<CSSPrimitiveValue>&);
    bool parseBorderRadius(CSSPropertyID, bool important);

    bool parseAspectRatio(bool important);

    bool parseReflect(CSSPropertyID, bool important);

    bool parseFlex(CSSParserValueList* args, bool important);

    bool parseObjectPosition(bool important);

    // Image generators
    bool parseCanvas(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseDeprecatedGradient(CSSParserValueList*, RefPtr<CSSValue>&);
    bool parseDeprecatedLinearGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseDeprecatedRadialGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseLinearGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseRadialGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseGradientColorStops(CSSParserValueList*, CSSGradientValue*, bool expectComma);

    bool parseCrossfade(CSSParserValueList*, RefPtr<CSSValue>&);

    PassRefPtr<CSSValue> parseImageSet(CSSParserValueList*);

    bool parseWillChange(bool important);

    PassRefPtrWillBeRawPtr<CSSValueList> parseFilter();
    PassRefPtrWillBeRawPtr<CSSFilterValue> parseBuiltinFilterArguments(CSSParserValueList*, CSSFilterValue::FilterOperationType);

    static bool isBlendMode(CSSValueID);
    static bool isCompositeOperator(CSSValueID);

    PassRefPtrWillBeRawPtr<CSSValueList> parseTransform();
    PassRefPtr<CSSValue> parseTransformValue(CSSParserValue*);
    bool parseTransformOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, CSSPropertyID& propId3, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parsePerspectiveOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,  RefPtr<CSSValue>&, RefPtr<CSSValue>&);

    bool parseTextEmphasisStyle(bool important);

    bool parseTouchAction(bool important);

    void addTextDecorationProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important);
    bool parseTextDecoration(CSSPropertyID propId, bool important);
    bool parseTextUnderlinePosition(bool important);

    PassRefPtr<CSSValue> parseTextIndent();

    bool parseLineBoxContain(bool important);
    bool parseCalculation(CSSParserValue*, ValueRange);

    bool parseFontFeatureTag(CSSValueList*);
    bool parseFontFeatureSettings(bool important);

    bool parseFontVariantLigatures(bool important);

    bool parseGeneratedImage(CSSParserValueList*, RefPtr<CSSValue>&);

    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createPrimitiveNumericValue(CSSParserValue*);
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createPrimitiveStringValue(CSSParserValue*);

    bool validWidthOrHeight(CSSParserValue*);

    PassRefPtrWillBeRawPtr<CSSBasicShape> parseInsetRoundedCorners(PassRefPtrWillBeRawPtr<CSSBasicShapeInset>, CSSParserValueList*);

    enum SizeParameterType {
        None,
        Auto,
        Length,
        PageSize,
        Orientation,
    };

    bool parsePage(CSSPropertyID propId, bool important);
    bool parseSize(CSSPropertyID propId, bool important);
    SizeParameterType parseSizeParameter(CSSValueList* parsedValues, CSSParserValue*, SizeParameterType prevParamType);

    bool parseFontFaceSrcURI(CSSValueList*);
    bool parseFontFaceSrcLocal(CSSValueList*);

    enum PropertyType {
        PropertyExplicit,
        PropertyImplicit
    };

    class ImplicitScope {
        WTF_MAKE_NONCOPYABLE(ImplicitScope);
    public:
        ImplicitScope(CSSPropertyParser* parser, PropertyType propertyType)
            : m_parser(parser)
        {
            m_parser->m_implicitShorthand = propertyType == CSSPropertyParser::PropertyImplicit;
        }

        ~ImplicitScope()
        {
            m_parser->m_implicitShorthand = false;
        }

    private:
        CSSPropertyParser* m_parser;
    };

    // FIXME: MSVC doesn't like ShorthandScope being private
    // since ~OwnPtr can't access its destructor if non-inlined.
public:
    class ShorthandScope {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ShorthandScope(CSSPropertyParser* parser, CSSPropertyID propId) : m_parser(parser)
        {
            if (!(m_parser->m_inParseShorthand++))
                m_parser->m_currentShorthand = propId;
        }
        ~ShorthandScope()
        {
            if (!(--m_parser->m_inParseShorthand))
                m_parser->m_currentShorthand = CSSPropertyInvalid;
        }

    private:
        CSSPropertyParser* m_parser;
    };

private:
    enum ReleaseParsedCalcValueCondition {
        ReleaseParsedCalcValue,
        DoNotReleaseParsedCalcValue
    };

    enum Units {
        FUnknown = 0x0000,
        FInteger = 0x0001,
        FNumber = 0x0002, // Real Numbers
        FPercent = 0x0004,
        FLength = 0x0008,
        FAngle = 0x0010,
        FTime = 0x0020,
        FFrequency = 0x0040,
        FPositiveInteger = 0x0080,
        FRelative = 0x0100,
        FResolution = 0x0200,
        FNonNeg = 0x0400
    };

    friend inline Units operator|(Units a, Units b)
    {
        return static_cast<Units>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    bool validCalculationUnit(CSSParserValue*, Units, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);

    bool shouldAcceptUnitLessValues(CSSParserValue*, Units, CSSParserMode);

    inline bool validUnit(CSSParserValue* value, Units unitflags, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue) { return validUnit(value, unitflags, m_context.mode(), releaseCalc); }
    bool validUnit(CSSParserValue*, Units, CSSParserMode, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);

    bool parseBorderImageQuad(Units, RefPtrWillBeRawPtr<CSSPrimitiveValue>&);
    int colorIntFromValue(CSSParserValue*);
    double parsedDouble(CSSParserValue*, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);
    bool isCalculation(CSSParserValue*);

private:
    // Inputs:
    // FIXME: This should not be an OwnPtr&, many callers will need to be changed.
    const OwnPtr<CSSParserValueList>& m_valueList;
    const CSSParserContext& m_context;
    const bool m_inViewport;
    const bool m_important; // FIXME: This is only used by font-face-src and unicode-range and undoubtably wrong!

    // Outputs:
    ParsedPropertyVector& m_parsedProperties;
    bool m_hasFontFaceOnlyValues;

    // Locals during parsing:
    int m_inParseShorthand;
    CSSPropertyID m_currentShorthand;
    bool m_implicitShorthand;
    RefPtrWillBePersistent<CSSCalcValue> m_parsedCalculation;

    // FIXME: There is probably a small set of APIs we could expose for these
    // classes w/o needing to make them friends.
    friend class ShadowParseContext;
    friend class BorderImageParseContext;
    friend class BorderImageSliceParseContext;
    friend class BorderImageQuadParseContext;
    friend class TransformOperationInfo;
    friend bool parseDeprecatedGradientColorStop(CSSPropertyParser*, CSSParserValue*, CSSGradientColorStop&);
    friend PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseGradientColorOrKeyword(CSSPropertyParser*, CSSParserValue*);
};

CSSPropertyID cssPropertyID(const CSSParserString&);
CSSPropertyID cssPropertyID(const String&);
CSSValueID cssValueKeywordID(const CSSParserString&);

bool isValidNthToken(const CSSParserString&);

} // namespace WebCore

#endif // CSSPropertyParser_h
