/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/resolver/StyleBuilderConverter.h"

#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/svg/SVGURIReference.h"

namespace blink {

namespace {

static GridLength convertGridTrackBreadth(const StyleResolverState& state, CSSPrimitiveValue* primitiveValue)
{
    if (primitiveValue->getValueID() == CSSValueMinContent)
        return Length(MinContent);

    if (primitiveValue->getValueID() == CSSValueMaxContent)
        return Length(MaxContent);

    // Fractional unit.
    if (primitiveValue->isFlex())
        return GridLength(primitiveValue->getDoubleValue());

    return primitiveValue->convertToLength<FixedConversion | PercentConversion | AutoConversion>(state.cssToLengthConversionData());
}

} // namespace

PassRefPtr<StyleReflection> StyleBuilderConverter::convertBoxReflect(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNone);
        return RenderStyle::initialBoxReflect();
    }

    CSSReflectValue* reflectValue = toCSSReflectValue(value);
    RefPtr<StyleReflection> reflection = StyleReflection::create();
    reflection->setDirection(*reflectValue->direction());
    if (reflectValue->offset())
        reflection->setOffset(reflectValue->offset()->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData()));
    NinePieceImage mask;
    mask.setMaskDefaults();
    state.styleMap().mapNinePieceImage(state.style(), CSSPropertyWebkitBoxReflect, reflectValue->mask(), mask);
    reflection->setMask(mask);

    return reflection.release();
}

Color StyleBuilderConverter::convertColor(StyleResolverState& state, CSSValue* value, bool forVisitedLink)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    return state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->color(), forVisitedLink);
}

AtomicString StyleBuilderConverter::convertFragmentIdentifier(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->isURI())
        return SVGURIReference::fragmentIdentifierFromIRIString(primitiveValue->getStringValue(), state.element()->treeScope());
    return nullAtom;
}

LengthBox StyleBuilderConverter::convertClip(StyleResolverState& state, CSSValue* value)
{
    Rect* rect = toCSSPrimitiveValue(value)->getRectValue();

    return LengthBox(convertLengthOrAuto(state, rect->top()),
        convertLengthOrAuto(state, rect->right()),
        convertLengthOrAuto(state, rect->bottom()),
        convertLengthOrAuto(state, rect->left()));
}

static FontDescription::GenericFamilyType convertGenericFamily(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueWebkitBody:
        return FontDescription::StandardFamily;
    case CSSValueSerif:
        return FontDescription::SerifFamily;
    case CSSValueSansSerif:
        return FontDescription::SansSerifFamily;
    case CSSValueCursive:
        return FontDescription::CursiveFamily;
    case CSSValueFantasy:
        return FontDescription::FantasyFamily;
    case CSSValueMonospace:
        return FontDescription::MonospaceFamily;
    case CSSValueWebkitPictograph:
        return FontDescription::PictographFamily;
    default:
        return FontDescription::NoFamily;
    }
}

static bool convertFontFamilyName(StyleResolverState& state, CSSPrimitiveValue* primitiveValue,
    FontDescription::GenericFamilyType& genericFamily, AtomicString& familyName)
{
    if (primitiveValue->isString()) {
        genericFamily = FontDescription::NoFamily;
        familyName = AtomicString(primitiveValue->getStringValue());
    } else if (state.document().settings()) {
        genericFamily = convertGenericFamily(primitiveValue->getValueID());
        familyName = state.fontBuilder().genericFontFamilyName(genericFamily);
    }

    return !familyName.isEmpty();
}

FontDescription::FamilyDescription StyleBuilderConverter::convertFontFamily(StyleResolverState& state, CSSValue* value)
{
    ASSERT(value->isValueList());

    FontDescription::FamilyDescription desc(FontDescription::NoFamily);
    FontFamily* currFamily = nullptr;

    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        if (!item->isPrimitiveValue())
            continue;

        FontDescription::GenericFamilyType genericFamily = FontDescription::NoFamily;
        AtomicString familyName;

        if (!convertFontFamilyName(state, toCSSPrimitiveValue(item), genericFamily, familyName))
            continue;

        if (!currFamily) {
            currFamily = &desc.family;
        } else {
            RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
            currFamily->appendFamily(newFamily);
            currFamily = newFamily.get();
        }

        currFamily->setFamily(familyName);

        if (genericFamily != FontDescription::NoFamily)
            desc.genericFamily = genericFamily;
    }

    return desc;
}

PassRefPtr<FontFeatureSettings> StyleBuilderConverter::convertFontFeatureSettings(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueNormal)
        return FontBuilder::initialFeatureSettings();

    CSSValueList* list = toCSSValueList(value);
    RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
    int len = list->length();
    for (int i = 0; i < len; ++i) {
        CSSFontFeatureValue* feature = toCSSFontFeatureValue(list->item(i));
        settings->append(FontFeature(feature->tag(), feature->value()));
    }
    return settings;
}

class RedirectSetHasViewportUnits {
public:
    RedirectSetHasViewportUnits(RenderStyle* from, RenderStyle* to)
        : m_from(from), m_to(to), m_hadViewportUnits(from->hasViewportUnits())
    {
        from->setHasViewportUnits(false);
    }
    ~RedirectSetHasViewportUnits()
    {
        m_to->setHasViewportUnits(m_from->hasViewportUnits());
        m_from->setHasViewportUnits(m_hadViewportUnits);
    }
private:
    RenderStyle* m_from;
    RenderStyle* m_to;
    bool m_hadViewportUnits;
};

static float computeFontSize(StyleResolverState& state, CSSPrimitiveValue* primitiveValue, const FontDescription::Size& parentSize)
{
    RedirectSetHasViewportUnits redirect(state.parentStyle(), state.style());

    CSSToLengthConversionData conversionData(state.parentStyle(), state.rootElementStyle(), state.document().renderView(), 1.0f, true);
    if (primitiveValue->isLength())
        return primitiveValue->computeLength<float>(conversionData);
    if (primitiveValue->isCalculatedPercentageWithLength())
        return primitiveValue->cssCalcValue()->toCalcValue(conversionData)->evaluate(parentSize.value);

    ASSERT_NOT_REACHED();
    return 0;
}

FontDescription::Size StyleBuilderConverter::convertFontSize(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    FontDescription::Size parentSize(0, 0.0f, false);

    // FIXME: Find out when parentStyle could be 0?
    if (state.parentStyle())
        parentSize = state.parentFontDescription().size();

    if (CSSValueID valueID = primitiveValue->getValueID()) {
        switch (valueID) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueWebkitXxxLarge:
            return FontDescription::Size(FontSize::keywordSize(valueID), 0.0f, false);
        case CSSValueLarger:
            return FontDescription::largerSize(parentSize);
        case CSSValueSmaller:
            return FontDescription::smallerSize(parentSize);
        default:
            ASSERT_NOT_REACHED();
            return FontBuilder::initialSize();
        }
    }

    bool parentIsAbsoluteSize = state.parentFontDescription().isAbsoluteSize();

    if (primitiveValue->isPercentage())
        return FontDescription::Size(0, (primitiveValue->getFloatValue() * parentSize.value / 100.0f), parentIsAbsoluteSize);

    return FontDescription::Size(0, computeFontSize(state, primitiveValue, parentSize), parentIsAbsoluteSize || !primitiveValue->isFontRelativeLength());
}

FontWeight StyleBuilderConverter::convertFontWeight(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue->getValueID()) {
    case CSSValueBolder:
        return FontDescription::bolderWeight(state.parentStyle()->fontDescription().weight());
    case CSSValueLighter:
        return FontDescription::lighterWeight(state.parentStyle()->fontDescription().weight());
    default:
        return *primitiveValue;
    }
}

FontDescription::VariantLigatures StyleBuilderConverter::convertFontVariantLigatures(StyleResolverState&, CSSValue* value)
{
    if (value->isValueList()) {
        FontDescription::VariantLigatures ligatures;
        CSSValueList* valueList = toCSSValueList(value);
        for (size_t i = 0; i < valueList->length(); ++i) {
            CSSValue* item = valueList->item(i);
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
            switch (primitiveValue->getValueID()) {
            case CSSValueNoCommonLigatures:
                ligatures.common = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueCommonLigatures:
                ligatures.common = FontDescription::EnabledLigaturesState;
                break;
            case CSSValueNoDiscretionaryLigatures:
                ligatures.discretionary = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueDiscretionaryLigatures:
                ligatures.discretionary = FontDescription::EnabledLigaturesState;
                break;
            case CSSValueNoHistoricalLigatures:
                ligatures.historical = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueHistoricalLigatures:
                ligatures.historical = FontDescription::EnabledLigaturesState;
                break;
            case CSSValueNoContextual:
                ligatures.contextual = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueContextual:
                ligatures.contextual = FontDescription::EnabledLigaturesState;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        return ligatures;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
    ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNormal);
    return FontDescription::VariantLigatures();
}

EGlyphOrientation StyleBuilderConverter::convertGlyphOrientation(StyleResolverState&, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return GO_0DEG;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_DEG)
        return GO_0DEG;

    float angle = fabsf(fmodf(primitiveValue->getFloatValue(), 360.0f));

    if (angle <= 45.0f || angle > 315.0f)
        return GO_0DEG;
    if (angle > 45.0f && angle <= 135.0f)
        return GO_90DEG;
    if (angle > 135.0f && angle <= 225.0f)
        return GO_180DEG;
    return GO_270DEG;
}

GridAutoFlow StyleBuilderConverter::convertGridAutoFlow(StyleResolverState&, CSSValue* value)
{
    CSSValueList* list = toCSSValueList(value);

    ASSERT(list->length() >= 1);
    CSSPrimitiveValue* first = toCSSPrimitiveValue(list->item(0));
    CSSPrimitiveValue* second = list->length() == 2 ? toCSSPrimitiveValue(list->item(1)) : nullptr;

    switch (first->getValueID()) {
    case CSSValueRow:
        if (second) {
            if (second->getValueID() == CSSValueDense)
                return AutoFlowRowDense;
            return AutoFlowStackRow;
        }
        return AutoFlowRow;
    case CSSValueColumn:
        if (second) {
            if (second->getValueID() == CSSValueDense)
                return AutoFlowColumnDense;
            return AutoFlowStackColumn;
        }
        return AutoFlowColumn;
    case CSSValueDense:
        if (second && second->getValueID() == CSSValueColumn)
            return AutoFlowColumnDense;
        return AutoFlowRowDense;
    case CSSValueStack:
        if (second && second->getValueID() == CSSValueColumn)
            return AutoFlowStackColumn;
        return AutoFlowStackRow;
    default:
        ASSERT_NOT_REACHED();
        return RenderStyle::initialGridAutoFlow();
    }
}

GridPosition StyleBuilderConverter::convertGridPosition(StyleResolverState&, CSSValue* value)
{
    // We accept the specification's grammar:
    // 'auto' | [ <integer> || <custom-ident> ] | [ span && [ <integer> || <custom-ident> ] ] | <custom-ident>

    GridPosition position;

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        // We translate <custom-ident> to <string> during parsing as it
        // makes handling it more simple.
        if (primitiveValue->isString()) {
            position.setNamedGridArea(primitiveValue->getStringValue());
            return position;
        }

        ASSERT(primitiveValue->getValueID() == CSSValueAuto);
        return position;
    }

    CSSValueList* values = toCSSValueList(value);
    ASSERT(values->length());

    bool isSpanPosition = false;
    // The specification makes the <integer> optional, in which case it default to '1'.
    int gridLineNumber = 1;
    String gridLineName;

    CSSValueListIterator it = values;
    CSSPrimitiveValue* currentValue = toCSSPrimitiveValue(it.value());
    if (currentValue->getValueID() == CSSValueSpan) {
        isSpanPosition = true;
        it.advance();
        currentValue = it.hasMore() ? toCSSPrimitiveValue(it.value()) : 0;
    }

    if (currentValue && currentValue->isNumber()) {
        gridLineNumber = currentValue->getIntValue();
        it.advance();
        currentValue = it.hasMore() ? toCSSPrimitiveValue(it.value()) : 0;
    }

    if (currentValue && currentValue->isString()) {
        gridLineName = currentValue->getStringValue();
        it.advance();
    }

    ASSERT(!it.hasMore());
    if (isSpanPosition)
        position.setSpanPosition(gridLineNumber, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return position;
}

GridTrackSize StyleBuilderConverter::convertGridTrackSize(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue())
        return GridTrackSize(convertGridTrackBreadth(state, toCSSPrimitiveValue(value)));

    CSSFunctionValue* minmaxFunction = toCSSFunctionValue(value);
    CSSValueList* arguments = minmaxFunction->arguments();
    ASSERT_WITH_SECURITY_IMPLICATION(arguments->length() == 2);
    GridLength minTrackBreadth(convertGridTrackBreadth(state, toCSSPrimitiveValue(arguments->item(0))));
    GridLength maxTrackBreadth(convertGridTrackBreadth(state, toCSSPrimitiveValue(arguments->item(1))));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

bool StyleBuilderConverter::convertGridTrackList(CSSValue* value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, OrderedNamedGridLines& orderedNamedGridLines, StyleResolverState& state)
{
    // Handle 'none'.
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        return primitiveValue->getValueID() == CSSValueNone;
    }

    if (!value->isValueList())
        return false;

    size_t currentNamedGridLine = 0;
    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (currValue->isGridLineNamesValue()) {
            CSSGridLineNamesValue* lineNamesValue = toCSSGridLineNamesValue(currValue);
            for (CSSValueListIterator j = lineNamesValue; j.hasMore(); j.advance()) {
                String namedGridLine = toCSSPrimitiveValue(j.value())->getStringValue();
                NamedGridLinesMap::AddResult result = namedGridLines.add(namedGridLine, Vector<size_t>());
                result.storedValue->value.append(currentNamedGridLine);
                OrderedNamedGridLines::AddResult orderedInsertionResult = orderedNamedGridLines.add(currentNamedGridLine, Vector<String>());
                orderedInsertionResult.storedValue->value.append(namedGridLine);
            }
            continue;
        }

        ++currentNamedGridLine;
        trackSizes.append(convertGridTrackSize(state, currValue));
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!trackSizes.isEmpty());
    return true;
}

void StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap& namedGridAreas, NamedGridLinesMap& namedGridLines, GridTrackSizingDirection direction)
{
    for (const auto& namedGridAreaEntry : namedGridAreas) {
        GridSpan areaSpan = direction == ForRows ? namedGridAreaEntry.value.rows : namedGridAreaEntry.value.columns;
        {
            NamedGridLinesMap::AddResult startResult = namedGridLines.add(namedGridAreaEntry.key + "-start", Vector<size_t>());
            startResult.storedValue->value.append(areaSpan.resolvedInitialPosition.toInt());
            std::sort(startResult.storedValue->value.begin(), startResult.storedValue->value.end());
        }
        {
            NamedGridLinesMap::AddResult endResult = namedGridLines.add(namedGridAreaEntry.key + "-end", Vector<size_t>());
            endResult.storedValue->value.append(areaSpan.resolvedFinalPosition.toInt() + 1);
            std::sort(endResult.storedValue->value.begin(), endResult.storedValue->value.end());
        }
    }
}

Length StyleBuilderConverter::convertLength(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length result = primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
    result.setQuirk(primitiveValue->isQuirkValue());
    return result;
}

Length StyleBuilderConverter::convertLengthOrAuto(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length result = primitiveValue->convertToLength<FixedConversion | PercentConversion | AutoConversion>(state.cssToLengthConversionData());
    result.setQuirk(primitiveValue->isQuirkValue());
    return result;
}

Length StyleBuilderConverter::convertLengthSizing(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue->getValueID()) {
    case CSSValueInvalid:
        return convertLength(state, value);
    case CSSValueIntrinsic:
        return Length(Intrinsic);
    case CSSValueMinIntrinsic:
        return Length(MinIntrinsic);
    case CSSValueWebkitMinContent:
        return Length(MinContent);
    case CSSValueWebkitMaxContent:
        return Length(MaxContent);
    case CSSValueWebkitFillAvailable:
        return Length(FillAvailable);
    case CSSValueWebkitFitContent:
        return Length(FitContent);
    case CSSValueAuto:
        return Length(Auto);
    default:
        ASSERT_NOT_REACHED();
        return Length();
    }
}

Length StyleBuilderConverter::convertLengthMaxSizing(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueNone)
        return Length(MaxSizeNone);
    return convertLengthSizing(state, value);
}

LengthPoint StyleBuilderConverter::convertLengthPoint(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* pair = primitiveValue->getPairValue();
    Length x = pair->first()->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
    Length y = pair->second()->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
    return LengthPoint(x, y);
}

LineBoxContain StyleBuilderConverter::convertLineBoxContain(StyleResolverState&, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNone);
        return LineBoxContainNone;
    }

    return toCSSLineBoxContainValue(value)->value();
}

float StyleBuilderConverter::convertNumberOrPercentage(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    ASSERT(primitiveValue->isNumber() || primitiveValue->isPercentage());
    if (primitiveValue->isNumber())
        return primitiveValue->getFloatValue();
    return primitiveValue->getFloatValue() / 100.0f;
}

static float convertPerspectiveLength(StyleResolverState& state, CSSPrimitiveValue* primitiveValue)
{
    return std::max(primitiveValue->computeLength<float>(state.cssToLengthConversionData()), 0.0f);
}

float StyleBuilderConverter::convertPerspective(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID() == CSSValueNone)
        return RenderStyle::initialPerspective();

    // CSSPropertyWebkitPerspective accepts unitless numbers.
    if (primitiveValue->isNumber()) {
        RefPtr<CSSPrimitiveValue> px = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX);
        return convertPerspectiveLength(state, px.get());
    }

    return convertPerspectiveLength(state, primitiveValue);
}

template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
static Length convertOriginLength(StyleResolverState& state, CSSPrimitiveValue* primitiveValue)
{
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case cssValueFor0:
            return Length(0, Percent);
        case cssValueFor100:
            return Length(100, Percent);
        case CSSValueCenter:
            return Length(50, Percent);
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return StyleBuilderConverter::convertLength(state, primitiveValue);
}

LengthPoint StyleBuilderConverter::convertPerspectiveOrigin(StyleResolverState& state, CSSValue* value)
{
    CSSValueList* list = toCSSValueList(value);
    ASSERT(list->length() == 2);

    CSSPrimitiveValue* primitiveValueX = toCSSPrimitiveValue(list->item(0));
    CSSPrimitiveValue* primitiveValueY = toCSSPrimitiveValue(list->item(1));

    return LengthPoint(
        convertOriginLength<CSSValueLeft, CSSValueRight>(state, primitiveValueX),
        convertOriginLength<CSSValueTop, CSSValueBottom>(state, primitiveValueY)
    );
}

EPaintOrder StyleBuilderConverter::convertPaintOrder(StyleResolverState&, CSSValue* cssPaintOrder)
{
    if (cssPaintOrder->isValueList()) {
        int paintOrder = 0;
        const CSSValueList& list = *toCSSValueList(cssPaintOrder);
        for (size_t i = 0; i < list.length(); ++i) {
            EPaintOrderType paintOrderType = PT_NONE;
            switch (toCSSPrimitiveValue(list.item(i))->getValueID()) {
            case CSSValueFill:
                paintOrderType = PT_FILL;
                break;
            case CSSValueStroke:
                paintOrderType = PT_STROKE;
                break;
            case CSSValueMarkers:
                paintOrderType = PT_MARKERS;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }

            paintOrder |= (paintOrderType << kPaintOrderBitwidth*i);
        }
        return (EPaintOrder)paintOrder;
    }

    return PO_NORMAL;
}

PassRefPtr<QuotesData> StyleBuilderConverter::convertQuotes(StyleResolverState&, CSSValue* value)
{
    if (value->isValueList()) {
        CSSValueList* list = toCSSValueList(value);
        RefPtr<QuotesData> quotes = QuotesData::create();
        for (size_t i = 0; i < list->length(); i += 2) {
            CSSValue* first = list->item(i);
            CSSValue* second = list->item(i + 1);
            String startQuote = toCSSPrimitiveValue(first)->getStringValue();
            String endQuote = toCSSPrimitiveValue(second)->getStringValue();
            quotes->addPair(std::make_pair(startQuote, endQuote));
        }
        return quotes.release();
    }
    // FIXME: We should assert we're a primitive value with valueID = CSSValueNone
    return QuotesData::create();
}

LengthSize StyleBuilderConverter::convertRadius(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* pair = primitiveValue->getPairValue();
    Length radiusWidth = pair->first()->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
    Length radiusHeight = pair->second()->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
    float width = radiusWidth.value();
    float height = radiusHeight.value();
    ASSERT(width >= 0 && height >= 0);
    if (width <= 0 || height <= 0)
        return LengthSize(Length(0, Fixed), Length(0, Fixed));
    return LengthSize(radiusWidth, radiusHeight);
}

PassRefPtr<ShadowList> StyleBuilderConverter::convertShadow(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNone);
        return PassRefPtr<ShadowList>();
    }

    const CSSValueList* valueList = toCSSValueList(value);
    size_t shadowCount = valueList->length();
    ShadowDataVector shadows;
    for (size_t i = 0; i < shadowCount; ++i) {
        const CSSShadowValue* item = toCSSShadowValue(valueList->item(i));
        float x = item->x->computeLength<float>(state.cssToLengthConversionData());
        float y = item->y->computeLength<float>(state.cssToLengthConversionData());
        float blur = item->blur ? item->blur->computeLength<float>(state.cssToLengthConversionData()) : 0;
        float spread = item->spread ? item->spread->computeLength<float>(state.cssToLengthConversionData()) : 0;
        ShadowStyle shadowStyle = item->style && item->style->getValueID() == CSSValueInset ? Inset : Normal;
        Color color;
        if (item->color)
            color = convertColor(state, item->color.get());
        else
            color = state.style()->color();
        shadows.append(ShadowData(FloatPoint(x, y), blur, spread, shadowStyle, color));
    }
    return ShadowList::adopt(shadows);
}

float StyleBuilderConverter::convertSpacing(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueNormal)
        return 0;
    return primitiveValue->computeLength<float>(state.cssToLengthConversionData());
}

PassRefPtr<SVGLengthList> StyleBuilderConverter::convertStrokeDasharray(StyleResolverState&, CSSValue* value)
{
    if (!value->isValueList()) {
        return SVGRenderStyle::initialStrokeDashArray();
    }

    CSSValueList* dashes = toCSSValueList(value);

    RefPtr<SVGLengthList> array = SVGLengthList::create();
    size_t length = dashes->length();
    for (size_t i = 0; i < length; ++i) {
        CSSValue* currValue = dashes->item(i);
        if (!currValue->isPrimitiveValue())
            continue;

        CSSPrimitiveValue* dash = toCSSPrimitiveValue(dashes->item(i));
        array->append(SVGLength::fromCSSPrimitiveValue(dash));
    }

    return array.release();
}

StyleColor StyleBuilderConverter::convertStyleColor(StyleResolverState& state, CSSValue* value, bool forVisitedLink)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueCurrentcolor)
        return StyleColor::currentColor();
    return state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, Color(), forVisitedLink);
}

Color StyleBuilderConverter::convertSVGColor(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->isRGBColor())
        return primitiveValue->getRGBA32Value();
    ASSERT(primitiveValue->getValueID() == CSSValueCurrentcolor);
    return state.style()->color();
}

PassRefPtr<SVGLength> StyleBuilderConverter::convertSVGLength(StyleResolverState&, CSSValue* value)
{
    return SVGLength::fromCSSPrimitiveValue(toCSSPrimitiveValue(value));
}

float StyleBuilderConverter::convertTextStrokeWidth(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID()) {
        float multiplier = convertLineWidth<float>(state, value);
        return CSSPrimitiveValue::create(multiplier / 48, CSSPrimitiveValue::CSS_EMS)->computeLength<float>(state.cssToLengthConversionData());
    }
    return primitiveValue->computeLength<float>(state.cssToLengthConversionData());
}

} // namespace blink
