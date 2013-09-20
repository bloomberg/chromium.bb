/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
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
#include "core/css/resolver/StyleBuilderCustom.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "StyleBuilderFunctions.h"
#include "StylePropertyShorthand.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridTemplateValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/Counter.h"
#include "core/css/FontValue.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/ShadowValue.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/page/Frame.h"
#include "core/page/Settings.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/rendering/style/CounterContent.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/SVGRenderStyle.h"
#include "core/rendering/style/SVGRenderStyleDefs.h"
#include "core/rendering/style/ShadowData.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/svg/SVGColor.h"
#include "core/svg/SVGPaint.h"
#include "core/svg/SVGURIReference.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace WebCore {

static Length clipConvertToLength(StyleResolverState& state, CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedIntegerConversion | PercentConversion | FractionConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyClip(StyleResolverState& state)
{
    state.style()->setClip(Length(), Length(), Length(), Length());
    state.style()->setHasClip(false);
}

void StyleBuilderFunctions::applyInheritCSSPropertyClip(StyleResolverState& state)
{
    RenderStyle* parentStyle = state.parentStyle();
    if (!parentStyle->hasClip())
        return applyInitialCSSPropertyClip(state);
    state.style()->setClip(parentStyle->clipTop(), parentStyle->clipRight(), parentStyle->clipBottom(), parentStyle->clipLeft());
    state.style()->setHasClip(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyClip(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (Rect* rect = primitiveValue->getRectValue()) {
        Length top = clipConvertToLength(state, rect->top());
        Length right = clipConvertToLength(state, rect->right());
        Length bottom = clipConvertToLength(state, rect->bottom());
        Length left = clipConvertToLength(state, rect->left());
        state.style()->setClip(top, right, bottom, left);
        state.style()->setHasClip(true);
    } else if (primitiveValue->getValueID() == CSSValueAuto) {
        state.style()->setClip(Length(), Length(), Length(), Length());
        state.style()->setHasClip(false);
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyCursor(StyleResolverState& state)
{
    state.style()->clearCursorList();
    state.style()->setCursor(RenderStyle::initialCursor());
}

void StyleBuilderFunctions::applyInheritCSSPropertyCursor(StyleResolverState& state)
{
    state.style()->setCursor(state.parentStyle()->cursor());
    state.style()->setCursorList(state.parentStyle()->cursors());
}

void StyleBuilderFunctions::applyValueCSSPropertyCursor(StyleResolverState& state, CSSValue* value)
{
    state.style()->clearCursorList();
    if (value->isValueList()) {
        CSSValueList* list = toCSSValueList(value);
        int len = list->length();
        state.style()->setCursor(CURSOR_AUTO);
        for (int i = 0; i < len; i++) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (item->isCursorImageValue()) {
                CSSCursorImageValue* image = static_cast<CSSCursorImageValue*>(item);
                if (image->updateIfSVGCursorIsUsed(state.element())) // Elements with SVG cursors are not allowed to share style.
                    state.style()->setUnique();
                state.style()->addCursor(state.styleImage(CSSPropertyCursor, image), image->hotSpot());
            } else if (item->isPrimitiveValue()) {
                CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
                if (primitiveValue->isValueID())
                    state.style()->setCursor(*primitiveValue);
            }
        }
    } else if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->isValueID() && state.style()->cursor() != ECursor(*primitiveValue))
            state.style()->setCursor(*primitiveValue);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyDirection(StyleResolverState& state, CSSValue* value)
{
    state.style()->setDirection(*toCSSPrimitiveValue(value));
    Element* element = state.element();
    if (element && element == element->document().documentElement())
        element->document().setDirectionSetOnDocumentElement(true);
}

static inline bool isValidDisplayValue(StyleResolverState& state, EDisplay displayPropertyValue)
{
    if (state.element() && state.element()->isSVGElement() && state.style()->styleType() == NOPSEUDO)
        return (displayPropertyValue == INLINE || displayPropertyValue == BLOCK || displayPropertyValue == NONE);
    return true;
}

void StyleBuilderFunctions::applyInheritCSSPropertyDisplay(StyleResolverState& state)
{
    EDisplay display = state.parentStyle()->display();
    if (!isValidDisplayValue(state, display))
        return;
    state.style()->setDisplay(display);
}

void StyleBuilderFunctions::applyValueCSSPropertyDisplay(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    EDisplay display = *toCSSPrimitiveValue(value);

    if (!isValidDisplayValue(state, display))
        return;

    state.style()->setDisplay(display);
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontFamily(StyleResolverState& state)
{
    state.fontBuilder().setFontFamilyInitial(state.style()->effectiveZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontFamily(StyleResolverState& state)
{
    state.fontBuilder().setFontFamilyInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontFamily(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontFamilyValue(value, state.style()->effectiveZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontSize(StyleResolverState& state)
{
    state.fontBuilder().setFontSizeInitial(state.style()->effectiveZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontSize(StyleResolverState& state)
{
    state.fontBuilder().setFontSizeInherit(state.parentFontDescription(), state.style()->effectiveZoom());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontSize(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontSizeValue(value, state.parentStyle(), state.rootElementStyle(), state.style()->effectiveZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontWeight(StyleResolverState& state)
{
    state.fontBuilder().setWeight(FontWeightNormal);
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontWeight(StyleResolverState& state)
{
    state.fontBuilder().setWeight(state.parentFontDescription().weight());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontWeight(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue->getValueID()) {
    case CSSValueInvalid:
        ASSERT_NOT_REACHED();
        break;
    case CSSValueBolder:
        state.fontBuilder().setWeightBolder();
        break;
    case CSSValueLighter:
        state.fontBuilder().setWeightLighter();
        break;
    default:
        state.fontBuilder().setWeight(*primitiveValue);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyLineHeight(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length lineHeight;

    if (primitiveValue->getValueID() == CSSValueNormal) {
        lineHeight = RenderStyle::initialLineHeight();
    } else if (primitiveValue->isLength()) {
        double multiplier = state.style()->effectiveZoom();
        if (Frame* frame = state.document().frame())
            multiplier *= frame->textZoomFactor();
        lineHeight = primitiveValue->computeLength<Length>(state.style(), state.rootElementStyle(), multiplier);
    } else if (primitiveValue->isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        lineHeight = Length((state.style()->fontSize() * primitiveValue->getIntValue()) / 100, Fixed);
    } else if (primitiveValue->isNumber()) {
        // FIXME: number and percentage values should produce the same type of Length (ie. Fixed or Percent).
        lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
    } else if (primitiveValue->isViewportPercentageLength()) {
        lineHeight = primitiveValue->viewportPercentageLength();
    } else {
        return;
    }
    state.style()->setLineHeight(lineHeight);
}

void StyleBuilderFunctions::applyValueCSSPropertyListStyleImage(StyleResolverState& state, CSSValue* value)
{
    state.style()->setListStyleImage(state.styleImage(CSSPropertyListStyleImage, value));
}

void StyleBuilderFunctions::applyInitialCSSPropertyOutlineStyle(StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(RenderStyle::initialOutlineStyleIsAuto());
    state.style()->setOutlineStyle(RenderStyle::initialBorderStyle());
}

void StyleBuilderFunctions::applyInheritCSSPropertyOutlineStyle(StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(state.parentStyle()->outlineStyleIsAuto());
    state.style()->setOutlineStyle(state.parentStyle()->outlineStyle());
}

void StyleBuilderFunctions::applyValueCSSPropertyOutlineStyle(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    state.style()->setOutlineStyleIsAuto(*primitiveValue);
    state.style()->setOutlineStyle(*primitiveValue);
}

void StyleBuilderFunctions::applyValueCSSPropertyResize(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    EResize r = RESIZE_NONE;
    switch (primitiveValue->getValueID()) {
    case 0:
        return;
    case CSSValueAuto:
        if (Settings* settings = state.document().settings())
            r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
        break;
    default:
        r = *primitiveValue;
    }
    state.style()->setResize(r);
}

static Length mmLength(double mm) { return CSSPrimitiveValue::create(mm, CSSPrimitiveValue::CSS_MM)->computeLength<Length>(0, 0); }
static Length inchLength(double inch) { return CSSPrimitiveValue::create(inch, CSSPrimitiveValue::CSS_IN)->computeLength<Length>(0, 0); }
static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height)
{
    DEFINE_STATIC_LOCAL(Length, a5Width, (mmLength(148)));
    DEFINE_STATIC_LOCAL(Length, a5Height, (mmLength(210)));
    DEFINE_STATIC_LOCAL(Length, a4Width, (mmLength(210)));
    DEFINE_STATIC_LOCAL(Length, a4Height, (mmLength(297)));
    DEFINE_STATIC_LOCAL(Length, a3Width, (mmLength(297)));
    DEFINE_STATIC_LOCAL(Length, a3Height, (mmLength(420)));
    DEFINE_STATIC_LOCAL(Length, b5Width, (mmLength(176)));
    DEFINE_STATIC_LOCAL(Length, b5Height, (mmLength(250)));
    DEFINE_STATIC_LOCAL(Length, b4Width, (mmLength(250)));
    DEFINE_STATIC_LOCAL(Length, b4Height, (mmLength(353)));
    DEFINE_STATIC_LOCAL(Length, letterWidth, (inchLength(8.5)));
    DEFINE_STATIC_LOCAL(Length, letterHeight, (inchLength(11)));
    DEFINE_STATIC_LOCAL(Length, legalWidth, (inchLength(8.5)));
    DEFINE_STATIC_LOCAL(Length, legalHeight, (inchLength(14)));
    DEFINE_STATIC_LOCAL(Length, ledgerWidth, (inchLength(11)));
    DEFINE_STATIC_LOCAL(Length, ledgerHeight, (inchLength(17)));

    if (!pageSizeName)
        return false;

    switch (pageSizeName->getValueID()) {
    case CSSValueA5:
        width = a5Width;
        height = a5Height;
        break;
    case CSSValueA4:
        width = a4Width;
        height = a4Height;
        break;
    case CSSValueA3:
        width = a3Width;
        height = a3Height;
        break;
    case CSSValueB5:
        width = b5Width;
        height = b5Height;
        break;
    case CSSValueB4:
        width = b4Width;
        height = b4Height;
        break;
    case CSSValueLetter:
        width = letterWidth;
        height = letterHeight;
        break;
    case CSSValueLegal:
        width = legalWidth;
        height = legalHeight;
        break;
    case CSSValueLedger:
        width = ledgerWidth;
        height = ledgerHeight;
        break;
    default:
        return false;
    }

    if (pageOrientation) {
        switch (pageOrientation->getValueID()) {
        case CSSValueLandscape:
            std::swap(width, height);
            break;
        case CSSValuePortrait:
            // Nothing to do.
            break;
        default:
            return false;
        }
    }
    return true;
}

void StyleBuilderFunctions::applyInitialCSSPropertySize(StyleResolverState&) { }
void StyleBuilderFunctions::applyInheritCSSPropertySize(StyleResolverState&) { }
void StyleBuilderFunctions::applyValueCSSPropertySize(StyleResolverState& state, CSSValue* value)
{
    state.style()->resetPageSizeType();
    Length width;
    Length height;
    PageSizeType pageSizeType = PAGE_SIZE_AUTO;
    CSSValueListInspector inspector(value);
    switch (inspector.length()) {
    case 2: {
        // <length>{2} | <page-size> <orientation>
        if (!inspector.first()->isPrimitiveValue() || !inspector.second()->isPrimitiveValue())
            return;
        CSSPrimitiveValue* first = toCSSPrimitiveValue(inspector.first());
        CSSPrimitiveValue* second = toCSSPrimitiveValue(inspector.second());
        if (first->isLength()) {
            // <length>{2}
            if (!second->isLength())
                return;
            width = first->computeLength<Length>(state.style(), state.rootElementStyle());
            height = second->computeLength<Length>(state.style(), state.rootElementStyle());
        } else {
            // <page-size> <orientation>
            // The value order is guaranteed. See CSSParser::parseSizeParameter.
            if (!getPageSizeFromName(first, second, width, height))
                return;
        }
        pageSizeType = PAGE_SIZE_RESOLVED;
        break;
    }
    case 1: {
        // <length> | auto | <page-size> | [ portrait | landscape]
        if (!inspector.first()->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(inspector.first());
        if (primitiveValue->isLength()) {
            // <length>
            pageSizeType = PAGE_SIZE_RESOLVED;
            width = height = primitiveValue->computeLength<Length>(state.style(), state.rootElementStyle());
        } else {
            switch (primitiveValue->getValueID()) {
            case 0:
                return;
            case CSSValueAuto:
                pageSizeType = PAGE_SIZE_AUTO;
                break;
            case CSSValuePortrait:
                pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                break;
            case CSSValueLandscape:
                pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                break;
            default:
                // <page-size>
                pageSizeType = PAGE_SIZE_RESOLVED;
                if (!getPageSizeFromName(primitiveValue, 0, width, height))
                    return;
            }
        }
        break;
    }
    default:
        return;
    }
    state.style()->setPageSizeType(pageSizeType);
    state.style()->setPageSize(LengthSize(width, height));
}

void StyleBuilderFunctions::applyValueCSSPropertyTextAlign(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    // FIXME : Per http://www.w3.org/TR/css3-text/#text-align0 can now take <string> but this is not implemented in the
    // rendering code.
    if (primitiveValue->isString())
        return;

    if (primitiveValue->isValueID() && primitiveValue->getValueID() != CSSValueWebkitMatchParent)
        state.style()->setTextAlign(*primitiveValue);
    else if (state.parentStyle()->textAlign() == TASTART)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? LEFT : RIGHT);
    else if (state.parentStyle()->textAlign() == TAEND)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? RIGHT : LEFT);
    else
        state.style()->setTextAlign(state.parentStyle()->textAlign());
}

void StyleBuilderFunctions::applyValueCSSPropertyTextDecoration(StyleResolverState& state, CSSValue* value)
{
    TextDecoration t = RenderStyle::initialTextDecoration();
    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        t |= *toCSSPrimitiveValue(item);
    }
    state.style()->setTextDecoration(t);
}

void StyleBuilderFunctions::applyInheritCSSPropertyTextIndent(StyleResolverState& state)
{
    state.style()->setTextIndent(state.parentStyle()->textIndent());
    state.style()->setTextIndentLine(state.parentStyle()->textIndentLine());
}

void StyleBuilderFunctions::applyInitialCSSPropertyTextIndent(StyleResolverState& state)
{
    state.style()->setTextIndent(RenderStyle::initialTextIndent());
    state.style()->setTextIndentLine(RenderStyle::initialTextIndentLine());
}

void StyleBuilderFunctions::applyValueCSSPropertyTextIndent(StyleResolverState& state, CSSValue* value)
{
    if (!value->isValueList())
        return;

    // [ <length> | <percentage> ] each-line
    // The order is guaranteed. See CSSParser::parseTextIndent.
    // The second value, each-line is handled only when css3TextEnabled() returns true.

    CSSValueList* valueList = toCSSValueList(value);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(valueList->itemWithoutBoundsCheck(0));
    Length lengthOrPercentageValue = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    ASSERT(!lengthOrPercentageValue.isUndefined());
    state.style()->setTextIndent(lengthOrPercentageValue);

    ASSERT(valueList->length() <= 2);
    CSSPrimitiveValue* eachLineValue = toCSSPrimitiveValue(valueList->item(1));
    if (eachLineValue) {
        ASSERT(eachLineValue->getValueID() == CSSValueEachLine);
        state.style()->setTextIndentLine(TextIndentEachLine);
    } else
        state.style()->setTextIndentLine(TextIndentFirstLine);
}

void StyleBuilderFunctions::applyValueCSSPropertyVerticalAlign(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID())
        return state.style()->setVerticalAlign(*primitiveValue);

    state.style()->setVerticalAlignLength(primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom()));
}

static void resetEffectiveZoom(StyleResolverState& state)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    state.setEffectiveZoom(state.parentStyle() ? state.parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyZoom(StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyZoom(StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(state.parentStyle()->zoom());
}

void StyleBuilderFunctions::applyValueCSSPropertyZoom(StyleResolverState& state, CSSValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID() == CSSValueNormal) {
        resetEffectiveZoom(state);
        state.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->getValueID() == CSSValueReset) {
        state.setEffectiveZoom(RenderStyle::initialZoom());
        state.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->getValueID() == CSSValueDocument) {
        float docZoom = state.rootElementStyle() ? state.rootElementStyle()->zoom() : RenderStyle::initialZoom();
        state.setEffectiveZoom(docZoom);
        state.setZoom(docZoom);
    } else if (primitiveValue->isPercentage()) {
        resetEffectiveZoom(state);
        if (float percent = primitiveValue->getFloatValue())
            state.setZoom(percent / 100.0f);
    } else if (primitiveValue->isNumber()) {
        resetEffectiveZoom(state);
        if (float number = primitiveValue->getFloatValue())
            state.setZoom(number);
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitAspectRatio(StyleResolverState& state)
{
    state.style()->setHasAspectRatio(RenderStyle::initialHasAspectRatio());
    state.style()->setAspectRatioDenominator(RenderStyle::initialAspectRatioDenominator());
    state.style()->setAspectRatioNumerator(RenderStyle::initialAspectRatioNumerator());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitAspectRatio(StyleResolverState& state)
{
    if (!state.parentStyle()->hasAspectRatio())
        return;
    state.style()->setHasAspectRatio(true);
    state.style()->setAspectRatioDenominator(state.parentStyle()->aspectRatioDenominator());
    state.style()->setAspectRatioNumerator(state.parentStyle()->aspectRatioNumerator());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitAspectRatio(StyleResolverState& state, CSSValue* value)
{
    if (!value->isAspectRatioValue()) {
        state.style()->setHasAspectRatio(false);
        return;
    }
    CSSAspectRatioValue* aspectRatioValue = static_cast<CSSAspectRatioValue*>(value);
    state.style()->setHasAspectRatio(true);
    state.style()->setAspectRatioDenominator(aspectRatioValue->denominatorValue());
    state.style()->setAspectRatioNumerator(aspectRatioValue->numeratorValue());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitClipPath(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setClipPath(0);
        } else if (primitiveValue->isShape()) {
            state.style()->setClipPath(ShapeClipPathOperation::create(basicShapeForValue(state, primitiveValue->getShapeValue())));
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue->getStringValue();
            KURL url = state.document().completeURL(cssURLValue);
            // FIXME: It doesn't work with forward or external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=90405)
            state.style()->setClipPath(ReferenceClipPathOperation::create(cssURLValue, url.fragmentIdentifier()));
        }
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitFontVariantLigatures(StyleResolverState& state)
{
    state.fontBuilder().setFontVariantLigaturesInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitFontVariantLigatures(StyleResolverState& state)
{
    state.fontBuilder().setFontVariantLigaturesInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitFontVariantLigatures(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontVariantLigaturesValue(value);
}

void StyleBuilderFunctions::applyValueCSSPropertyInternalMarqueeIncrement(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueSmall:
            state.style()->setMarqueeIncrement(Length(1, Fixed)); // 1px.
            break;
        case CSSValueNormal:
            state.style()->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
            break;
        case CSSValueLarge:
            state.style()->setMarqueeIncrement(Length(36, Fixed)); // 36px.
            break;
        default:
            break;
        }
    } else {
        Length marqueeLength = primitiveValue ? primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | FractionConversion>(state.style(), state.rootElementStyle()) : Length(Undefined);
        if (!marqueeLength.isUndefined())
            state.style()->setMarqueeIncrement(marqueeLength);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyInternalMarqueeSpeed(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (CSSValueID valueID = primitiveValue->getValueID()) {
        switch (valueID) {
        case CSSValueSlow:
            state.style()->setMarqueeSpeed(500); // 500 msec.
            break;
        case CSSValueNormal:
            state.style()->setMarqueeSpeed(85); // 85msec. The WinIE default.
            break;
        case CSSValueFast:
            state.style()->setMarqueeSpeed(10); // 10msec. Super fast.
            break;
        default:
            break;
        }
    } else if (primitiveValue->isTime()) {
        state.style()->setMarqueeSpeed(primitiveValue->computeTime<int, CSSPrimitiveValue::Milliseconds>());
    } else if (primitiveValue->isNumber()) { // For scrollamount support.
        state.style()->setMarqueeSpeed(primitiveValue->getIntValue());
    }
}

// FIXME: We should use the same system for this as the rest of the pseudo-shorthands (e.g. background-position)
void StyleBuilderFunctions::applyInitialCSSPropertyWebkitPerspectiveOrigin(StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitPerspectiveOriginX(state);
    applyInitialCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitPerspectiveOrigin(StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitPerspectiveOriginX(state);
    applyInheritCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitPerspectiveOrigin(StyleResolverState&, CSSValue* value)
{
    // This is expanded in the parser
    ASSERT_NOT_REACHED();
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
    state.style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
    state.style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(state.parentStyle()->textEmphasisFill());
    state.style()->setTextEmphasisMark(state.parentStyle()->textEmphasisMark());
    state.style()->setTextEmphasisCustomMark(state.parentStyle()->textEmphasisCustomMark());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state, CSSValue* value)
{
    if (value->isValueList()) {
        CSSValueList* list = toCSSValueList(value);
        ASSERT(list->length() == 2);
        if (list->length() != 2)
            return;
        for (unsigned i = 0; i < 2; ++i) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (!item->isPrimitiveValue())
                continue;

            CSSPrimitiveValue* value = toCSSPrimitiveValue(item);
            if (value->getValueID() == CSSValueFilled || value->getValueID() == CSSValueOpen)
                state.style()->setTextEmphasisFill(*value);
            else
                state.style()->setTextEmphasisMark(*value);
        }
        state.style()->setTextEmphasisCustomMark(nullAtom);
        return;
    }

    if (!value->isPrimitiveValue())
        return;
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->isString()) {
        state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        state.style()->setTextEmphasisMark(TextEmphasisMarkCustom);
        state.style()->setTextEmphasisCustomMark(primitiveValue->getStringValue());
        return;
    }

    state.style()->setTextEmphasisCustomMark(nullAtom);

    if (primitiveValue->getValueID() == CSSValueFilled || primitiveValue->getValueID() == CSSValueOpen) {
        state.style()->setTextEmphasisFill(*primitiveValue);
        state.style()->setTextEmphasisMark(TextEmphasisMarkAuto);
    } else {
        state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        state.style()->setTextEmphasisMark(*primitiveValue);
    }
}

#if ENABLE(CSS3_TEXT)
void StyleBuilderFunctions::applyValueCSSPropetyWebkitTextUnderlinePosition(StyleResolverState& state, CSSValue* value)
{
    // This is true if value is 'auto' or 'alphabetic'.
    if (value->isPrimitiveValue()) {
        TextUnderlinePosition t = *toCSSPrimitiveValue(value);
        state.style()->setTextUnderlinePosition(t);
        return;
    }

    unsigned t = 0;
    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        TextUnderlinePosition t2 = *toCSSPrimitiveValue(item);
        t |= t2;
    }
    state.style()->setTextUnderlinePosition(static_cast<TextUnderlinePosition>(t));
}
#endif // CSS3_TEXT

Length StyleBuilderConverter::convertLength(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length result = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    ASSERT(!result.isUndefined());
    result.setQuirk(primitiveValue->isQuirkValue());
    return result;
}

Length StyleBuilderConverter::convertLengthOrAuto(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length result = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    ASSERT(!result.isUndefined());
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
        return Length(Undefined);
    return convertLengthSizing(state, value);
}

LengthSize StyleBuilderConverter::convertRadius(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* pair = primitiveValue->getPairValue();
    Length radiusWidth = pair->first()->convertToLength<FixedIntegerConversion | PercentConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    Length radiusHeight = pair->second()->convertToLength<FixedIntegerConversion | PercentConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    float width = radiusWidth.value();
    float height = radiusHeight.value();
    ASSERT(width >= 0 && height >= 0);
    if (width <= 0 || height <= 0)
        return LengthSize(Length(0, Fixed), Length(0, Fixed));
    return LengthSize(radiusWidth, radiusHeight);
}

float StyleBuilderConverter::convertSpacing(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueNormal)
        return 0;
    float zoom = state.useSVGZoomRules() ? 1.0f : state.style()->effectiveZoom();
    return primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoom);
}


// Everything below this line is from the old StyleResolver::applyProperty
// and eventually needs to move into new StyleBuilderFunctions calls intead.

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) { \
    state.style()->set##Prop(state.parentStyle()->prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    state.style()->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_SVG_INHERIT(prop, Prop) \
if (isInherit) { \
    state.style()->accessSVGStyle()->set##Prop(state.parentStyle()->svgStyle()->prop()); \
    return; \
}

#define HANDLE_SVG_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_SVG_INHERIT(prop, Prop) \
if (isInitial) { \
    state.style()->accessSVGStyle()->set##Prop(SVGRenderStyle::initial##Prop()); \
    return; \
}

static bool createGridTrackBreadth(CSSPrimitiveValue* primitiveValue, const StyleResolverState& state, GridLength& workingLength)
{
    if (primitiveValue->getValueID() == CSSValueMinContent) {
        workingLength = Length(MinContent);
        return true;
    }

    if (primitiveValue->getValueID() == CSSValueMaxContent) {
        workingLength = Length(MaxContent);
        return true;
    }

    if (primitiveValue->isFlex()) {
        // Fractional unit.
        workingLength.setFlex(primitiveValue->getDoubleValue());
        return true;
    }

    workingLength = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    if (workingLength.length().isUndefined())
        return false;

    if (primitiveValue->isLength())
        workingLength.length().setQuirk(primitiveValue->isQuirkValue());

    return true;
}

static bool createGridTrackSize(CSSValue* value, GridTrackSize& trackSize, const StyleResolverState& state)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        GridLength workingLength;
        if (!createGridTrackBreadth(primitiveValue, state, workingLength))
            return false;

        trackSize.setLength(workingLength);
        return true;
    }

    CSSFunctionValue* minmaxFunction = toCSSFunctionValue(value);
    CSSValueList* arguments = minmaxFunction->arguments();
    ASSERT_WITH_SECURITY_IMPLICATION(arguments->length() == 2);
    GridLength minTrackBreadth;
    GridLength maxTrackBreadth;
    if (!createGridTrackBreadth(toCSSPrimitiveValue(arguments->itemWithoutBoundsCheck(0)), state, minTrackBreadth) || !createGridTrackBreadth(toCSSPrimitiveValue(arguments->itemWithoutBoundsCheck(1)), state, maxTrackBreadth))
        return false;

    trackSize.setMinMax(minTrackBreadth, maxTrackBreadth);
    return true;
}

static bool createGridTrackList(CSSValue* value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, OrderedNamedGridLines& orderedNamedGridLines, const StyleResolverState& state)
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
        if (currValue->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(currValue);
            if (primitiveValue->isString()) {
                String namedGridLine = primitiveValue->getStringValue();
                NamedGridLinesMap::AddResult result = namedGridLines.add(namedGridLine, Vector<size_t>());
                result.iterator->value.append(currentNamedGridLine);
                OrderedNamedGridLines::AddResult orderedInsertionResult = orderedNamedGridLines.add(currentNamedGridLine, Vector<String>());
                orderedInsertionResult.iterator->value.append(namedGridLine);
                continue;
            }
        }

        ++currentNamedGridLine;
        GridTrackSize trackSize;
        if (!createGridTrackSize(currValue, trackSize, state))
            return false;

        trackSizes.append(trackSize);
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!trackSizes.isEmpty());
    return true;
}

static bool createGridPosition(CSSValue* value, GridPosition& position)
{
    // We accept the specification's grammar:
    // 'auto' | [ <integer> || <string> ] | [ span && [ <integer> || string ] ] | <ident>

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        // We translate <ident> to <string> during parsing as it
        // makes handling it more simple.
        if (primitiveValue->isString()) {
            position.setNamedGridArea(primitiveValue->getStringValue());
            return true;
        }

        ASSERT(primitiveValue->getValueID() == CSSValueAuto);
        return true;
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

    return true;
}

static bool degreeToGlyphOrientation(CSSPrimitiveValue* primitiveValue, EGlyphOrientation& orientation)
{
    if (!primitiveValue)
        return false;

    if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_DEG)
        return false;

    float angle = fabsf(fmodf(primitiveValue->getFloatValue(), 360.0f));

    if (angle <= 45.0f || angle > 315.0f) {
        orientation = GO_0DEG;
        return true;
    }
    if (angle > 45.0f && angle <= 135.0f) {
        orientation = GO_90DEG;
        return true;
    }
    if (angle > 135.0f && angle <= 225.0f) {
        orientation = GO_180DEG;
        return true;
    }
    orientation = GO_270DEG;
    return true;
}

static Color colorFromSVGColorCSSValue(SVGColor* svgColor, const Color& fgColor)
{
    Color color;
    if (svgColor->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
        color = fgColor;
    else
        color = svgColor->color();
    return color;
}

static EPaintOrder paintOrderFlattened(CSSValue* cssPaintOrder)
{
    if (cssPaintOrder->isValueList()) {
        int paintOrder = 0;
        CSSValueListInspector iter(cssPaintOrder);
        for (size_t i = 0; i < iter.length(); i++) {
            CSSPrimitiveValue* value = toCSSPrimitiveValue(iter.item(i));

            EPaintOrderType paintOrderType = PT_NONE;
            switch (value->getValueID()) {
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

static bool numberToFloat(const CSSPrimitiveValue* primitiveValue, float& out)
{
    if (!primitiveValue)
        return false;
    if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
        return false;
    out = primitiveValue->getFloatValue();
    return true;
}

static bool percentageOrNumberToFloat(const CSSPrimitiveValue* primitiveValue, float& out)
{
    if (!primitiveValue)
        return false;
    int type = primitiveValue->primitiveType();
    if (type == CSSPrimitiveValue::CSS_PERCENTAGE) {
        out = primitiveValue->getFloatValue() / 100.0f;
        return true;
    }
    return numberToFloat(primitiveValue, out);
}

static String fragmentIdentifier(const CSSPrimitiveValue* primitiveValue, Document& document)
{
    if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_URI)
        return String();
    return SVGURIReference::fragmentIdentifierFromIRIString(primitiveValue->getStringValue(), document);
}

static inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyOutlineColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    default:
        break;
    }

    return false;
}

static bool hasVariableReference(CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        return primitiveValue->hasVariableReference();
    }

    if (value->isCalculationValue())
        return static_cast<CSSCalcValue*>(value)->hasVariableReference();

    if (value->isReflectValue()) {
        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        CSSPrimitiveValue* direction = reflectValue->direction();
        CSSPrimitiveValue* offset = reflectValue->offset();
        CSSValue* mask = reflectValue->mask();
        return (direction && hasVariableReference(direction)) || (offset && hasVariableReference(offset)) || (mask && hasVariableReference(mask));
    }

    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        if (hasVariableReference(i.value()))
            return true;
    }

    return false;
}

// FIXME: Resolving variables should be factored better. Maybe a resover-style class?
static void resolveVariables(StyleResolverState& state, CSSPropertyID id, CSSValue* value, Vector<std::pair<CSSPropertyID, String> >& knownExpressions)
{
    std::pair<CSSPropertyID, String> expression(id, value->serializeResolvingVariables(*state.style()->variables()));

    if (knownExpressions.contains(expression))
        return; // cycle detected.

    knownExpressions.append(expression);

    // FIXME: It would be faster not to re-parse from strings, but for now CSS property validation lives inside the parser so we do it there.
    RefPtr<MutableStylePropertySet> resultSet = MutableStylePropertySet::create();
    if (!CSSParser::parseValue(resultSet.get(), id, expression.second, false, state.document()))
        return; // expression failed to parse.

    for (unsigned i = 0; i < resultSet->propertyCount(); i++) {
        StylePropertySet::PropertyReference property = resultSet->propertyAt(i);
        if (property.id() != CSSPropertyVariable && hasVariableReference(property.value())) {
            resolveVariables(state, property.id(), property.value(), knownExpressions);
        } else {
            StyleBuilder::applyProperty(property.id(), state, property.value());
            // All properties become dependent on their parent style when they use variables.
            state.style()->setHasExplicitlyInheritedProperties();
        }
    }
}

void StyleBuilder::applyProperty(CSSPropertyID id, StyleResolverState& state, CSSValue* value)
{
    if (RuntimeEnabledFeatures::cssVariablesEnabled() && id != CSSPropertyVariable && hasVariableReference(value)) {
        Vector<std::pair<CSSPropertyID, String> > knownExpressions;
        resolveVariables(state, id, value, knownExpressions);
        return;
    }

    // CSS variables don't resolve shorthands at parsing time, so this should be *after* handling variables.
    ASSERT_WITH_MESSAGE(!isExpandedShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    bool isInherit = state.parentNode() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentNode() && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    ASSERT(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;
    if (primitiveValue && primitiveValue->getValueID() == CSSValueCurrentcolor)
        state.style()->setHasCurrentColor();

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();

    if (id == CSSPropertyVariable) {
        ASSERT_WITH_SECURITY_IMPLICATION(value->isVariableValue());
        CSSVariableValue* variable = toCSSVariableValue(value);
        ASSERT(!variable->name().isEmpty());
        ASSERT(!variable->value().isEmpty());
        state.style()->setVariable(variable->name(), variable->value());
        return;
    }

    if (StyleBuilder::applyProperty(id, state, value, isInitial, isInherit))
        return;

    // Fall back to the old switch statement, which is now in StyleBuilderCustom.cpp
    StyleBuilder::oldApplyProperty(id, state, value, isInitial, isInherit);
}


void StyleBuilder::oldApplyProperty(CSSPropertyID id, StyleResolverState& state, CSSValue* value, bool isInitial, bool isInherit)
{
    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;

    float zoomFactor = state.style()->effectiveZoom();

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.
    switch (id) {
    case CSSPropertyContent:
        // list of string, uri, counter, attr, i
        {
            // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not. This
            // note is a reminder that eventually "inherit" needs to be supported.

            if (isInitial) {
                state.style()->clearContent();
                return;
            }

            if (!value->isValueList())
                return;

            bool didSet = false;
            for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
                CSSValue* item = i.value();
                if (item->isImageGeneratorValue()) {
                    if (item->isGradientValue())
                        state.style()->setContent(StyleGeneratedImage::create(static_cast<CSSGradientValue*>(item)->gradientWithStylesResolved(state.document().textLinkColors(), state.style()->color()).get()), didSet);
                    else
                        state.style()->setContent(StyleGeneratedImage::create(static_cast<CSSImageGeneratorValue*>(item)), didSet);
                    didSet = true;
                } else if (item->isImageSetValue()) {
                    state.style()->setContent(state.elementStyleResources().setOrPendingFromValue(CSSPropertyContent, static_cast<CSSImageSetValue*>(item)), didSet);
                    didSet = true;
                }

                if (item->isImageValue()) {
                    state.style()->setContent(state.elementStyleResources().cachedOrPendingFromValue(CSSPropertyContent, toCSSImageValue(item)), didSet);
                    didSet = true;
                    continue;
                }

                if (!item->isPrimitiveValue())
                    continue;

                CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);

                if (contentValue->isString()) {
                    state.style()->setContent(contentValue->getStringValue().impl(), didSet);
                    didSet = true;
                } else if (contentValue->isAttr()) {
                    // FIXME: Can a namespace be specified for an attr(foo)?
                    if (state.style()->styleType() == NOPSEUDO)
                        state.style()->setUnique();
                    else
                        state.parentStyle()->setUnique();
                    QualifiedName attr(nullAtom, contentValue->getStringValue().impl(), nullAtom);
                    const AtomicString& value = state.element()->getAttribute(attr);
                    state.style()->setContent(value.isNull() ? emptyAtom : value.impl(), didSet);
                    didSet = true;
                    // register the fact that the attribute value affects the style
                    state.contentAttrValues().append(attr.localName());
                } else if (contentValue->isCounter()) {
                    Counter* counterValue = contentValue->getCounterValue();
                    EListStyleType listStyleType = NoneListStyle;
                    CSSValueID listStyleIdent = counterValue->listStyleIdent();
                    if (listStyleIdent != CSSValueNone)
                        listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
                    OwnPtr<CounterContent> counter = adoptPtr(new CounterContent(counterValue->identifier(), listStyleType, counterValue->separator()));
                    state.style()->setContent(counter.release(), didSet);
                    didSet = true;
                } else {
                    switch (contentValue->getValueID()) {
                    case CSSValueOpenQuote:
                        state.style()->setContent(OPEN_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueCloseQuote:
                        state.style()->setContent(CLOSE_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueNoOpenQuote:
                        state.style()->setContent(NO_OPEN_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueNoCloseQuote:
                        state.style()->setContent(NO_CLOSE_QUOTE, didSet);
                        didSet = true;
                        break;
                    default:
                        // normal and none do not have any effect.
                        { }
                    }
                }
            }
            if (!didSet)
                state.style()->clearContent();
            return;
        }
    case CSSPropertyQuotes:
        HANDLE_INHERIT_AND_INITIAL(quotes, Quotes);
        if (value->isValueList()) {
            CSSValueList* list = toCSSValueList(value);
            RefPtr<QuotesData> quotes = QuotesData::create();
            for (size_t i = 0; i < list->length(); i += 2) {
                CSSValue* first = list->itemWithoutBoundsCheck(i);
                // item() returns null if out of bounds so this is safe.
                CSSValue* second = list->item(i + 1);
                if (!second)
                    continue;
                String startQuote = toCSSPrimitiveValue(first)->getStringValue();
                String endQuote = toCSSPrimitiveValue(second)->getStringValue();
                quotes->addPair(std::make_pair(startQuote, endQuote));
            }
            state.style()->setQuotes(quotes);
            return;
        }
        if (primitiveValue) {
            if (primitiveValue->getValueID() == CSSValueNone)
                state.style()->setQuotes(QuotesData::create());
        }
        return;
    // Shorthand properties.
    case CSSPropertyFont:
        // Only System Font identifiers should come through this method
        // all other values should have been handled when the shorthand
        // was expanded by the parser.
        // FIXME: System Font identifiers should not hijack this
        // short-hand CSSProperty like this.
        ASSERT(!isInitial);
        ASSERT(!isInherit);
        ASSERT(primitiveValue);
        state.style()->setLineHeight(RenderStyle::initialLineHeight());
        state.setLineHeightValue(0);
        state.fontBuilder().fromSystemFont(primitiveValue->getValueID(), state.style()->effectiveZoom());
        return;
    case CSSPropertyAnimation:
    case CSSPropertyBackground:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBorder:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRadius:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderSpacing:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderWidth:
    case CSSPropertyListStyle:
    case CSSPropertyMargin:
    case CSSPropertyOutline:
    case CSSPropertyOverflow:
    case CSSPropertyPadding:
    case CSSPropertyTransition:
    case CSSPropertyWebkitAnimation:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitColumns:
    case CSSPropertyWebkitColumnRule:
    case CSSPropertyFlex:
    case CSSPropertyFlexFlow:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyGridArea:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTransition:
    case CSSPropertyWebkitTransformOrigin:
        ASSERT(isExpandedShorthand(id));
        ASSERT_NOT_REACHED();
        break;

    // CSS3 Properties
    case CSSPropertyTextShadow:
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow: {
        if (isInherit) {
            if (id == CSSPropertyTextShadow)
                return state.style()->setTextShadow(cloneShadow(state.parentStyle()->textShadow()));
            return state.style()->setBoxShadow(cloneShadow(state.parentStyle()->boxShadow()));
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSSPropertyTextShadow ? state.style()->setTextShadow(nullptr) : state.style()->setBoxShadow(nullptr);

        if (!value->isValueList())
            return;

        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* currValue = i.value();
            if (!currValue->isShadowValue())
                continue;
            ShadowValue* item = static_cast<ShadowValue*>(currValue);
            int x = item->x->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor);
            int y = item->y->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor);
            int blur = item->blur ? item->blur->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor) : 0;
            int spread = item->spread ? item->spread->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor) : 0;
            ShadowStyle shadowStyle = item->style && item->style->getValueID() == CSSValueInset ? Inset : Normal;
            Color color;
            if (item->color)
                color = state.document().textLinkColors().colorFromPrimitiveValue(item->color.get(), state.style()->visitedDependentColor(CSSPropertyColor));
            else if (state.style())
                color = state.style()->color();

            if (!color.isValid())
                color = Color::transparent;
            OwnPtr<ShadowData> shadow = ShadowData::create(IntPoint(x, y), blur, spread, shadowStyle, color);
            if (id == CSSPropertyTextShadow)
                state.style()->setTextShadow(shadow.release(), i.index()); // add to the list if this is not the first entry
            else
                state.style()->setBoxShadow(shadow.release(), i.index()); // add to the list if this is not the first entry
        }
        return;
    }
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            state.style()->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }

        if (!value->isReflectValue())
            return;

        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(*reflectValue->direction());
        if (reflectValue->offset())
            reflection->setOffset(reflectValue->offset()->convertToLength<FixedIntegerConversion | PercentConversion>(state.style(), state.rootElementStyle(), zoomFactor));
        NinePieceImage mask;
        mask.setMaskDefaults();
        state.styleMap().mapNinePieceImage(state.style(), id, reflectValue->mask(), mask);
        reflection->setMask(mask);

        state.style()->setBoxReflect(reflection.release());
        return;
    }
    case CSSPropertySrc: // Only used in @font-face rules.
        return;
    case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        return;
    case CSSPropertyWebkitLocale: {
        HANDLE_INHERIT_AND_INITIAL(locale, Locale);
        if (!primitiveValue)
            return;
        if (primitiveValue->getValueID() == CSSValueAuto)
            state.style()->setLocale(nullAtom);
        else
            state.style()->setLocale(primitiveValue->getStringValue());
        state.fontBuilder().setScript(state.style()->locale());
        return;
    }
    case CSSPropertyWebkitAppRegion: {
        if (!primitiveValue || !primitiveValue->getValueID())
            return;
        state.style()->setDraggableRegionMode(primitiveValue->getValueID() == CSSValueDrag ? DraggableRegionDrag : DraggableRegionNoDrag);
        state.document().setHasAnnotatedRegions(true);
        return;
    }
    case CSSPropertyWebkitTextStrokeWidth: {
        HANDLE_INHERIT_AND_INITIAL(textStrokeWidth, TextStrokeWidth)
        float width = 0;
        switch (primitiveValue->getValueID()) {
        case CSSValueThin:
        case CSSValueMedium:
        case CSSValueThick: {
            double result = 1.0 / 48;
            if (primitiveValue->getValueID() == CSSValueMedium)
                result *= 3;
            else if (primitiveValue->getValueID() == CSSValueThick)
                result *= 5;
            width = CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS)->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
            break;
        }
        default:
            width = primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
            break;
        }
        state.style()->setTextStrokeWidth(width);
        return;
    }
    case CSSPropertyWebkitTransform: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        TransformBuilder::createTransformOperations(value, state.style(), state.rootElementStyle(), operations);
        state.style()->setTransform(operations);
        return;
    }
    case CSSPropertyWebkitPerspective: {
        HANDLE_INHERIT_AND_INITIAL(perspective, Perspective)

        if (!primitiveValue)
            return;

        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setPerspective(0);
            return;
        }

        float perspectiveValue;
        if (primitiveValue->isLength()) {
            perspectiveValue = primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
        } else if (primitiveValue->isNumber()) {
            // For backward compatibility, treat valueless numbers as px.
            perspectiveValue = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX)->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
        } else {
            return;
        }

        if (perspectiveValue >= 0.0f)
            state.style()->setPerspective(perspectiveValue);
        return;
    }
    case CSSPropertyWebkitTapHighlightColor: {
        HANDLE_INHERIT_AND_INITIAL(tapHighlightColor, TapHighlightColor);
        if (!primitiveValue)
            break;

        Color col = state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->visitedDependentColor(CSSPropertyColor));
        state.style()->setTapHighlightColor(col);
        return;
    }
    case CSSPropertyInvalid:
        return;
    // Directional properties are resolved by resolveDirectionAwareProperty() before the switch.
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
    {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, state.style()->direction(), state.style()->writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, state, value);
    }
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyWebkitTextDecorationsInEffect:
        return;

    // CSS Text Layout Module Level 3: Vertical writing support
    case CSSPropertyWebkitWritingMode: {
        HANDLE_INHERIT_AND_INITIAL(writingMode, WritingMode);

        if (primitiveValue)
            state.setWritingMode(*primitiveValue);

        // FIXME: It is not ok to modify document state while applying style.
        if (state.element() && state.element() == state.document().documentElement())
            state.document().setWritingModeSetOnDocumentElement(true);
        return;
    }

    case CSSPropertyWebkitTextOrientation: {
        HANDLE_INHERIT_AND_INITIAL(textOrientation, TextOrientation);

        if (primitiveValue)
            state.setTextOrientation(*primitiveValue);

        return;
    }

    case CSSPropertyWebkitLineBoxContain: {
        HANDLE_INHERIT_AND_INITIAL(lineBoxContain, LineBoxContain)
        if (primitiveValue && primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setLineBoxContain(LineBoxContainNone);
            return;
        }

        if (!value->isCSSLineBoxContainValue())
            return;

        CSSLineBoxContainValue* lineBoxContainValue = static_cast<CSSLineBoxContainValue*>(value);
        state.style()->setLineBoxContain(lineBoxContainValue->value());
        return;
    }

    // CSS Fonts Module Level 3
    case CSSPropertyWebkitFontFeatureSettings: {
        if (primitiveValue && primitiveValue->getValueID() == CSSValueNormal) {
            state.fontBuilder().setFeatureSettingsNormal();
            return;
        }

        if (!value->isValueList())
            return;

        state.fontBuilder().setFeatureSettingsValue(value);
        return;
    }

    case CSSPropertyWebkitFilter: {
        HANDLE_INHERIT_AND_INITIAL(filter, Filter);
        FilterOperations operations;
        if (FilterOperationResolver::createFilterOperations(value, state.style(), state.rootElementStyle(), operations, state))
            state.style()->setFilter(operations);
        return;
    }
    case CSSPropertyGridAutoColumns: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoColumns, GridAutoColumns);
        GridTrackSize trackSize;
        if (!createGridTrackSize(value, trackSize, state))
            return;
        state.style()->setGridAutoColumns(trackSize);
        return;
    }
    case CSSPropertyGridAutoRows: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoRows, GridAutoRows);
        GridTrackSize trackSize;
        if (!createGridTrackSize(value, trackSize, state))
            return;
        state.style()->setGridAutoRows(trackSize);
        return;
    }
    case CSSPropertyGridDefinitionColumns: {
        if (isInherit) {
            state.style()->setGridDefinitionColumns(state.parentStyle()->gridDefinitionColumns());
            state.style()->setNamedGridColumnLines(state.parentStyle()->namedGridColumnLines());
            state.style()->setOrderedNamedGridColumnLines(state.parentStyle()->orderedNamedGridColumnLines());
            return;
        }
        if (isInitial) {
            state.style()->setGridDefinitionColumns(RenderStyle::initialGridDefinitionColumns());
            state.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
            state.style()->setOrderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        OrderedNamedGridLines orderedNamedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, orderedNamedGridLines, state))
            return;
        state.style()->setGridDefinitionColumns(trackSizes);
        state.style()->setNamedGridColumnLines(namedGridLines);
        state.style()->setOrderedNamedGridColumnLines(orderedNamedGridLines);
        return;
    }
    case CSSPropertyGridDefinitionRows: {
        if (isInherit) {
            state.style()->setGridDefinitionRows(state.parentStyle()->gridDefinitionRows());
            state.style()->setNamedGridRowLines(state.parentStyle()->namedGridRowLines());
            state.style()->setOrderedNamedGridRowLines(state.parentStyle()->orderedNamedGridRowLines());
            return;
        }
        if (isInitial) {
            state.style()->setGridDefinitionRows(RenderStyle::initialGridDefinitionRows());
            state.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
            state.style()->setOrderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        OrderedNamedGridLines orderedNamedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, orderedNamedGridLines, state))
            return;
        state.style()->setGridDefinitionRows(trackSizes);
        state.style()->setNamedGridRowLines(namedGridLines);
        state.style()->setOrderedNamedGridRowLines(orderedNamedGridLines);
        return;
    }

    case CSSPropertyGridColumnStart: {
        HANDLE_INHERIT_AND_INITIAL(gridColumnStart, GridColumnStart);
        GridPosition startPosition;
        if (!createGridPosition(value, startPosition))
            return;
        state.style()->setGridColumnStart(startPosition);
        return;
    }
    case CSSPropertyGridColumnEnd: {
        HANDLE_INHERIT_AND_INITIAL(gridColumnEnd, GridColumnEnd);
        GridPosition endPosition;
        if (!createGridPosition(value, endPosition))
            return;
        state.style()->setGridColumnEnd(endPosition);
        return;
    }

    case CSSPropertyGridRowStart: {
        HANDLE_INHERIT_AND_INITIAL(gridRowStart, GridRowStart);
        GridPosition beforePosition;
        if (!createGridPosition(value, beforePosition))
            return;
        state.style()->setGridRowStart(beforePosition);
        return;
    }
    case CSSPropertyGridRowEnd: {
        HANDLE_INHERIT_AND_INITIAL(gridRowEnd, GridRowEnd);
        GridPosition afterPosition;
        if (!createGridPosition(value, afterPosition))
            return;
        state.style()->setGridRowEnd(afterPosition);
        return;
    }

    case CSSPropertyGridTemplate: {
        if (isInherit) {
            state.style()->setNamedGridArea(state.parentStyle()->namedGridArea());
            state.style()->setNamedGridAreaRowCount(state.parentStyle()->namedGridAreaRowCount());
            state.style()->setNamedGridAreaColumnCount(state.parentStyle()->namedGridAreaColumnCount());
            return;
        }
        if (isInitial) {
            state.style()->setNamedGridArea(RenderStyle::initialNamedGridArea());
            state.style()->setNamedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount());
            state.style()->setNamedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount());
            return;
        }

        if (value->isPrimitiveValue()) {
            ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNone);
            return;
        }

        CSSGridTemplateValue* gridTemplateValue = toCSSGridTemplateValue(value);
        state.style()->setNamedGridArea(gridTemplateValue->gridAreaMap());
        state.style()->setNamedGridAreaRowCount(gridTemplateValue->rowCount());
        state.style()->setNamedGridAreaColumnCount(gridTemplateValue->columnCount());
        return;
    }

    // These properties are aliased and we already applied the property on the prefixed version.
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
        return;
    // These properties are implemented in StyleBuilder::applyProperty.
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxSizing:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyClip:
    case CSSPropertyColor:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyCursor:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFloat:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyHeight:
    case CSSPropertyImageRendering:
    case CSSPropertyIsolation:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyleImage:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMixBlendMode:
    case CSSPropertyMinWidth:
    case CSSPropertyObjectFit:
    case CSSPropertyOpacity:
    case CSSPropertyOrphans:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyRight:
    case CSSPropertySize:
    case CSSPropertySpeak:
    case CSSPropertyTabSize:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlign:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextIndent:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTop:
    case CSSPropertyTouchAction:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVariable:
    case CSSPropertyVerticalAlign:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitAspectRatio:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxFlex:
    case CSSPropertyWebkitBoxFlexGroup:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColumnAxis:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnProgression:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnSpan:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyAlignContent:
    case CSSPropertyAlignItems:
    case CSSPropertyAlignSelf:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFlexWrap:
    case CSSPropertyJustifyContent:
    case CSSPropertyOrder:
    case CSSPropertyWebkitFlowFrom:
    case CSSPropertyWebkitFlowInto:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitFontVariantLigatures:
    case CSSPropertyWebkitHighlight:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineClamp:
    case CSSPropertyWebkitLineGrid:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyInternalMarqueeDirection:
    case CSSPropertyInternalMarqueeIncrement:
    case CSSPropertyInternalMarqueeRepetition:
    case CSSPropertyInternalMarqueeSpeed:
    case CSSPropertyInternalMarqueeStyle:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageRepeat:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
    case CSSPropertyWebkitRegionBreakInside:
    case CSSPropertyWebkitRegionFragment:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextUnderlinePosition:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitClipPath:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitShapeMargin:
    case CSSPropertyWebkitShapePadding:
    case CSSPropertyWebkitWrapThrough:
    case CSSPropertyWebkitShapeInside:
    case CSSPropertyWebkitShapeOutside:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWidows:
    case CSSPropertyWidth:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
    case CSSPropertyWordWrap:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
    case CSSPropertyFontFamily:
    case CSSPropertyGridAutoFlow:
    case CSSPropertyMarker:
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyBufferedRendering:
    case CSSPropertyClipRule:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyColorRendering:
    case CSSPropertyDominantBaseline:
    case CSSPropertyFillRule:
    case CSSPropertyMaskSourceType:
    case CSSPropertyMaskType:
    case CSSPropertyShapeRendering:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyTextAnchor:
    case CSSPropertyVectorEffect:
    case CSSPropertyWritingMode:
        ASSERT_NOT_REACHED();
        return;
    // Only used in @viewport rules
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
        return;

    case CSSPropertyBaselineShift:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(baselineShift, BaselineShift);
        if (!primitiveValue)
            break;

        SVGRenderStyle* svgStyle = state.style()->accessSVGStyle();
        if (primitiveValue->getValueID()) {
            switch (primitiveValue->getValueID()) {
            case CSSValueBaseline:
                svgStyle->setBaselineShift(BS_BASELINE);
                break;
            case CSSValueSub:
                svgStyle->setBaselineShift(BS_SUB);
                break;
            case CSSValueSuper:
                svgStyle->setBaselineShift(BS_SUPER);
                break;
            default:
                break;
            }
        } else {
            svgStyle->setBaselineShift(BS_LENGTH);
            svgStyle->setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(primitiveValue));
        }

        break;
    }
    case CSSPropertyKerning:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(kerning, Kerning);
        if (primitiveValue)
            state.style()->accessSVGStyle()->setKerning(SVGLength::fromCSSPrimitiveValue(primitiveValue));
        break;
    }
    case CSSPropertyColorProfile:
    {
        // Not implemented.
        break;
    }
    // end of ident only properties
    case CSSPropertyFill:
    {
        SVGRenderStyle* svgStyle = state.style()->accessSVGStyle();
        if (isInherit) {
            const SVGRenderStyle* svgParentStyle = state.parentStyle()->svgStyle();
            svgStyle->setFillPaint(svgParentStyle->fillPaintType(), svgParentStyle->fillPaintColor(), svgParentStyle->fillPaintUri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
            return;
        }
        if (isInitial) {
            svgStyle->setFillPaint(SVGRenderStyle::initialFillPaintType(), SVGRenderStyle::initialFillPaintColor(), SVGRenderStyle::initialFillPaintUri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
            return;
        }
        if (value->isSVGPaint()) {
            SVGPaint* svgPaint = static_cast<SVGPaint*>(value);
            svgStyle->setFillPaint(svgPaint->paintType(), colorFromSVGColorCSSValue(svgPaint, state.style()->color()), svgPaint->uri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
        }
        break;
    }
    case CSSPropertyStroke:
    {
        SVGRenderStyle* svgStyle = state.style()->accessSVGStyle();
        if (isInherit) {
            const SVGRenderStyle* svgParentStyle = state.parentStyle()->svgStyle();
            svgStyle->setStrokePaint(svgParentStyle->strokePaintType(), svgParentStyle->strokePaintColor(), svgParentStyle->strokePaintUri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
            return;
        }
        if (isInitial) {
            svgStyle->setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
            return;
        }
        if (value->isSVGPaint()) {
            SVGPaint* svgPaint = static_cast<SVGPaint*>(value);
            svgStyle->setStrokePaint(svgPaint->paintType(), colorFromSVGColorCSSValue(svgPaint, state.style()->color()), svgPaint->uri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
        }
        break;
    }
    case CSSPropertyStrokeWidth:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(strokeWidth, StrokeWidth)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setStrokeWidth(SVGLength::fromCSSPrimitiveValue(primitiveValue));
        break;
    }
    case CSSPropertyStrokeDasharray:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
        if (!value->isValueList()) {
            state.style()->accessSVGStyle()->setStrokeDashArray(SVGRenderStyle::initialStrokeDashArray());
            break;
        }

        CSSValueList* dashes = toCSSValueList(value);

        Vector<SVGLength> array;
        size_t length = dashes->length();
        for (size_t i = 0; i < length; ++i) {
            CSSValue* currValue = dashes->itemWithoutBoundsCheck(i);
            if (!currValue->isPrimitiveValue())
                continue;

            CSSPrimitiveValue* dash = toCSSPrimitiveValue(dashes->itemWithoutBoundsCheck(i));
            array.append(SVGLength::fromCSSPrimitiveValue(dash));
        }

        state.style()->accessSVGStyle()->setStrokeDashArray(array);
        break;
    }
    case CSSPropertyStrokeDashoffset:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(strokeDashOffset, StrokeDashOffset)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setStrokeDashOffset(SVGLength::fromCSSPrimitiveValue(primitiveValue));
        break;
    }
    case CSSPropertyFillOpacity:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(fillOpacity, FillOpacity)
        float f = 0.0f;
        if (percentageOrNumberToFloat(primitiveValue, f))
            state.style()->accessSVGStyle()->setFillOpacity(f);
        break;
    }
    case CSSPropertyStrokeOpacity:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(strokeOpacity, StrokeOpacity)
        float f = 0.0f;
        if (percentageOrNumberToFloat(primitiveValue, f))
            state.style()->accessSVGStyle()->setStrokeOpacity(f);
        break;
    }
    case CSSPropertyStopOpacity:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(stopOpacity, StopOpacity)
        float f = 0.0f;
        if (percentageOrNumberToFloat(primitiveValue, f))
            state.style()->accessSVGStyle()->setStopOpacity(f);
        break;
    }
    case CSSPropertyMarkerStart:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(markerStartResource, MarkerStartResource)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setMarkerStartResource(fragmentIdentifier(primitiveValue, state.document()));
        break;
    }
    case CSSPropertyMarkerMid:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(markerMidResource, MarkerMidResource)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setMarkerMidResource(fragmentIdentifier(primitiveValue, state.document()));
        break;
    }
    case CSSPropertyMarkerEnd:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(markerEndResource, MarkerEndResource)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setMarkerEndResource(fragmentIdentifier(primitiveValue, state.document()));
        break;
    }
    case CSSPropertyStrokeMiterlimit:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(strokeMiterLimit, StrokeMiterLimit)
        float f = 0.0f;
        if (numberToFloat(primitiveValue, f))
            state.style()->accessSVGStyle()->setStrokeMiterLimit(f);
        break;
    }
    case CSSPropertyFilter:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(filterResource, FilterResource)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setFilterResource(fragmentIdentifier(primitiveValue, state.document()));
        break;
    }
    case CSSPropertyMask:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(maskerResource, MaskerResource)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setMaskerResource(fragmentIdentifier(primitiveValue, state.document()));
        break;
    }
    case CSSPropertyClipPath:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(clipperResource, ClipperResource)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setClipperResource(fragmentIdentifier(primitiveValue, state.document()));
        break;
    }
    case CSSPropertyStopColor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(stopColor, StopColor);
        if (value->isSVGColor())
            state.style()->accessSVGStyle()->setStopColor(colorFromSVGColorCSSValue(static_cast<SVGColor*>(value), state.style()->color()));
        break;
    }
    case CSSPropertyLightingColor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(lightingColor, LightingColor);
        if (value->isSVGColor())
            state.style()->accessSVGStyle()->setLightingColor(colorFromSVGColorCSSValue(static_cast<SVGColor*>(value), state.style()->color()));
        break;
    }
    case CSSPropertyFloodOpacity:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(floodOpacity, FloodOpacity)
        float f = 0.0f;
        if (percentageOrNumberToFloat(primitiveValue, f))
            state.style()->accessSVGStyle()->setFloodOpacity(f);
        break;
    }
    case CSSPropertyFloodColor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(floodColor, FloodColor);
        if (value->isSVGColor())
            state.style()->accessSVGStyle()->setFloodColor(colorFromSVGColorCSSValue(static_cast<SVGColor*>(value), state.style()->color()));
        break;
    }
    case CSSPropertyGlyphOrientationHorizontal:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(glyphOrientationHorizontal, GlyphOrientationHorizontal)
        EGlyphOrientation orientation;
        if (degreeToGlyphOrientation(primitiveValue, orientation))
            state.style()->accessSVGStyle()->setGlyphOrientationHorizontal(orientation);
        break;
    }
    case CSSPropertyPaintOrder: {
        HANDLE_SVG_INHERIT_AND_INITIAL(paintOrder, PaintOrder)
        if (value->isValueList())
            state.style()->accessSVGStyle()->setPaintOrder(paintOrderFlattened(value));
        break;
    }
    case CSSPropertyGlyphOrientationVertical:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(glyphOrientationVertical, GlyphOrientationVertical)
        if (primitiveValue->getValueID() == CSSValueAuto) {
            state.style()->accessSVGStyle()->setGlyphOrientationVertical(GO_AUTO);
            break;
        }
        EGlyphOrientation orientation;
        if (degreeToGlyphOrientation(primitiveValue, orientation))
            state.style()->accessSVGStyle()->setGlyphOrientationVertical(orientation);
        break;
    }
    case CSSPropertyEnableBackground:
        // Silently ignoring this property for now
        // http://bugs.webkit.org/show_bug.cgi?id=6022
        break;
    }
}

} // namespace WebCore
