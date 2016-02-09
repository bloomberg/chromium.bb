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

#include "core/css/CSSColorValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/Length.h"

namespace blink {

class CSSCustomIdentValue;
class CSSFunctionValue;
class CSSGradientValue;
class CSSGridLineNamesValue;
struct CSSParserString;
struct CSSParserValue;
class CSSParserValueList;
class CSSPrimitiveValue;
class CSSProperty;
class CSSValue;
class CSSValueList;
class StylePropertyShorthand;

enum class UnitlessQuirk {
    Allow,
    Forbid
};

// Inputs: PropertyID, isImportant bool, CSSParserValueList.
// Outputs: Vector of CSSProperties

class CSSPropertyParser {
    STACK_ALLOCATED();
public:

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
        FNonNeg = 0x0400,
        FUnitlessQuirk = 0x0800
    };

    static bool parseValue(CSSPropertyID, bool important,
        const CSSParserTokenRange&, const CSSParserContext&,
        WillBeHeapVector<CSSProperty, 256>&, StyleRule::Type);

    static bool isSystemColor(CSSValueID);
    static bool isColorKeyword(CSSValueID);
    static bool isValidNumericValue(double);

    // TODO(rwlbuis): move to CSSPropertyParser.cpp once CSSParserToken conversion is done.
    static PassRefPtrWillBeRawPtr<CSSValue> createCSSImageValueWithReferrer(const AtomicString& rawValue, const CSSParserContext& context)
    {
        RefPtrWillBeRawPtr<CSSValue> imageValue = CSSImageValue::create(rawValue, context.completeURL(rawValue));
        toCSSImageValue(imageValue.get())->setReferrer(context.referrer());
        return imageValue;
    }

    // TODO(rwlbuis): move to CSSPropertyParser.cpp once CSSParserToken conversion is done.
    static bool isGeneratedImage(CSSValueID id)
    {
        return id == CSSValueLinearGradient || id == CSSValueRadialGradient
            || id == CSSValueRepeatingLinearGradient || id == CSSValueRepeatingRadialGradient
            || id == CSSValueWebkitLinearGradient || id == CSSValueWebkitRadialGradient
            || id == CSSValueWebkitRepeatingLinearGradient || id == CSSValueWebkitRepeatingRadialGradient
            || id == CSSValueWebkitGradient || id == CSSValueWebkitCrossFade;
    }

private:
    CSSPropertyParser(const CSSParserTokenRange&, const CSSParserContext&,
        WillBeHeapVector<CSSProperty, 256>&);

    // TODO(timloh): Rename once the CSSParserValue-based parseValue is removed
    bool parseValueStart(CSSPropertyID unresolvedProperty, bool important);
    bool consumeCSSWideKeyword(CSSPropertyID unresolvedProperty, bool important);
    PassRefPtrWillBeRawPtr<CSSValue> parseSingleValue(CSSPropertyID);

    PassRefPtrWillBeRawPtr<CSSValue> legacyParseValue(CSSPropertyID);
    bool legacyParseAndApplyValue(CSSPropertyID, bool important);
    bool legacyParseShorthand(CSSPropertyID, bool important);

    bool inShorthand() const { return m_inParseShorthand; }
    bool inQuirksMode() const { return isQuirksModeBehavior(m_context.mode()); }

    bool parseViewportDescriptor(CSSPropertyID propId, bool important);
    bool parseFontFaceDescriptor(CSSPropertyID);

    void addProperty(CSSPropertyID, PassRefPtrWillBeRawPtr<CSSValue>, bool important, bool implicit = false);
    void addExpandedPropertyForValue(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue>, bool);

    bool consumeBorder(bool important);

    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseValidPrimitive(CSSValueID ident, CSSParserValue*);

    bool parseShorthand(CSSPropertyID, bool important);
    bool consumeShorthandGreedily(const StylePropertyShorthand&, bool important);
    bool consume4Values(const StylePropertyShorthand&, bool important);

    bool parseFillImage(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&);

    enum FillPositionFlag { InvalidFillPosition = 0, AmbiguousFillPosition = 1, XFillPosition = 2, YFillPosition = 4 };
    enum FillPositionParsingMode { ResolveValuesAsPercent = 0, ResolveValuesAsKeyword = 1 };
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseFillPositionComponent(CSSParserValueList*, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode = ResolveValuesAsPercent, Units = FUnknown);
    PassRefPtrWillBeRawPtr<CSSValue> parseFillPositionX(CSSParserValueList*);
    PassRefPtrWillBeRawPtr<CSSValue> parseFillPositionY(CSSParserValueList*);
    void parse2ValuesFillPosition(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, RefPtrWillBeRawPtr<CSSValue>&, Units = FUnknown);
    bool isPotentialPositionValue(CSSParserValue*);
    void parseFillPosition(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, RefPtrWillBeRawPtr<CSSValue>&, Units = FUnknown);
    void parse3ValuesFillPosition(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, RefPtrWillBeRawPtr<CSSValue>&, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>);
    void parse4ValuesFillPosition(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, RefPtrWillBeRawPtr<CSSValue>&, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>, PassRefPtrWillBeRawPtr<CSSPrimitiveValue>);

    void parseFillRepeat(RefPtrWillBeRawPtr<CSSValue>&, RefPtrWillBeRawPtr<CSSValue>&);
    PassRefPtrWillBeRawPtr<CSSValue> parseFillSize(CSSPropertyID);

    bool parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtrWillBeRawPtr<CSSValue>&, RefPtrWillBeRawPtr<CSSValue>&);
    bool parseFillShorthand(CSSPropertyID, const CSSPropertyID* properties, int numProperties, bool important);

    void addFillValue(RefPtrWillBeRawPtr<CSSValue>& lval, PassRefPtrWillBeRawPtr<CSSValue> rval);

    // Legacy parsing allows <string>s for animation-name
    bool consumeAnimationShorthand(const StylePropertyShorthand&, bool useLegacyParsing, bool important);

    bool consumeColumns(bool important);

    enum TrackSizeRestriction { FixedSizeOnly, AllowAll };
    PassRefPtrWillBeRawPtr<CSSValue> parseGridPosition();
    bool parseIntegerOrCustomIdentFromGridPosition(RefPtrWillBeRawPtr<CSSPrimitiveValue>& numericValue, RefPtrWillBeRawPtr<CSSCustomIdentValue>& gridLineName);
    bool parseGridItemPositionShorthand(CSSPropertyID, bool important);
    bool parseGridTemplateRowsAndAreas(PassRefPtrWillBeRawPtr<CSSValue>, bool important);
    bool parseGridTemplateShorthand(bool important);
    bool parseGridShorthand(bool important);
    bool parseGridAreaShorthand(bool important);
    bool parseGridGapShorthand(bool important);
    bool parseSingleGridAreaLonghand(RefPtrWillBeRawPtr<CSSValue>&);
    PassRefPtrWillBeRawPtr<CSSValue> parseGridTrackList();
    bool parseGridTrackRepeatFunction(CSSValueList&, bool& isAutoRepeat);
    PassRefPtrWillBeRawPtr<CSSValue> parseGridTrackSize(CSSParserValueList& inputList, TrackSizeRestriction = AllowAll);
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseGridBreadth(CSSParserValue*, TrackSizeRestriction = AllowAll);
    bool parseGridTemplateAreasRow(NamedGridAreaMap&, const size_t, size_t&);
    PassRefPtrWillBeRawPtr<CSSValue> parseGridTemplateAreas();
    bool parseGridLineNames(CSSParserValueList&, CSSValueList&, CSSGridLineNamesValue* = nullptr);
    PassRefPtrWillBeRawPtr<CSSValue> parseGridAutoFlow(CSSParserValueList&);

    PassRefPtrWillBeRawPtr<CSSValue> parseLegacyPosition();
    PassRefPtrWillBeRawPtr<CSSValue> parseItemPositionOverflowPosition();

    bool consumeFont(bool important);
    bool consumeSystemFont(bool important);

    bool consumeBorderSpacing(bool important);

    bool parseColorParameters(const CSSParserValue*, int* colorValues, bool parseAlpha);
    bool parseHSLParameters(const CSSParserValue*, double* colorValues, bool parseAlpha);
    PassRefPtrWillBeRawPtr<CSSValue> parseColor(const CSSParserValue*, bool acceptQuirkyColors = false);
    bool parseColorFromValue(const CSSParserValue*, RGBA32&, bool acceptQuirkyColors = false);

    // CSS3 Parsing Routines (for properties specific to CSS3)
    bool consumeBorderImage(CSSPropertyID, bool important);

    bool consumeFlex(bool important);

    // Image generators
    bool parseDeprecatedGradient(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&);
    bool parseDeprecatedLinearGradient(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseDeprecatedRadialGradient(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseLinearGradient(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseRadialGradient(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseGradientColorStops(CSSParserValueList*, CSSGradientValue*, bool expectComma);

    bool parseCrossfade(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&);

    PassRefPtrWillBeRawPtr<CSSValue> parseImageSet(CSSParserValueList*);

    bool parseCalculation(CSSParserValue*, ValueRange);

    bool parseGeneratedImage(CSSParserValueList*, RefPtrWillBeRawPtr<CSSValue>&);

    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createPrimitiveNumericValue(CSSParserValue*);
    PassRefPtrWillBeRawPtr<CSSCustomIdentValue> createPrimitiveCustomIdentValue(CSSParserValue*);

    class ImplicitScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(ImplicitScope);
    public:
        ImplicitScope(CSSPropertyParser* parser)
            : m_parser(parser)
        {
            m_parser->m_implicitShorthand = true;
        }

        ~ImplicitScope()
        {
            m_parser->m_implicitShorthand = false;
        }

    private:
        CSSPropertyParser* m_parser;
    };

    class ShorthandScope {
        STACK_ALLOCATED();
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

    enum ReleaseParsedCalcValueCondition {
        ReleaseParsedCalcValue,
        DoNotReleaseParsedCalcValue
    };

    friend inline Units operator|(Units a, Units b)
    {
        return static_cast<Units>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    bool validCalculationUnit(CSSParserValue*, Units, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);

    bool shouldAcceptUnitLessValues(CSSParserValue*, Units, CSSParserMode);

    inline bool validUnit(CSSParserValue* value, Units unitflags, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue) { return validUnit(value, unitflags, m_context.mode(), releaseCalc); }
    bool validUnit(CSSParserValue*, Units, CSSParserMode, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);

    int colorIntFromValue(CSSParserValue*);

    bool parseDeprecatedGradientColorStop(CSSParserValue*, CSSGradientColorStop&);
    PassRefPtrWillBeRawPtr<CSSValue> parseDeprecatedGradientStopColor(const CSSParserValue*);

private:
    // Inputs:
    CSSParserValueList* m_valueList;
    CSSParserTokenRange m_range;
    const CSSParserContext& m_context;

    // Outputs:
    WillBeHeapVector<CSSProperty, 256>& m_parsedProperties;

    // Locals during parsing:
    int m_inParseShorthand;
    CSSPropertyID m_currentShorthand;
    bool m_implicitShorthand;
    RefPtrWillBeMember<CSSCalcValue> m_parsedCalculation;
};

CSSPropertyID unresolvedCSSPropertyID(const CSSParserString&);
CSSValueID cssValueKeywordID(const CSSParserString&);

} // namespace blink

#endif // CSSPropertyParser_h
