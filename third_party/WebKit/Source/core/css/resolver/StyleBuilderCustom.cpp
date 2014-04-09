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
#include "StyleBuilderFunctions.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "StylePropertyShorthand.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFontValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/Counter.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/rendering/style/CounterContent.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/SVGRenderStyle.h"
#include "core/rendering/style/SVGRenderStyleDefs.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/svg/SVGPaint.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace WebCore {

static Length clipConvertToLength(StyleResolverState& state, CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedConversion | PercentConversion | AutoConversion>(state.cssToLengthConversionData());
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

void StyleBuilderFunctions::applyInitialCSSPropertyColor(StyleResolverState& state)
{
    Color color = RenderStyle::initialColor();
    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(color);
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(color);
}

void StyleBuilderFunctions::applyInheritCSSPropertyColor(StyleResolverState& state)
{
    Color color = state.parentStyle()->color();
    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(color);
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(color);
}

void StyleBuilderFunctions::applyValueCSSPropertyColor(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    // As per the spec, 'color: currentColor' is treated as 'color: inherit'
    if (primitiveValue->getValueID() == CSSValueCurrentcolor) {
        applyInheritCSSPropertyColor(state);
        return;
    }

    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->color()));
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->color(), true));
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
                CSSCursorImageValue* image = toCSSCursorImageValue(item);
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
    state.fontBuilder().setFontFamilyInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontFamily(StyleResolverState& state)
{
    state.fontBuilder().setFontFamilyInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontFamily(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontFamilyValue(value);
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontSize(StyleResolverState& state)
{
    state.fontBuilder().setFontSizeInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontSize(StyleResolverState& state)
{
    state.fontBuilder().setFontSizeInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontSize(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontSizeValue(value, state.parentStyle(), state.rootElementStyle());
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
        state.fontBuilder().setWeight(state.parentStyle()->fontDescription().weight());
        state.fontBuilder().setWeightBolder();
        break;
    case CSSValueLighter:
        state.fontBuilder().setWeight(state.parentStyle()->fontDescription().weight());
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
        float multiplier = state.style()->effectiveZoom();
        if (LocalFrame* frame = state.document().frame())
            multiplier *= frame->textZoomFactor();
        lineHeight = primitiveValue->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(multiplier));
    } else if (primitiveValue->isPercentage()) {
        lineHeight = Length((state.style()->computedFontSize() * primitiveValue->getIntValue()) / 100.0, Fixed);
    } else if (primitiveValue->isNumber()) {
        lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
    } else if (primitiveValue->isCalculated()) {
        double multiplier = state.style()->effectiveZoom();
        if (LocalFrame* frame = state.document().frame())
            multiplier *= frame->textZoomFactor();
        Length zoomedLength = Length(primitiveValue->cssCalcValue()->toCalcValue(state.cssToLengthConversionData().copyWithAdjustedZoom(multiplier)));
        lineHeight = Length(valueForLength(zoomedLength, state.style()->fontSize()), Fixed);
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

static Length mmLength(double mm) { return Length(mm * cssPixelsPerMillimeter, Fixed); }
static Length inchLength(double inch) { return Length(inch * cssPixelsPerInch, Fixed); }
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
            width = first->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            height = second->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
        } else {
            // <page-size> <orientation>
            // The value order is guaranteed. See BisonCSSParser::parseSizeParameter.
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
            width = height = primitiveValue->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
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
    // The order is guaranteed. See BisonCSSParser::parseTextIndent.
    // The second value, each-line is handled only when css3TextEnabled() returns true.

    CSSValueList* valueList = toCSSValueList(value);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(valueList->itemWithoutBoundsCheck(0));
    Length lengthOrPercentageValue = primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
    state.style()->setTextIndent(lengthOrPercentageValue);

    ASSERT(valueList->length() <= 2);
    CSSPrimitiveValue* eachLineValue = toCSSPrimitiveValue(valueList->item(1));
    if (eachLineValue) {
        ASSERT(eachLineValue->getValueID() == CSSValueEachLine);
        state.style()->setTextIndentLine(TextIndentEachLine);
    } else
        state.style()->setTextIndentLine(TextIndentFirstLine);
}

void StyleBuilderFunctions::applyInitialCSSPropertyTransformOrigin(StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitTransformOriginX(state);
    applyInitialCSSPropertyWebkitTransformOriginY(state);
    applyInitialCSSPropertyWebkitTransformOriginZ(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyTransformOrigin(StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitTransformOriginX(state);
    applyInheritCSSPropertyWebkitTransformOriginY(state);
    applyInheritCSSPropertyWebkitTransformOriginZ(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyTransformOrigin(StyleResolverState& state, CSSValue* value)
{
    CSSValueList* list = toCSSValueList(value);
    ASSERT(list->length() == 3);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(list->item(0));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueLeft:
            state.style()->setTransformOriginX(Length(0, Percent));
            break;
        case CSSValueRight:
            state.style()->setTransformOriginX(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setTransformOriginX(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setTransformOriginX(StyleBuilderConverter::convertLength(state, primitiveValue));
    }

    primitiveValue = toCSSPrimitiveValue(list->item(1));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueTop:
            state.style()->setTransformOriginY(Length(0, Percent));
            break;
        case CSSValueBottom:
            state.style()->setTransformOriginY(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setTransformOriginY(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setTransformOriginY(StyleBuilderConverter::convertLength(state, primitiveValue));
    }

    primitiveValue = toCSSPrimitiveValue(list->item(2));
    state.style()->setTransformOriginZ(StyleBuilderConverter::convertComputedLength<float>(state, primitiveValue));
}

void StyleBuilderFunctions::applyInitialCSSPropertyPerspectiveOrigin(StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitPerspectiveOriginX(state);
    applyInitialCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyPerspectiveOrigin(StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitPerspectiveOriginX(state);
    applyInheritCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyPerspectiveOrigin(StyleResolverState& state, CSSValue* value)
{
    CSSValueList* list = toCSSValueList(value);
    ASSERT(list->length() == 2);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(list->item(0));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueLeft:
            state.style()->setPerspectiveOriginX(Length(0, Percent));
            break;
        case CSSValueRight:
            state.style()->setPerspectiveOriginX(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setPerspectiveOriginX(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setPerspectiveOriginX(StyleBuilderConverter::convertLength(state, primitiveValue));
    }

    primitiveValue = toCSSPrimitiveValue(list->item(1));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueTop:
            state.style()->setPerspectiveOriginY(Length(0, Percent));
            break;
        case CSSValueBottom:
            state.style()->setPerspectiveOriginY(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setPerspectiveOriginY(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setPerspectiveOriginY(StyleBuilderConverter::convertLength(state, primitiveValue));
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyVerticalAlign(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID())
        return state.style()->setVerticalAlign(*primitiveValue);

    state.style()->setVerticalAlignLength(primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData()));
}

void StyleBuilderFunctions::applyValueCSSPropertyTouchAction(StyleResolverState& state, CSSValue* value)
{
    TouchAction action = RenderStyle::initialTouchAction();
    for (CSSValueListIterator i(value); i.hasMore(); i.advance())
        action |= *toCSSPrimitiveValue(i.value());

    state.style()->setTouchAction(action);
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
    CSSAspectRatioValue* aspectRatioValue = toCSSAspectRatioValue(value);
    state.style()->setHasAspectRatio(true);
    state.style()->setAspectRatioDenominator(aspectRatioValue->denominatorValue());
    state.style()->setAspectRatioNumerator(aspectRatioValue->numeratorValue());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitBorderImage(StyleResolverState& state, CSSValue* value)
{
    NinePieceImage image;
    state.styleMap().mapNinePieceImage(state.style(), CSSPropertyWebkitBorderImage, value, image);
    state.style()->setBorderImage(image);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitClipPath(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setClipPath(nullptr);
        } else if (primitiveValue->isShape()) {
            state.style()->setClipPath(ShapeClipPathOperation::create(basicShapeForValue(state, primitiveValue->getShapeValue())));
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue->getStringValue();
            KURL url = state.document().completeURL(cssURLValue);
            // FIXME: It doesn't work with forward or external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=90405)
            state.style()->setClipPath(ReferenceClipPathOperation::create(cssURLValue, AtomicString(url.fragmentIdentifier())));
        }
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontVariantLigatures(StyleResolverState& state)
{
    state.fontBuilder().setFontVariantLigaturesInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontVariantLigatures(StyleResolverState& state)
{
    state.fontBuilder().setFontVariantLigaturesInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontVariantLigatures(StyleResolverState& state, CSSValue* value)
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
        Length marqueeLength = primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
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
        state.style()->setTextEmphasisCustomMark(AtomicString(primitiveValue->getStringValue()));
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

void StyleBuilderFunctions::applyValueCSSPropertyTextUnderlinePosition(StyleResolverState& state, CSSValue* value)
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

void StyleBuilderFunctions::applyInitialCSSPropertyWillChange(StyleResolverState& state)
{
    state.style()->setWillChangeContents(false);
    state.style()->setWillChangeScrollPosition(false);
    state.style()->setWillChangeProperties(Vector<CSSPropertyID>());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWillChange(StyleResolverState& state)
{
    state.style()->setWillChangeContents(state.parentStyle()->willChangeContents());
    state.style()->setWillChangeScrollPosition(state.parentStyle()->willChangeScrollPosition());
    state.style()->setWillChangeProperties(state.parentStyle()->willChangeProperties());
}

void StyleBuilderFunctions::applyValueCSSPropertyWillChange(StyleResolverState& state, CSSValue* value)
{
    ASSERT(value->isValueList());
    bool willChangeContents = false;
    bool willChangeScrollPosition = false;
    Vector<CSSPropertyID> willChangeProperties;

    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(i.value());
        if (CSSPropertyID propertyID = primitiveValue->getPropertyID())
            willChangeProperties.append(propertyID);
        else if (primitiveValue->getValueID() == CSSValueContents)
            willChangeContents = true;
        else if (primitiveValue->getValueID() == CSSValueScrollPosition)
            willChangeScrollPosition = true;
        else
            ASSERT_NOT_REACHED();
    }
    state.style()->setWillChangeContents(willChangeContents);
    state.style()->setWillChangeScrollPosition(willChangeScrollPosition);
    state.style()->setWillChangeProperties(willChangeProperties);
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

static GridLength createGridTrackBreadth(CSSPrimitiveValue* primitiveValue, const StyleResolverState& state)
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

static GridTrackSize createGridTrackSize(CSSValue* value, const StyleResolverState& state)
{
    if (value->isPrimitiveValue())
        return GridTrackSize(createGridTrackBreadth(toCSSPrimitiveValue(value), state));

    CSSFunctionValue* minmaxFunction = toCSSFunctionValue(value);
    CSSValueList* arguments = minmaxFunction->arguments();
    ASSERT_WITH_SECURITY_IMPLICATION(arguments->length() == 2);
    GridLength minTrackBreadth(createGridTrackBreadth(toCSSPrimitiveValue(arguments->itemWithoutBoundsCheck(0)), state));
    GridLength maxTrackBreadth(createGridTrackBreadth(toCSSPrimitiveValue(arguments->itemWithoutBoundsCheck(1)), state));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
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
        trackSizes.append(createGridTrackSize(currValue, state));
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

static Color colorFromSVGPaintCSSValue(SVGPaint* svgPaint, const Color& fgColor)
{
    Color color;
    if (svgPaint->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR
        || svgPaint->paintType() == SVGPaint::SVG_PAINTTYPE_URI_CURRENTCOLOR)
        color = fgColor;
    else
        color = svgPaint->color();
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


void StyleBuilder::applyProperty(CSSPropertyID id, StyleResolverState& state, CSSValue* value)
{
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

    if (StyleBuilder::applyProperty(id, state, value, isInitial, isInherit))
        return;

    // Fall back to the old switch statement, which is now in StyleBuilderCustom.cpp
    StyleBuilder::oldApplyProperty(id, state, value, isInitial, isInherit);
}


void StyleBuilder::oldApplyProperty(CSSPropertyID id, StyleResolverState& state, CSSValue* value, bool isInitial, bool isInherit)
{
    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;

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
                        state.style()->setContent(StyleGeneratedImage::create(toCSSGradientValue(item)->gradientWithStylesResolved(state.document().textLinkColors(), state.style()->color()).get()), didSet);
                    else
                        state.style()->setContent(StyleGeneratedImage::create(toCSSImageGeneratorValue(item)), didSet);
                    didSet = true;
                } else if (item->isImageSetValue()) {
                    state.style()->setContent(state.elementStyleResources().setOrPendingFromValue(CSSPropertyContent, toCSSImageSetValue(item)), didSet);
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
                    QualifiedName attr(nullAtom, AtomicString(contentValue->getStringValue()), nullAtom);
                    const AtomicString& value = state.element()->getAttribute(attr);
                    state.style()->setContent(value.isNull() ? emptyString() : value.string(), didSet);
                    didSet = true;
                    // register the fact that the attribute value affects the style
                    state.contentAttrValues().append(attr.localName());
                } else if (contentValue->isCounter()) {
                    Counter* counterValue = contentValue->getCounterValue();
                    EListStyleType listStyleType = NoneListStyle;
                    CSSValueID listStyleIdent = counterValue->listStyleIdent();
                    if (listStyleIdent != CSSValueNone)
                        listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
                    OwnPtr<CounterContent> counter = adoptPtr(new CounterContent(AtomicString(counterValue->identifier()), listStyleType, AtomicString(counterValue->separator())));
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
    case CSSPropertyObjectPosition:
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
    case CSSPropertyGrid:
    case CSSPropertyGridTemplate:
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
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            state.style()->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }

        if (!value->isReflectValue())
            return;

        CSSReflectValue* reflectValue = toCSSReflectValue(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(*reflectValue->direction());
        if (reflectValue->offset())
            reflection->setOffset(reflectValue->offset()->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData()));
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
            state.style()->setLocale(AtomicString(primitiveValue->getStringValue()));
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
            width = CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS)->computeLength<float>(state.cssToLengthConversionData());
            break;
        }
        default:
            width = primitiveValue->computeLength<float>(state.cssToLengthConversionData());
            break;
        }
        state.style()->setTextStrokeWidth(width);
        return;
    }
    case CSSPropertyTransform:
    case CSSPropertyWebkitTransform: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        TransformBuilder::createTransformOperations(value, state.cssToLengthConversionData(), operations);
        state.style()->setTransform(operations);
        return;
    }
    case CSSPropertyPerspective:
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
            perspectiveValue = primitiveValue->computeLength<float>(state.cssToLengthConversionData());
        } else if (id == CSSPropertyWebkitPerspective && primitiveValue->isNumber()) {
            // Prefixed version treats unitless numbers as px.
            perspectiveValue = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX)->computeLength<float>(state.cssToLengthConversionData());
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

        Color col = state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->color());
        state.style()->setTapHighlightColor(col);
        return;
    }
    case CSSPropertyInternalCallback: {
        if (isInherit || isInitial)
            return;
        if (primitiveValue && primitiveValue->getValueID() == CSSValueInternalPresence) {
            state.style()->addCallbackSelector(state.currentRule()->selectorList().selectorsText());
            return;
        }
        break;
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

        if (!value->isLineBoxContainValue())
            return;

        state.style()->setLineBoxContain(toCSSLineBoxContainValue(value)->value());
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
        if (FilterOperationResolver::createFilterOperations(value, state.cssToLengthConversionData(), operations, state))
            state.style()->setFilter(operations);
        return;
    }
    case CSSPropertyGridAutoColumns: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoColumns, GridAutoColumns);
        state.style()->setGridAutoColumns(createGridTrackSize(value, state));
        return;
    }
    case CSSPropertyGridAutoRows: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoRows, GridAutoRows);
        state.style()->setGridAutoRows(createGridTrackSize(value, state));
        return;
    }
    case CSSPropertyGridTemplateColumns: {
        if (isInherit) {
            state.style()->setGridTemplateColumns(state.parentStyle()->gridTemplateColumns());
            state.style()->setNamedGridColumnLines(state.parentStyle()->namedGridColumnLines());
            state.style()->setOrderedNamedGridColumnLines(state.parentStyle()->orderedNamedGridColumnLines());
            return;
        }
        if (isInitial) {
            state.style()->setGridTemplateColumns(RenderStyle::initialGridTemplateColumns());
            state.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
            state.style()->setOrderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        OrderedNamedGridLines orderedNamedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, orderedNamedGridLines, state))
            return;
        state.style()->setGridTemplateColumns(trackSizes);
        state.style()->setNamedGridColumnLines(namedGridLines);
        state.style()->setOrderedNamedGridColumnLines(orderedNamedGridLines);
        return;
    }
    case CSSPropertyGridTemplateRows: {
        if (isInherit) {
            state.style()->setGridTemplateRows(state.parentStyle()->gridTemplateRows());
            state.style()->setNamedGridRowLines(state.parentStyle()->namedGridRowLines());
            state.style()->setOrderedNamedGridRowLines(state.parentStyle()->orderedNamedGridRowLines());
            return;
        }
        if (isInitial) {
            state.style()->setGridTemplateRows(RenderStyle::initialGridTemplateRows());
            state.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
            state.style()->setOrderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        OrderedNamedGridLines orderedNamedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, orderedNamedGridLines, state))
            return;
        state.style()->setGridTemplateRows(trackSizes);
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

    case CSSPropertyGridTemplateAreas: {
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

        CSSGridTemplateAreasValue* gridTemplateAreasValue = toCSSGridTemplateAreasValue(value);
        state.style()->setNamedGridArea(gridTemplateAreasValue->gridAreaMap());
        state.style()->setNamedGridAreaRowCount(gridTemplateAreasValue->rowCount());
        state.style()->setNamedGridAreaColumnCount(gridTemplateAreasValue->columnCount());
        return;
    }

    case CSSPropertyJustifySelf: {
        HANDLE_INHERIT_AND_INITIAL(justifySelf, JustifySelf);
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (Pair* pairValue = primitiveValue->getPairValue()) {
            state.style()->setJustifySelf(*pairValue->first());
            state.style()->setJustifySelfOverflowAlignment(*pairValue->second());
        } else {
            state.style()->setJustifySelf(*primitiveValue);
        }
        return;
    }

    case CSSPropertyAlignSelf: {
        HANDLE_INHERIT_AND_INITIAL(alignSelf, AlignSelf);
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (Pair* pairValue = primitiveValue->getPairValue()) {
            state.style()->setAlignSelf(*pairValue->first());
            state.style()->setAlignSelfOverflowAlignment(*pairValue->second());
        } else {
            state.style()->setAlignSelf(*primitiveValue);
        }
        return;
    }

    case CSSPropertyAlignItems: {
        HANDLE_INHERIT_AND_INITIAL(alignItems, AlignItems);
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (Pair* pairValue = primitiveValue->getPairValue()) {
            state.style()->setAlignItems(*pairValue->first());
            state.style()->setAlignItemsOverflowAlignment(*pairValue->second());
        } else {
            state.style()->setAlignItems(*primitiveValue);
        }
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
    case CSSPropertyBoxShadow:
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
    case CSSPropertyFontKerning:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontVariantLigatures:
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
    case CSSPropertyScrollBehavior:
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
    case CSSPropertyTextJustify:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextShadow:
    case CSSPropertyTextTransform:
    case CSSPropertyTop:
    case CSSPropertyTouchAction:
    case CSSPropertyTouchActionDelay:
    case CSSPropertyUnicodeBidi:
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
    case CSSPropertyBackfaceVisibility:
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
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyColumnFill:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnSpan:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyAlignContent:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFlexWrap:
    case CSSPropertyJustifyContent:
    case CSSPropertyOrder:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitHighlight:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineClamp:
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
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyTransformOrigin:
    case CSSPropertyTransformStyle:
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
    case CSSPropertyShapeMargin:
    case CSSPropertyShapeImageThreshold:
    case CSSPropertyWebkitWrapThrough:
    case CSSPropertyShapeOutside:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWidows:
    case CSSPropertyWidth:
    case CSSPropertyWillChange:
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
    case CSSPropertyClipPath:
    case CSSPropertyFillOpacity:
    case CSSPropertyFilter:
    case CSSPropertyFloodOpacity:
    case CSSPropertyKerning:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerStart:
    case CSSPropertyMask:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyStrokeWidth:
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
            SVGPaint* svgPaint = toSVGPaint(value);
            svgStyle->setFillPaint(svgPaint->paintType(), colorFromSVGPaintCSSValue(svgPaint, state.style()->color()), svgPaint->uri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
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
            SVGPaint* svgPaint = toSVGPaint(value);
            svgStyle->setStrokePaint(svgPaint->paintType(), colorFromSVGPaintCSSValue(svgPaint, state.style()->color()), svgPaint->uri(), state.applyPropertyToRegularStyle(), state.applyPropertyToVisitedLinkStyle());
        }
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

        RefPtr<SVGLengthList> array = SVGLengthList::create();
        size_t length = dashes->length();
        for (size_t i = 0; i < length; ++i) {
            CSSValue* currValue = dashes->itemWithoutBoundsCheck(i);
            if (!currValue->isPrimitiveValue())
                continue;

            CSSPrimitiveValue* dash = toCSSPrimitiveValue(dashes->itemWithoutBoundsCheck(i));
            array->append(SVGLength::fromCSSPrimitiveValue(dash));
        }

        state.style()->accessSVGStyle()->setStrokeDashArray(array.release());
        break;
    }
    case CSSPropertyStopColor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(stopColor, StopColor);
        if (primitiveValue->isRGBColor())
            state.style()->accessSVGStyle()->setStopColor(primitiveValue->getRGBA32Value());
        else if (primitiveValue->getValueID() == CSSValueCurrentcolor)
            state.style()->accessSVGStyle()->setStopColor(state.style()->color());
        break;
    }
    case CSSPropertyLightingColor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(lightingColor, LightingColor);
        if (primitiveValue->isRGBColor())
            state.style()->accessSVGStyle()->setLightingColor(primitiveValue->getRGBA32Value());
        else if (primitiveValue->getValueID() == CSSValueCurrentcolor)
            state.style()->accessSVGStyle()->setLightingColor(state.style()->color());
        break;
    }
    case CSSPropertyFloodColor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(floodColor, FloodColor);
        if (primitiveValue->isRGBColor())
            state.style()->accessSVGStyle()->setFloodColor(primitiveValue->getRGBA32Value());
        else if (primitiveValue->getValueID() == CSSValueCurrentcolor)
            state.style()->accessSVGStyle()->setFloodColor(state.style()->color());
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
