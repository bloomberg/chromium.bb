/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2012 Apple Inc. All rights reserved.
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

#include "core/css/CSSPrimitiveValue.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSMarkup.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Node.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/FontMetrics.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringBuilder.h"

using namespace WTF;

namespace blink {

namespace {

// Max/min values for CSS, needs to slightly smaller/larger than the true max/min values to allow for rounding without overflowing.
// Subtract two (rather than one) to allow for values to be converted to float and back without exceeding the LayoutUnit::max.
const int maxValueForCssLength = INT_MAX / kFixedPointDenominator - 2;
const int minValueForCssLength = INT_MIN / kFixedPointDenominator + 2;

using StringToUnitTable = HashMap<String, CSSPrimitiveValue::UnitType>;

StringToUnitTable createStringToUnitTable()
{
    StringToUnitTable table;
    table.set(String("em"), CSSPrimitiveValue::UnitType::Ems);
    table.set(String("ex"), CSSPrimitiveValue::UnitType::Exs);
    table.set(String("px"), CSSPrimitiveValue::UnitType::Pixels);
    table.set(String("cm"), CSSPrimitiveValue::UnitType::Centimeters);
    table.set(String("mm"), CSSPrimitiveValue::UnitType::Millimeters);
    table.set(String("in"), CSSPrimitiveValue::UnitType::Inches);
    table.set(String("pt"), CSSPrimitiveValue::UnitType::Points);
    table.set(String("pc"), CSSPrimitiveValue::UnitType::Picas);
    table.set(String(""), CSSPrimitiveValue::UnitType::UserUnits);
    table.set(String("deg"), CSSPrimitiveValue::UnitType::Degrees);
    table.set(String("rad"), CSSPrimitiveValue::UnitType::Radians);
    table.set(String("grad"), CSSPrimitiveValue::UnitType::Gradians);
    table.set(String("ms"), CSSPrimitiveValue::UnitType::Milliseconds);
    table.set(String("s"), CSSPrimitiveValue::UnitType::Seconds);
    table.set(String("hz"), CSSPrimitiveValue::UnitType::Hertz);
    table.set(String("khz"), CSSPrimitiveValue::UnitType::Kilohertz);
    table.set(String("dpi"), CSSPrimitiveValue::UnitType::DotsPerInch);
    table.set(String("dpcm"), CSSPrimitiveValue::UnitType::DotsPerCentimeter);
    table.set(String("dppx"), CSSPrimitiveValue::UnitType::DotsPerPixel);
    table.set(String("vw"), CSSPrimitiveValue::UnitType::ViewportWidth);
    table.set(String("vh"), CSSPrimitiveValue::UnitType::ViewportHeight);
    table.set(String("vmin"), CSSPrimitiveValue::UnitType::ViewportMin);
    table.set(String("vmax"), CSSPrimitiveValue::UnitType::ViewportMax);
    table.set(String("rem"), CSSPrimitiveValue::UnitType::Rems);
    table.set(String("fr"), CSSPrimitiveValue::UnitType::Fraction);
    table.set(String("turn"), CSSPrimitiveValue::UnitType::Turns);
    table.set(String("ch"), CSSPrimitiveValue::UnitType::Chs);
    table.set(String("__qem"), CSSPrimitiveValue::UnitType::QuirkyEms);
    return table;
}

StringToUnitTable& unitTable()
{
    DEFINE_STATIC_LOCAL(StringToUnitTable, unitTable, (createStringToUnitTable()));
    return unitTable;
}

} // namespace

float CSSPrimitiveValue::clampToCSSLengthRange(double value)
{
    return clampTo<float>(value, minValueForCssLength, maxValueForCssLength);
}

void CSSPrimitiveValue::initUnitTable()
{
    // Make sure we initialize this during blink initialization
    // to avoid racy static local initialization.
    unitTable();
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::fromName(const String& unit)
{
    return unitTable().get(unit.lower());
}

CSSPrimitiveValue::UnitCategory CSSPrimitiveValue::unitCategory(UnitType type)
{
    switch (type) {
    case UnitType::Number:
        return CSSPrimitiveValue::UNumber;
    case UnitType::Percentage:
        return CSSPrimitiveValue::UPercent;
    case UnitType::Pixels:
    case UnitType::Centimeters:
    case UnitType::Millimeters:
    case UnitType::Inches:
    case UnitType::Points:
    case UnitType::Picas:
    case UnitType::UserUnits:
        return CSSPrimitiveValue::ULength;
    case UnitType::Milliseconds:
    case UnitType::Seconds:
        return CSSPrimitiveValue::UTime;
    case UnitType::Degrees:
    case UnitType::Radians:
    case UnitType::Gradians:
    case UnitType::Turns:
        return CSSPrimitiveValue::UAngle;
    case UnitType::Hertz:
    case UnitType::Kilohertz:
        return CSSPrimitiveValue::UFrequency;
    case UnitType::DotsPerPixel:
    case UnitType::DotsPerInch:
    case UnitType::DotsPerCentimeter:
        return CSSPrimitiveValue::UResolution;
    default:
        return CSSPrimitiveValue::UOther;
    }
}

bool CSSPrimitiveValue::colorIsDerivedFromElement() const
{
    int valueID = getValueID();
    switch (valueID) {
    case CSSValueWebkitText:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
        return true;
    default:
        return false;
    }
}

using CSSTextCache = WillBePersistentHeapHashMap<RawPtrWillBeWeakMember<const CSSPrimitiveValue>, String>;

#if ENABLE(OILPAN) && defined(LEAK_SANITIZER)

namespace {
// With LSan, wrap the persistent cache so that the registration of the
// (per-thread) static reference can be done.
class CSSTextCacheWrapper {
public:
    CSSTextCacheWrapper()
    {
        m_cache.registerAsStaticReference();
    }

    operator CSSTextCache&() { return m_cache; }

private:
    CSSTextCache m_cache;
};

}
#else
using CSSTextCacheWrapper = CSSTextCache;
#endif

static CSSTextCache& cssTextCache()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<CSSTextCacheWrapper>, cache, new ThreadSpecific<CSSTextCacheWrapper>);
    return *cache;
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::typeWithCalcResolved() const
{
    if (type() != UnitType::Calc)
        return type();

    switch (m_value.calc->category()) {
    case CalcAngle:
        return UnitType::Degrees;
    case CalcFrequency:
        return UnitType::Hertz;
    case CalcNumber:
        return UnitType::Number;
    case CalcPercent:
        return UnitType::Percentage;
    case CalcLength:
        return UnitType::Pixels;
    case CalcPercentNumber:
        return UnitType::CalcPercentageWithNumber;
    case CalcPercentLength:
        return UnitType::CalcPercentageWithLength;
    case CalcTime:
        return UnitType::Milliseconds;
    case CalcOther:
        return UnitType::Unknown;
    }
    return UnitType::Unknown;
}

static const AtomicString& valueName(CSSValueID valueID)
{
    ASSERT_ARG(valueID, valueID >= 0);
    ASSERT_ARG(valueID, valueID < numCSSValueKeywords);

    if (valueID < 0)
        return nullAtom;

    static AtomicString* keywordStrings = new AtomicString[numCSSValueKeywords]; // Leaked intentionally.
    AtomicString& keywordString = keywordStrings[valueID];
    if (keywordString.isNull())
        keywordString = getValueName(valueID);
    return keywordString;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSValueID valueID)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    m_value.valueID = valueID;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitType type)
    : CSSValue(PrimitiveClass)
{
    init(type);
    ASSERT(std::isfinite(num));
    m_value.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length, float zoom)
    : CSSValue(PrimitiveClass)
{
    switch (length.type()) {
    case Auto:
        init(UnitType::ValueID);
        m_value.valueID = CSSValueAuto;
        break;
    case MinContent:
        init(UnitType::ValueID);
        m_value.valueID = CSSValueMinContent;
        break;
    case MaxContent:
        init(UnitType::ValueID);
        m_value.valueID = CSSValueMaxContent;
        break;
    case FillAvailable:
        init(UnitType::ValueID);
        m_value.valueID = CSSValueWebkitFillAvailable;
        break;
    case FitContent:
        init(UnitType::ValueID);
        m_value.valueID = CSSValueFitContent;
        break;
    case ExtendToZoom:
        init(UnitType::ValueID);
        m_value.valueID = CSSValueInternalExtendToZoom;
        break;
    case Percent:
        init(UnitType::Percentage);
        ASSERT(std::isfinite(length.percent()));
        m_value.num = length.percent();
        break;
    case Fixed:
        init(UnitType::Pixels);
        m_value.num = length.value() / zoom;
        break;
    case Calculated: {
        const CalculationValue& calc = length.calculationValue();
        if (calc.pixels() && calc.percent()) {
            init(CSSCalcValue::create(
                CSSCalcValue::createExpressionNode(calc.pixels() / zoom, calc.percent()),
                calc.valueRange()));
            break;
        }
        if (calc.percent()) {
            init(UnitType::Percentage);
            m_value.num = calc.percent();
        } else {
            init(UnitType::Pixels);
            m_value.num = calc.pixels() / zoom;
        }
        if (m_value.num < 0 && calc.isNonNegative())
            m_value.num = 0;
        break;
    }
    case DeviceWidth:
    case DeviceHeight:
    case MaxSizeNone:
        ASSERT_NOT_REACHED();
        break;
    }
}

void CSSPrimitiveValue::init(UnitType type)
{
    m_primitiveUnitType = static_cast<unsigned>(type);
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<CSSCalcValue> c)
{
    init(UnitType::Calc);
    m_hasCachedCSSText = false;
    m_value.calc = c.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
#if !ENABLE(OILPAN)
    switch (type()) {
    case UnitType::Calc:
        // We must not call deref() when oilpan is enabled because m_value.calc is traced.
        m_value.calc->deref();
        break;
    case UnitType::CalcPercentageWithNumber:
    case UnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        break;
    case UnitType::Number:
    case UnitType::Integer:
    case UnitType::Percentage:
    case UnitType::Ems:
    case UnitType::QuirkyEms:
    case UnitType::Exs:
    case UnitType::Rems:
    case UnitType::Chs:
    case UnitType::Pixels:
    case UnitType::Centimeters:
    case UnitType::Millimeters:
    case UnitType::Inches:
    case UnitType::Points:
    case UnitType::Picas:
    case UnitType::UserUnits:
    case UnitType::Degrees:
    case UnitType::Radians:
    case UnitType::Gradians:
    case UnitType::Milliseconds:
    case UnitType::Seconds:
    case UnitType::Hertz:
    case UnitType::Kilohertz:
    case UnitType::Turns:
    case UnitType::ViewportWidth:
    case UnitType::ViewportHeight:
    case UnitType::ViewportMin:
    case UnitType::ViewportMax:
    case UnitType::DotsPerPixel:
    case UnitType::DotsPerInch:
    case UnitType::DotsPerCentimeter:
    case UnitType::Fraction:
    case UnitType::Unknown:
    case UnitType::ValueID:
        break;
    }
    if (m_hasCachedCSSText) {
        cssTextCache().remove(this);
        m_hasCachedCSSText = false;
    }
#endif
}

double CSSPrimitiveValue::computeSeconds() const
{
    ASSERT(isTime() || (isCalculated() && cssCalcValue()->category() == CalcTime));
    UnitType currentType = isCalculated() ? cssCalcValue()->expressionNode()->typeWithCalcResolved() : type();
    if (currentType == UnitType::Seconds)
        return getDoubleValue();
    if (currentType == UnitType::Milliseconds)
        return getDoubleValue() / 1000;
    ASSERT_NOT_REACHED();
    return 0;
}

double CSSPrimitiveValue::computeDegrees() const
{
    ASSERT(isAngle() || (isCalculated() && cssCalcValue()->category() == CalcAngle));
    UnitType currentType = isCalculated() ? cssCalcValue()->expressionNode()->typeWithCalcResolved() : type();
    switch (currentType) {
    case UnitType::Degrees:
        return getDoubleValue();
    case UnitType::Radians:
        return rad2deg(getDoubleValue());
    case UnitType::Gradians:
        return grad2deg(getDoubleValue());
    case UnitType::Turns:
        return turn2deg(getDoubleValue());
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

template<> int CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<int>(computeLengthDouble(conversionData));
}

template<> unsigned CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<unsigned>(computeLengthDouble(conversionData));
}

template<> Length CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return Length(clampToCSSLengthRange(computeLengthDouble(conversionData)), Fixed);
}

template<> short CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<short>(computeLengthDouble(conversionData));
}

template<> unsigned short CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<unsigned short>(computeLengthDouble(conversionData));
}

template<> float CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return static_cast<float>(computeLengthDouble(conversionData));
}

template<> double CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return computeLengthDouble(conversionData);
}

double CSSPrimitiveValue::computeLengthDouble(const CSSToLengthConversionData& conversionData) const
{
    if (type() == UnitType::Calc)
        return m_value.calc->computeLengthPx(conversionData);
    return conversionData.zoomedComputedPixels(getDoubleValue(), type());
}

void CSSPrimitiveValue::accumulateLengthArray(CSSLengthArray& lengthArray, CSSLengthTypeArray& lengthTypeArray, double multiplier) const
{
    ASSERT(lengthArray.size() == LengthUnitTypeCount);

    if (type() == UnitType::Calc) {
        cssCalcValue()->accumulateLengthArray(lengthArray, lengthTypeArray, multiplier);
        return;
    }

    LengthUnitType lengthType;
    bool conversionSuccess = unitTypeToLengthUnitType(type(), lengthType);
    ASSERT_UNUSED(conversionSuccess, conversionSuccess);
    lengthArray.at(lengthType) += m_value.num * conversionToCanonicalUnitsScaleFactor(type()) * multiplier;
    lengthTypeArray.set(lengthType);
}

void CSSPrimitiveValue::accumulateLengthArray(CSSLengthArray& lengthArray, double multiplier) const
{
    CSSLengthTypeArray lengthTypeArray;
    lengthTypeArray.resize(CSSPrimitiveValue::LengthUnitTypeCount);
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; ++i)
        lengthTypeArray.clear(i);
    return CSSPrimitiveValue::accumulateLengthArray(lengthArray, lengthTypeArray, multiplier);
}

double CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(UnitType unitType)
{
    double factor = 1.0;
    // FIXME: the switch can be replaced by an array of scale factors.
    switch (unitType) {
    // These are "canonical" units in their respective categories.
    case UnitType::Pixels:
    case UnitType::UserUnits:
    case UnitType::Degrees:
    case UnitType::Milliseconds:
    case UnitType::Hertz:
        break;
    case UnitType::Centimeters:
        factor = cssPixelsPerCentimeter;
        break;
    case UnitType::DotsPerCentimeter:
        factor = 1 / cssPixelsPerCentimeter;
        break;
    case UnitType::Millimeters:
        factor = cssPixelsPerMillimeter;
        break;
    case UnitType::Inches:
        factor = cssPixelsPerInch;
        break;
    case UnitType::DotsPerInch:
        factor = 1 / cssPixelsPerInch;
        break;
    case UnitType::Points:
        factor = cssPixelsPerPoint;
        break;
    case UnitType::Picas:
        factor = cssPixelsPerPica;
        break;
    case UnitType::Radians:
        factor = 180 / piDouble;
        break;
    case UnitType::Gradians:
        factor = 0.9;
        break;
    case UnitType::Turns:
        factor = 360;
        break;
    case UnitType::Seconds:
    case UnitType::Kilohertz:
        factor = 1000;
        break;
    default:
        break;
    }

    return factor;
}

Length CSSPrimitiveValue::convertToLength(const CSSToLengthConversionData& conversionData) const
{
    if (isLength())
        return computeLength<Length>(conversionData);
    if (isPercentage())
        return Length(getDoubleValue(), Percent);
    ASSERT(isCalculated());
    return Length(cssCalcValue()->toCalcValue(conversionData));
}

double CSSPrimitiveValue::getDoubleValue() const
{
    return type() != UnitType::Calc ? m_value.num : m_value.calc->doubleValue();
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::canonicalUnitTypeForCategory(UnitCategory category)
{
    // The canonical unit type is chosen according to the way CSSPropertyParser::validUnit() chooses the default unit
    // in each category (based on unitflags).
    switch (category) {
    case UNumber:
        return UnitType::Number;
    case ULength:
        return UnitType::Pixels;
    case UPercent:
        return UnitType::Unknown; // Cannot convert between numbers and percent.
    case UTime:
        return UnitType::Milliseconds;
    case UAngle:
        return UnitType::Degrees;
    case UFrequency:
        return UnitType::Hertz;
    case UResolution:
        return UnitType::DotsPerPixel;
    default:
        return UnitType::Unknown;
    }
}

bool CSSPrimitiveValue::unitTypeToLengthUnitType(UnitType unitType, LengthUnitType& lengthType)
{
    switch (unitType) {
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::Centimeters:
    case CSSPrimitiveValue::UnitType::Millimeters:
    case CSSPrimitiveValue::UnitType::Inches:
    case CSSPrimitiveValue::UnitType::Points:
    case CSSPrimitiveValue::UnitType::Picas:
    case CSSPrimitiveValue::UnitType::UserUnits:
        lengthType = UnitTypePixels;
        return true;
    case CSSPrimitiveValue::UnitType::Ems:
    case CSSPrimitiveValue::UnitType::QuirkyEms:
        lengthType = UnitTypeFontSize;
        return true;
    case CSSPrimitiveValue::UnitType::Exs:
        lengthType = UnitTypeFontXSize;
        return true;
    case CSSPrimitiveValue::UnitType::Rems:
        lengthType = UnitTypeRootFontSize;
        return true;
    case CSSPrimitiveValue::UnitType::Chs:
        lengthType = UnitTypeZeroCharacterWidth;
        return true;
    case CSSPrimitiveValue::UnitType::Percentage:
        lengthType = UnitTypePercentage;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportWidth:
        lengthType = UnitTypeViewportWidth;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportHeight:
        lengthType = UnitTypeViewportHeight;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportMin:
        lengthType = UnitTypeViewportMin;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportMax:
        lengthType = UnitTypeViewportMax;
        return true;
    default:
        return false;
    }
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::lengthUnitTypeToUnitType(LengthUnitType type)
{
    switch (type) {
    case UnitTypePixels:
        return CSSPrimitiveValue::UnitType::Pixels;
    case UnitTypeFontSize:
        return CSSPrimitiveValue::UnitType::Ems;
    case UnitTypeFontXSize:
        return CSSPrimitiveValue::UnitType::Exs;
    case UnitTypeRootFontSize:
        return CSSPrimitiveValue::UnitType::Rems;
    case UnitTypeZeroCharacterWidth:
        return CSSPrimitiveValue::UnitType::Chs;
    case UnitTypePercentage:
        return CSSPrimitiveValue::UnitType::Percentage;
    case UnitTypeViewportWidth:
        return CSSPrimitiveValue::UnitType::ViewportWidth;
    case UnitTypeViewportHeight:
        return CSSPrimitiveValue::UnitType::ViewportHeight;
    case UnitTypeViewportMin:
        return CSSPrimitiveValue::UnitType::ViewportMin;
    case UnitTypeViewportMax:
        return CSSPrimitiveValue::UnitType::ViewportMax;
    case LengthUnitTypeCount:
        break;
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::UnitType::Unknown;
}

static String formatNumber(double number, const char* suffix, unsigned suffixLength)
{
#if OS(WIN) && _MSC_VER < 1900
    unsigned oldFormat = _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
    String result = String::format("%.6g", number);
#if OS(WIN) && _MSC_VER < 1900
    _set_output_format(oldFormat);
#endif
    result.append(suffix, suffixLength);
    return result;
}

template <unsigned characterCount>
ALWAYS_INLINE static String formatNumber(double number, const char (&characters)[characterCount])
{
    return formatNumber(number, characters, characterCount - 1);
}

static String formatNumber(double number, const char* characters)
{
    return formatNumber(number, characters, strlen(characters));
}

const char* CSSPrimitiveValue::unitTypeToString(UnitType type)
{
    switch (type) {
    case UnitType::Number:
    case UnitType::Integer:
    case UnitType::UserUnits:
        return "";
    case UnitType::Percentage:
        return "%";
    case UnitType::Ems:
    case UnitType::QuirkyEms:
        return "em";
    case UnitType::Exs:
        return "ex";
    case UnitType::Rems:
        return "rem";
    case UnitType::Chs:
        return "ch";
    case UnitType::Pixels:
        return "px";
    case UnitType::Centimeters:
        return "cm";
    case UnitType::DotsPerPixel:
        return "dppx";
    case UnitType::DotsPerInch:
        return "dpi";
    case UnitType::DotsPerCentimeter:
        return "dpcm";
    case UnitType::Millimeters:
        return "mm";
    case UnitType::Inches:
        return "in";
    case UnitType::Points:
        return "pt";
    case UnitType::Picas:
        return "pc";
    case UnitType::Degrees:
        return "deg";
    case UnitType::Radians:
        return "rad";
    case UnitType::Gradians:
        return "grad";
    case UnitType::Milliseconds:
        return "ms";
    case UnitType::Seconds:
        return "s";
    case UnitType::Hertz:
        return "hz";
    case UnitType::Kilohertz:
        return "khz";
    case UnitType::Turns:
        return "turn";
    case UnitType::Fraction:
        return "fr";
    case UnitType::ViewportWidth:
        return "vw";
    case UnitType::ViewportHeight:
        return "vh";
    case UnitType::ViewportMin:
        return "vmin";
    case UnitType::ViewportMax:
        return "vmax";
    case UnitType::Unknown:
    case UnitType::ValueID:
    case UnitType::Calc:
    case UnitType::CalcPercentageWithNumber:
    case UnitType::CalcPercentageWithLength:
        break;
    };
    ASSERT_NOT_REACHED();
    return "";
}

String CSSPrimitiveValue::customCSSText() const
{
    if (m_hasCachedCSSText) {
        ASSERT(cssTextCache().contains(this));
        return cssTextCache().get(this);
    }

    String text;
    switch (type()) {
    case UnitType::Unknown:
        // FIXME
        break;
    case UnitType::Integer:
        text = String::format("%d", getIntValue());
        break;
    case UnitType::Number:
    case UnitType::Percentage:
    case UnitType::Ems:
    case UnitType::QuirkyEms:
    case UnitType::Exs:
    case UnitType::Rems:
    case UnitType::Chs:
    case UnitType::Pixels:
    case UnitType::Centimeters:
    case UnitType::DotsPerPixel:
    case UnitType::DotsPerInch:
    case UnitType::DotsPerCentimeter:
    case UnitType::Millimeters:
    case UnitType::Inches:
    case UnitType::Points:
    case UnitType::Picas:
    case UnitType::UserUnits:
    case UnitType::Degrees:
    case UnitType::Radians:
    case UnitType::Gradians:
    case UnitType::Milliseconds:
    case UnitType::Seconds:
    case UnitType::Hertz:
    case UnitType::Kilohertz:
    case UnitType::Turns:
    case UnitType::Fraction:
    case UnitType::ViewportWidth:
    case UnitType::ViewportHeight:
    case UnitType::ViewportMin:
    case UnitType::ViewportMax:
        text = formatNumber(m_value.num, unitTypeToString(type()));
        break;
    case UnitType::ValueID:
        text = valueName(m_value.valueID);
        break;
    case UnitType::Calc:
        text = m_value.calc->customCSSText();
        break;
    case UnitType::CalcPercentageWithNumber:
    case UnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT(!cssTextCache().contains(this));
    cssTextCache().set(this, text);
    m_hasCachedCSSText = true;
    return text;
}

bool CSSPrimitiveValue::equals(const CSSPrimitiveValue& other) const
{
    if (type() != other.type())
        return false;

    switch (type()) {
    case UnitType::Unknown:
        return false;
    case UnitType::Number:
    case UnitType::Integer:
    case UnitType::Percentage:
    case UnitType::Ems:
    case UnitType::Exs:
    case UnitType::Rems:
    case UnitType::Pixels:
    case UnitType::Centimeters:
    case UnitType::DotsPerPixel:
    case UnitType::DotsPerInch:
    case UnitType::DotsPerCentimeter:
    case UnitType::Millimeters:
    case UnitType::Inches:
    case UnitType::Points:
    case UnitType::Picas:
    case UnitType::UserUnits:
    case UnitType::Degrees:
    case UnitType::Radians:
    case UnitType::Gradians:
    case UnitType::Milliseconds:
    case UnitType::Seconds:
    case UnitType::Hertz:
    case UnitType::Kilohertz:
    case UnitType::Turns:
    case UnitType::ViewportWidth:
    case UnitType::ViewportHeight:
    case UnitType::ViewportMin:
    case UnitType::ViewportMax:
    case UnitType::Fraction:
        return m_value.num == other.m_value.num;
    case UnitType::ValueID:
        return m_value.valueID == other.m_value.valueID;
    case UnitType::Calc:
        return m_value.calc && other.m_value.calc && m_value.calc->equals(*other.m_value.calc);
    case UnitType::Chs:
    case UnitType::CalcPercentageWithNumber:
    case UnitType::CalcPercentageWithLength:
    case UnitType::QuirkyEms:
        return false;
    }
    return false;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPrimitiveValue)
{
#if ENABLE(OILPAN)
    switch (type()) {
    case UnitType::Calc:
        visitor->trace(m_value.calc);
        break;
    default:
        break;
    }
#endif
    CSSValue::traceAfterDispatch(visitor);
}

} // namespace blink
