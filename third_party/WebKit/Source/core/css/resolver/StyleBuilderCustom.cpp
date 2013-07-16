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
#include "core/css/resolver/StyleBuilder.h"

// FIXME: This is way more than we need to include!
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "StyleBuilderFunctions.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/Animation.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/Counter.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/FontFeatureValue.h"
#include "core/css/FontSize.h"
#include "core/css/FontValue.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/RuleSet.h"
#include "core/css/ShadowValue.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StylePropertyShorthand.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/LinkHash.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/platform/graphics/filters/custom/CustomFilterConstants.h"
#include "core/platform/text/LocaleToScriptMapping.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/CounterContent.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/SVGRenderStyle.h"
#include "core/rendering/style/SVGRenderStyleDefs.h"
#include "core/rendering/style/ShadowData.h"
#include "core/rendering/style/StyleCachedImage.h"
#include "core/rendering/style/StyleCachedImageSet.h"
#include "core/rendering/style/StyleCustomFilterProgramCache.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/rendering/style/StylePendingImage.h"
#include "core/rendering/style/StylePendingShader.h"
#include "core/rendering/style/StyleShader.h"
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

void StyleBuilderFunctions::applyInitialCSSPropertyClip(StyleResolver*, StyleResolverState& state)
{
    state.style()->setClip(Length(), Length(), Length(), Length());
    state.style()->setHasClip(false);
}

void StyleBuilderFunctions::applyInheritCSSPropertyClip(StyleResolver* styleResolver, StyleResolverState& state)
{
    RenderStyle* parentStyle = state.parentStyle();
    if (!parentStyle->hasClip())
        return applyInitialCSSPropertyClip(styleResolver, state);
    state.style()->setClip(parentStyle->clipTop(), parentStyle->clipRight(), parentStyle->clipBottom(), parentStyle->clipLeft());
    state.style()->setHasClip(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyClip(StyleResolver*, StyleResolverState& state, CSSValue* value)
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

void StyleBuilderFunctions::applyInitialCSSPropertyCursor(StyleResolver*, StyleResolverState& state)
{
    state.style()->clearCursorList();
    state.style()->setCursor(RenderStyle::initialCursor());
}

void StyleBuilderFunctions::applyInheritCSSPropertyCursor(StyleResolver*, StyleResolverState& state)
{
    state.style()->setCursor(state.parentStyle()->cursor());
    state.style()->setCursorList(state.parentStyle()->cursors());
}

void StyleBuilderFunctions::applyValueCSSPropertyCursor(StyleResolver*, StyleResolverState& state, CSSValue* value)
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

void StyleBuilderFunctions::applyValueCSSPropertyDirection(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    state.style()->setDirection(*toCSSPrimitiveValue(value));
    Element* element = state.element();
    if (element && element == element->document()->documentElement())
        element->document()->setDirectionSetOnDocumentElement(true);
}

static inline bool isValidDisplayValue(StyleResolverState& state, EDisplay displayPropertyValue)
{
    if (state.element() && state.element()->isSVGElement() && state.style()->styleType() == NOPSEUDO)
        return (displayPropertyValue == INLINE || displayPropertyValue == BLOCK || displayPropertyValue == NONE);
    return true;
}

void StyleBuilderFunctions::applyInheritCSSPropertyDisplay(StyleResolver*, StyleResolverState& state)
{
    EDisplay display = state.parentStyle()->display();
    if (!isValidDisplayValue(state, display))
        return;
    state.style()->setDisplay(display);
}

void StyleBuilderFunctions::applyValueCSSPropertyDisplay(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    EDisplay display = *toCSSPrimitiveValue(value);

    if (!isValidDisplayValue(state, display))
        return;

    state.style()->setDisplay(display);
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontFamily(StyleResolver* styleResolver, StyleResolverState& state)
{
    FontDescription fontDescription = state.style()->fontDescription();
    FontDescription initialDesc = FontDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize())
        styleResolver->setFontSize(fontDescription, FontSize::fontSizeForKeyword(styleResolver->document(), CSSValueXxSmall + fontDescription.keywordSize() - 1, false));
    fontDescription.setGenericFamily(initialDesc.genericFamily());
    if (!initialDesc.firstFamily().familyIsEmpty())
        fontDescription.setFamily(initialDesc.firstFamily());

    state.setFontDescription(fontDescription);
    return;
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontFamily(StyleResolver*, StyleResolverState& state)
{
    FontDescription fontDescription = state.style()->fontDescription();
    FontDescription parentFontDescription = state.parentStyle()->fontDescription();

    fontDescription.setGenericFamily(parentFontDescription.genericFamily());
    fontDescription.setFamily(parentFontDescription.firstFamily());
    fontDescription.setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
    state.setFontDescription(fontDescription);
    return;
}

void StyleBuilderFunctions::applyValueCSSPropertyFontFamily(StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
{
    if (!value->isValueList())
        return;

    FontDescription fontDescription = state.style()->fontDescription();
    FontFamily& firstFamily = fontDescription.firstFamily();
    FontFamily* currFamily = 0;

    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();
    fontDescription.setGenericFamily(FontDescription::NoFamily);

    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        if (!item->isPrimitiveValue())
            continue;
        CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);
        AtomicString face;
        Settings* settings = styleResolver->document()->settings();
        if (contentValue->isString()) {
            face = contentValue->getStringValue();
        } else if (settings) {
            switch (contentValue->getValueID()) {
            case CSSValueWebkitBody:
                face = settings->standardFontFamily();
                break;
            case CSSValueSerif:
                face = serifFamily;
                fontDescription.setGenericFamily(FontDescription::SerifFamily);
                break;
            case CSSValueSansSerif:
                face = sansSerifFamily;
                fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                break;
            case CSSValueCursive:
                face = cursiveFamily;
                fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                break;
            case CSSValueFantasy:
                face = fantasyFamily;
                fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                break;
            case CSSValueMonospace:
                face = monospaceFamily;
                fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
                break;
            case CSSValueWebkitPictograph:
                face = pictographFamily;
                fontDescription.setGenericFamily(FontDescription::PictographFamily);
                break;
            default:
                break;
            }
        }

        if (!face.isEmpty()) {
            if (!currFamily) {
                // Filling in the first family.
                firstFamily.setFamily(face);
                firstFamily.appendFamily(0); // Remove any inherited family-fallback list.
                currFamily = &firstFamily;
                fontDescription.setIsSpecifiedFont(fontDescription.genericFamily() == FontDescription::NoFamily);
            } else {
                RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
                newFamily->setFamily(face);
                currFamily->appendFamily(newFamily);
                currFamily = newFamily.get();
            }
        }
    }

    // We can't call useFixedDefaultSize() until all new font families have been added
    // If currFamily is non-zero then we set at least one family on this description.
    if (currFamily) {
        if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize)
            styleResolver->setFontSize(fontDescription, FontSize::fontSizeForKeyword(styleResolver->document(), CSSValueXxSmall + fontDescription.keywordSize() - 1, !oldFamilyUsedFixedDefaultSize));

        state.setFontDescription(fontDescription);
    }
    return;
}

// FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large)
// and scale down/up to the next size level.
static float largerFontSize(float size)
{
    return size * 1.2f;
}

static float smallerFontSize(float size)
{
    return size / 1.2f;
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontSize(StyleResolver* styleResolver, StyleResolverState& state)
{
    FontDescription fontDescription = state.style()->fontDescription();
    float size = FontSize::fontSizeForKeyword(styleResolver->document(), CSSValueMedium, fontDescription.useFixedDefaultSize());

    if (size < 0)
        return;

    fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
    styleResolver->setFontSize(fontDescription, size);
    state.setFontDescription(fontDescription);
    return;
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontSize(StyleResolver* styleResolver, StyleResolverState& state)
{
    float size = state.parentStyle()->fontDescription().specifiedSize();

    if (size < 0)
        return;

    FontDescription fontDescription = state.style()->fontDescription();
    fontDescription.setKeywordSize(state.parentStyle()->fontDescription().keywordSize());
    styleResolver->setFontSize(fontDescription, size);
    state.setFontDescription(fontDescription);
    return;
}

void StyleBuilderFunctions::applyValueCSSPropertyFontSize(StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    FontDescription fontDescription = state.style()->fontDescription();
    fontDescription.setKeywordSize(0);
    float parentSize = 0;
    bool parentIsAbsoluteSize = false;
    float size = 0;

    if (state.parentStyle()) {
        parentSize = state.parentStyle()->fontDescription().specifiedSize();
        parentIsAbsoluteSize = state.parentStyle()->fontDescription().isAbsoluteSize();
    }

    if (CSSValueID valueID = primitiveValue->getValueID()) {
        // Keywords are being used.
        switch (valueID) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueWebkitXxxLarge:
            size = FontSize::fontSizeForKeyword(styleResolver->document(), valueID, fontDescription.useFixedDefaultSize());
            fontDescription.setKeywordSize(valueID - CSSValueXxSmall + 1);
            break;
        case CSSValueLarger:
            size = largerFontSize(parentSize);
            break;
        case CSSValueSmaller:
            size = smallerFontSize(parentSize);
            break;
        default:
            return;
        }

        fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && (valueID == CSSValueLarger || valueID == CSSValueSmaller));
    } else {
        fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize || !(primitiveValue->isPercentage() || primitiveValue->isFontRelativeLength()));
        if (primitiveValue->isLength())
            size = primitiveValue->computeLength<float>(state.parentStyle(), state.rootElementStyle(), 1.0, true);
        else if (primitiveValue->isPercentage())
            size = (primitiveValue->getFloatValue() * parentSize) / 100.0f;
        else if (primitiveValue->isCalculatedPercentageWithLength())
            size = primitiveValue->cssCalcValue()->toCalcValue(state.parentStyle(), state.rootElementStyle())->evaluate(parentSize);
        else if (primitiveValue->isViewportPercentageLength())
            size = valueForLength(primitiveValue->viewportPercentageLength(), 0, styleResolver->document()->renderView());
        else
            return;
    }

    if (size < 0)
        return;

    // Overly large font sizes will cause crashes on some platforms (such as Windows).
    // Cap font size here to make sure that doesn't happen.
    size = std::min(maximumAllowedFontSize, size);

    styleResolver->setFontSize(fontDescription, size);
    state.setFontDescription(fontDescription);
    return;
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontWeight(StyleResolver*, StyleResolverState& state)
{
    FontDescription fontDescription = state.fontDescription();
    fontDescription.setWeight(FontWeightNormal);
    state.setFontDescription(fontDescription);
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontWeight(StyleResolver*, StyleResolverState& state)
{
    FontDescription fontDescription = state.fontDescription();
    fontDescription.setWeight(state.parentFontDescription().weight());
    state.setFontDescription(fontDescription);
}

void StyleBuilderFunctions::applyValueCSSPropertyFontWeight(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    FontDescription fontDescription = state.fontDescription();
    switch (primitiveValue->getValueID()) {
    case CSSValueInvalid:
        ASSERT_NOT_REACHED();
        break;
    case CSSValueBolder:
        fontDescription.setWeight(fontDescription.bolderWeight());
        break;
    case CSSValueLighter:
        fontDescription.setWeight(fontDescription.lighterWeight());
        break;
    default:
        fontDescription.setWeight(*primitiveValue);
    }
    state.setFontDescription(fontDescription);
}

void StyleBuilderFunctions::applyValueCSSPropertyLineHeight(StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length lineHeight;

    if (primitiveValue->getValueID() == CSSValueNormal) {
        lineHeight = RenderStyle::initialLineHeight();
    } else if (primitiveValue->isLength()) {
        double multiplier = state.style()->effectiveZoom();
        if (Frame* frame = styleResolver->document()->frame())
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

void StyleBuilderFunctions::applyValueCSSPropertyListStyleImage(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    state.style()->setListStyleImage(state.styleImage(CSSPropertyListStyleImage, value));
}

void StyleBuilderFunctions::applyInitialCSSPropertyOutlineStyle(StyleResolver*, StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(RenderStyle::initialOutlineStyleIsAuto());
    state.style()->setOutlineStyle(RenderStyle::initialBorderStyle());
}

void StyleBuilderFunctions::applyInheritCSSPropertyOutlineStyle(StyleResolver*, StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(state.parentStyle()->outlineStyleIsAuto());
    state.style()->setOutlineStyle(state.parentStyle()->outlineStyle());
}

void StyleBuilderFunctions::applyValueCSSPropertyOutlineStyle(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    state.style()->setOutlineStyleIsAuto(*primitiveValue);
    state.style()->setOutlineStyle(*primitiveValue);
}

void StyleBuilderFunctions::applyValueCSSPropertyResize(StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    EResize r = RESIZE_NONE;
    switch (primitiveValue->getValueID()) {
    case 0:
        return;
    case CSSValueAuto:
        if (Settings* settings = styleResolver->document()->settings())
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

void StyleBuilderFunctions::applyInitialCSSPropertySize(StyleResolver*, StyleResolverState&) { }
void StyleBuilderFunctions::applyInheritCSSPropertySize(StyleResolver*, StyleResolverState&) { }
void StyleBuilderFunctions::applyValueCSSPropertySize(StyleResolver*, StyleResolverState& state, CSSValue* value)
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

void StyleBuilderFunctions::applyValueCSSPropertyTextAlign(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID() != CSSValueWebkitMatchParent)
        state.style()->setTextAlign(*primitiveValue);
    else if (state.parentStyle()->textAlign() == TASTART)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? LEFT : RIGHT);
    else if (state.parentStyle()->textAlign() == TAEND)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? RIGHT : LEFT);
    else
        state.style()->setTextAlign(state.parentStyle()->textAlign());
}

void StyleBuilderFunctions::applyValueCSSPropertyTextDecoration(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    TextDecoration t = RenderStyle::initialTextDecoration();
    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        t |= *toCSSPrimitiveValue(item);
    }
    state.style()->setTextDecoration(t);
}

void StyleBuilderFunctions::applyInheritCSSPropertyTextIndent(StyleResolver*, StyleResolverState& state)
{
    state.style()->setTextIndent(state.parentStyle()->textIndent());
#if ENABLE(CSS3_TEXT)
    state.style()->setTextIndentLine(state.parentStyle()->textIndentLine());
#endif
}

void StyleBuilderFunctions::applyInitialCSSPropertyTextIndent(StyleResolver*, StyleResolverState& state)
{
    state.style()->setTextIndent(RenderStyle::initialTextIndent());
#if ENABLE(CSS3_TEXT)
    state.style()->setTextIndentLine(RenderStyle::initialTextIndentLine());
#endif
}

void StyleBuilderFunctions::applyValueCSSPropertyTextIndent(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    if (!value->isValueList())
        return;

    // [ <length> | <percentage> ] -webkit-each-line
    // The order is guaranteed. See CSSParser::parseTextIndent.
    // The second value, -webkit-each-line is handled only when CSS3_TEXT is enabled.

    CSSValueList* valueList = toCSSValueList(value);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(valueList->itemWithoutBoundsCheck(0));
    Length lengthOrPercentageValue = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | ViewportPercentageConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    ASSERT(!lengthOrPercentageValue.isUndefined());
    state.style()->setTextIndent(lengthOrPercentageValue);

#if ENABLE(CSS3_TEXT)
    ASSERT(valueList->length() <= 2);
    CSSPrimitiveValue* eachLineValue = toCSSPrimitiveValue(valueList->item(1));
    if (eachLineValue) {
        ASSERT(eachLineValue->getValueID() == CSSValueWebkitEachLine);
        state.style()->setTextIndentLine(TextIndentEachLine);
    } else {
        state.style()->setTextIndentLine(TextIndentFirstLine);
    }
#endif
}

void StyleBuilderFunctions::applyValueCSSPropertyVerticalAlign(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID())
        return state.style()->setVerticalAlign(*primitiveValue);

    state.style()->setVerticalAlignLength(primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | ViewportPercentageConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom()));
}

static void resetEffectiveZoom(StyleResolverState& state)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    state.setEffectiveZoom(state.parentStyle() ? state.parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyZoom(StyleResolver*, StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyZoom(StyleResolver*, StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(state.parentStyle()->zoom());
}

void StyleBuilderFunctions::applyValueCSSPropertyZoom(StyleResolver*, StyleResolverState& state, CSSValue* value)
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

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitAspectRatio(StyleResolver*, StyleResolverState& state)
{
    state.style()->setHasAspectRatio(RenderStyle::initialHasAspectRatio());
    state.style()->setAspectRatioDenominator(RenderStyle::initialAspectRatioDenominator());
    state.style()->setAspectRatioNumerator(RenderStyle::initialAspectRatioNumerator());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitAspectRatio(StyleResolver*, StyleResolverState& state)
{
    if (!state.parentStyle()->hasAspectRatio())
        return;
    state.style()->setHasAspectRatio(true);
    state.style()->setAspectRatioDenominator(state.parentStyle()->aspectRatioDenominator());
    state.style()->setAspectRatioNumerator(state.parentStyle()->aspectRatioNumerator());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitAspectRatio(StyleResolver*, StyleResolverState& state, CSSValue* value)
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

void StyleBuilderFunctions::applyValueCSSPropertyWebkitClipPath(StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setClipPath(0);
        } else if (primitiveValue->isShape()) {
            state.style()->setClipPath(ShapeClipPathOperation::create(basicShapeForValue(state, primitiveValue->getShapeValue())));
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue->getStringValue();
            KURL url = styleResolver->document()->completeURL(cssURLValue);
            // FIXME: It doesn't work with forward or external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=90405)
            state.style()->setClipPath(ReferenceClipPathOperation::create(cssURLValue, url.fragmentIdentifier()));
        }
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitFontVariantLigatures(StyleResolver*, StyleResolverState& state)
{
    FontDescription fontDescription = state.fontDescription();

    fontDescription.setCommonLigaturesState(FontDescription::NormalLigaturesState);
    fontDescription.setDiscretionaryLigaturesState(FontDescription::NormalLigaturesState);
    fontDescription.setHistoricalLigaturesState(FontDescription::NormalLigaturesState);

    state.setFontDescription(fontDescription);
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitFontVariantLigatures(StyleResolver*, StyleResolverState& state)
{
    const FontDescription& parentFontDescription = state.parentFontDescription();
    FontDescription fontDescription = state.fontDescription();

    fontDescription.setCommonLigaturesState(parentFontDescription.commonLigaturesState());
    fontDescription.setDiscretionaryLigaturesState(parentFontDescription.discretionaryLigaturesState());
    fontDescription.setHistoricalLigaturesState(parentFontDescription.historicalLigaturesState());

    state.setFontDescription(fontDescription);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitFontVariantLigatures(StyleResolver*, StyleResolverState& state, CSSValue* value)
{
    FontDescription::LigaturesState commonLigaturesState = FontDescription::NormalLigaturesState;
    FontDescription::LigaturesState discretionaryLigaturesState = FontDescription::NormalLigaturesState;
    FontDescription::LigaturesState historicalLigaturesState = FontDescription::NormalLigaturesState;

    if (value->isValueList()) {
        CSSValueList* valueList = toCSSValueList(value);
        for (size_t i = 0; i < valueList->length(); ++i) {
            CSSValue* item = valueList->itemWithoutBoundsCheck(i);
            ASSERT(item->isPrimitiveValue());
            if (item->isPrimitiveValue()) {
                CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
                switch (primitiveValue->getValueID()) {
                case CSSValueNoCommonLigatures:
                    commonLigaturesState = FontDescription::DisabledLigaturesState;
                    break;
                case CSSValueCommonLigatures:
                    commonLigaturesState = FontDescription::EnabledLigaturesState;
                    break;
                case CSSValueNoDiscretionaryLigatures:
                    discretionaryLigaturesState = FontDescription::DisabledLigaturesState;
                    break;
                case CSSValueDiscretionaryLigatures:
                    discretionaryLigaturesState = FontDescription::EnabledLigaturesState;
                    break;
                case CSSValueNoHistoricalLigatures:
                    historicalLigaturesState = FontDescription::DisabledLigaturesState;
                    break;
                case CSSValueHistoricalLigatures:
                    historicalLigaturesState = FontDescription::EnabledLigaturesState;
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    break;
                }
            }
        }
    }
#if !ASSERT_DISABLED
    else {
        ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
        ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNormal);
    }
#endif

    FontDescription fontDescription = state.fontDescription();
    fontDescription.setCommonLigaturesState(commonLigaturesState);
    fontDescription.setDiscretionaryLigaturesState(discretionaryLigaturesState);
    fontDescription.setHistoricalLigaturesState(historicalLigaturesState);
    state.setFontDescription(fontDescription);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitMarqueeIncrement(StyleResolver*, StyleResolverState& state, CSSValue* value)
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
        Length marqueeLength = primitiveValue ? primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | FractionConversion | ViewportPercentageConversion>(state.style(), state.rootElementStyle()) : Length(Undefined);
        if (!marqueeLength.isUndefined())
            state.style()->setMarqueeIncrement(marqueeLength);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitMarqueeSpeed(StyleResolver*, StyleResolverState& state, CSSValue* value)
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
void StyleBuilderFunctions::applyInitialCSSPropertyWebkitPerspectiveOrigin(StyleResolver* styleResolver, StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitPerspectiveOriginX(styleResolver, state);
    applyInitialCSSPropertyWebkitPerspectiveOriginY(styleResolver, state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitPerspectiveOrigin(StyleResolver* styleResolver, StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitPerspectiveOriginX(styleResolver, state);
    applyInheritCSSPropertyWebkitPerspectiveOriginY(styleResolver, state);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitPerspectiveOrigin(StyleResolver*, StyleResolverState&, CSSValue* value)
{
    // This is expanded in the parser
    ASSERT_NOT_REACHED();
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitTextEmphasisStyle(StyleResolver*, StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
    state.style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
    state.style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitTextEmphasisStyle(StyleResolver*, StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(state.parentStyle()->textEmphasisFill());
    state.style()->setTextEmphasisMark(state.parentStyle()->textEmphasisMark());
    state.style()->setTextEmphasisCustomMark(state.parentStyle()->textEmphasisCustomMark());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextEmphasisStyle(StyleResolver*, StyleResolverState& state, CSSValue* value)
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
void StyleBuilderFunctions::applyValueCSSPropetyWebkitTextUnderlinePosition(StyleResolver*, StyleResolverState& state, CSSValue* value)
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

float getComputedSizeFromSpecifiedSize(const Document* document, const RenderStyle* style, bool isAbsoluteSize, float specifiedSize, bool useSVGZoomRules)
{
    float zoomFactor = 1.0f;
    if (!useSVGZoomRules) {
        zoomFactor = style->effectiveZoom();
        if (Frame* frame = document->frame())
            zoomFactor *= frame->textZoomFactor();
    }

    return FontSize::getComputedSizeFromSpecifiedSize(document, zoomFactor, isAbsoluteSize, specifiedSize);
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
        workingLength.setFlex(primitiveValue->getFloatValue());
        return true;
    }

    workingLength = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | ViewportPercentageConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    if (workingLength.length().isUndefined())
        return false;

    if (primitiveValue->isLength())
        workingLength.length().setQuirk(primitiveValue->isQuirkValue());

    return true;
}

static bool createGridTrackSize(CSSValue* value, GridTrackSize& trackSize, const StyleResolverState& state)
{
    if (!value->isPrimitiveValue())
        return false;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* minMaxTrackBreadth = primitiveValue->getPairValue();
    if (!minMaxTrackBreadth) {
        GridLength workingLength;
        if (!createGridTrackBreadth(primitiveValue, state, workingLength))
            return false;

        trackSize.setLength(workingLength);
        return true;
    }

    GridLength minTrackBreadth;
    GridLength maxTrackBreadth;
    if (!createGridTrackBreadth(minMaxTrackBreadth->first(), state, minTrackBreadth) || !createGridTrackBreadth(minMaxTrackBreadth->second(), state, maxTrackBreadth))
        return false;

    trackSize.setMinMax(minTrackBreadth, maxTrackBreadth);
    return true;
}

static bool createGridTrackList(CSSValue* value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, const StyleResolverState& state)
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
                NamedGridLinesMap::AddResult result = namedGridLines.add(primitiveValue->getStringValue(), Vector<size_t>());
                result.iterator->value.append(currentNamedGridLine);
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
    // For now, we only accept: 'auto' | [ <integer> || <string> ] | span && <integer>?

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
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

static String fragmentIdentifier(const CSSPrimitiveValue* primitiveValue, Document* document)
{
    if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_URI)
        return String();
    return SVGURIReference::fragmentIdentifierFromIRIString(primitiveValue->getStringValue(), document);
}


// FIXME: Every use of "styleResolver" in this function is a layering
// violation and should be removed.
void StyleBuilder::oldApplyProperty(CSSPropertyID id, StyleResolver* styleResolver, StyleResolverState& state, CSSValue* value, bool isInitial, bool isInherit)
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
                        state.style()->setContent(StyleGeneratedImage::create(static_cast<CSSGradientValue*>(item)->gradientWithStylesResolved(state.document()->textLinkColors(), state.style()->color()).get()), didSet);
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
                    styleResolver->m_features.attrsInRules.add(attr.localName().impl());
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
        if (isInherit) {
            FontDescription fontDescription = state.parentStyle()->fontDescription();
            state.style()->setLineHeight(state.parentStyle()->specifiedLineHeight());
            state.setLineHeightValue(0);
            state.setFontDescription(fontDescription);
        } else if (isInitial) {
            const Settings* settings = state.document()->settings();
            ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
            if (!settings)
                return;
            styleResolver->initializeFontStyle(settings);
        } else if (primitiveValue) {
            state.style()->setLineHeight(RenderStyle::initialLineHeight());
            state.setLineHeightValue(0);

            FontDescription fontDescription;
            RenderTheme::defaultTheme()->systemFont(primitiveValue->getValueID(), fontDescription);

            // Double-check and see if the theme did anything. If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
                // Make sure the rendering mode and printer font settings are updated.
                const Document* document = state.document();
                const Settings* settings = document->settings();
                ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
                if (!settings)
                    return;
                fontDescription.setUsePrinterFont(document->printing());

                // Handle the zoom factor.
                fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(document, state.style(), fontDescription.isAbsoluteSize(), fontDescription.specifiedSize(), state.useSVGZoomRules()));
                state.setFontDescription(fontDescription);
            }
        } else if (value->isFontValue()) {
            FontValue* font = static_cast<FontValue*>(value);
            if (!font->style || !font->variant || !font->weight
                || !font->size || !font->lineHeight || !font->family)
                return;
            styleResolver->applyProperty(CSSPropertyFontStyle, font->style.get());
            styleResolver->applyProperty(CSSPropertyFontVariant, font->variant.get());
            styleResolver->applyProperty(CSSPropertyFontWeight, font->weight.get());
            // The previous properties can dirty our font but they don't try to read the font's
            // properties back, which is safe. However if font-size is using the 'ex' unit, it will
            // need query the dirtied font's x-height to get the computed size. To be safe in this
            // case, let's just update the font now.
            styleResolver->updateFont();
            styleResolver->applyProperty(CSSPropertyFontSize, font->size.get());

            state.setLineHeightValue(font->lineHeight.get());

            styleResolver->applyProperty(CSSPropertyFontFamily, font->family.get());
        }
        return;

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
    case CSSPropertyWebkitMarquee:
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
                color = state.document()->textLinkColors().colorFromPrimitiveValue(item->color.get(), state.style()->visitedDependentColor(CSSPropertyColor));
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
            reflection->setOffset(reflectValue->offset()->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(state.style(), state.rootElementStyle(), zoomFactor));
        NinePieceImage mask;
        mask.setMaskDefaults();
        state.styleMap().mapNinePieceImage(id, reflectValue->mask(), mask);
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
        FontDescription fontDescription = state.style()->fontDescription();
        fontDescription.setScript(localeToScriptCodeForFontSelection(state.style()->locale()));
        state.setFontDescription(fontDescription);
        return;
    }
    case CSSPropertyWebkitAppRegion: {
        if (!primitiveValue || !primitiveValue->getValueID())
            return;
        state.style()->setDraggableRegionMode(primitiveValue->getValueID() == CSSValueDrag ? DraggableRegionDrag : DraggableRegionNoDrag);
        state.document()->setHasAnnotatedRegions(true);
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

        Color col = state.document()->textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->visitedDependentColor(CSSPropertyColor));
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
        return styleResolver->applyProperty(newId, value);
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
        if (state.element() && state.element() == state.document()->documentElement())
            state.document()->setWritingModeSetOnDocumentElement(true);
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
            state.setFontDescription(state.style()->fontDescription().makeNormalFeatureSettings());
            return;
        }

        if (!value->isValueList())
            return;

        FontDescription fontDescription = state.style()->fontDescription();
        CSSValueList* list = toCSSValueList(value);
        RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
        int len = list->length();
        for (int i = 0; i < len; ++i) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (!item->isFontFeatureValue())
                continue;
            FontFeatureValue* feature = static_cast<FontFeatureValue*>(item);
            settings->append(FontFeature(feature->tag(), feature->value()));
        }
        fontDescription.setFeatureSettings(settings.release());
        state.setFontDescription(fontDescription);
        return;
    }

    case CSSPropertyWebkitFilter: {
        HANDLE_INHERIT_AND_INITIAL(filter, Filter);
        FilterOperations operations;
        if (FilterOperationResolver::createFilterOperations(value, state.style(), state.rootElementStyle(), operations, styleResolver->m_styleResourceLoader.customFilterProgramCache(), state))
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
            return;
        }
        if (isInitial) {
            state.style()->setGridDefinitionColumns(RenderStyle::initialGridDefinitionColumns());
            state.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, state))
            return;
        state.style()->setGridDefinitionColumns(trackSizes);
        state.style()->setNamedGridColumnLines(namedGridLines);
        return;
    }
    case CSSPropertyGridDefinitionRows: {
        if (isInherit) {
            state.style()->setGridDefinitionRows(state.parentStyle()->gridDefinitionRows());
            state.style()->setNamedGridRowLines(state.parentStyle()->namedGridRowLines());
            return;
        }
        if (isInitial) {
            state.style()->setGridDefinitionRows(RenderStyle::initialGridDefinitionRows());
            state.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, state))
            return;
        state.style()->setGridDefinitionRows(trackSizes);
        state.style()->setNamedGridRowLines(namedGridLines);
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

    // These properties are aliased and we already applied the property on the prefixed version.
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
    case CSSPropertyWebkitHyphenateLimitAfter:
    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitLines:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineClamp:
    case CSSPropertyWebkitLineGrid:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeIncrement:
    case CSSPropertyWebkitMarqueeRepetition:
    case CSSPropertyWebkitMarqueeSpeed:
    case CSSPropertyWebkitMarqueeStyle:
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
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
    case CSSPropertyFontFamily:
    case CSSPropertyGridAutoFlow:
    case CSSPropertyMarker:
        ASSERT_NOT_REACHED();
        return;

    case CSSPropertyAlignmentBaseline:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(alignmentBaseline, AlignmentBaseline)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setAlignmentBaseline(*primitiveValue);
        break;
    }
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
    case CSSPropertyDominantBaseline:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(dominantBaseline, DominantBaseline)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setDominantBaseline(*primitiveValue);
        break;
    }
    case CSSPropertyColorInterpolation:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(colorInterpolation, ColorInterpolation)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setColorInterpolation(*primitiveValue);
        break;
    }
    case CSSPropertyColorInterpolationFilters:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(colorInterpolationFilters, ColorInterpolationFilters)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setColorInterpolationFilters(*primitiveValue);
        break;
    }
    case CSSPropertyColorProfile:
    {
        // Not implemented.
        break;
    }
    case CSSPropertyColorRendering:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(colorRendering, ColorRendering)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setColorRendering(*primitiveValue);
        break;
    }
    case CSSPropertyClipRule:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(clipRule, ClipRule)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setClipRule(*primitiveValue);
        break;
    }
    case CSSPropertyFillRule:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(fillRule, FillRule)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setFillRule(*primitiveValue);
        break;
    }
    case CSSPropertyStrokeLinejoin:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(joinStyle, JoinStyle)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setJoinStyle(*primitiveValue);
        break;
    }
    case CSSPropertyShapeRendering:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(shapeRendering, ShapeRendering)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setShapeRendering(*primitiveValue);
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
    case CSSPropertyStrokeLinecap:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(capStyle, CapStyle)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setCapStyle(*primitiveValue);
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
    case CSSPropertyTextAnchor:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(textAnchor, TextAnchor)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setTextAnchor(*primitiveValue);
        break;
    }
    case CSSPropertyWritingMode:
    {
        HANDLE_SVG_INHERIT_AND_INITIAL(writingMode, WritingMode)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setWritingMode(*primitiveValue);
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
    case CSSPropertyWebkitSvgShadow: {
        if (isInherit)
            return state.style()->accessSVGStyle()->setShadow(cloneShadow(state.parentStyle()->svgStyle()->shadow()));
        if (isInitial || primitiveValue) // initial | none
            return state.style()->accessSVGStyle()->setShadow(nullptr);

        if (!value->isValueList())
            return;

        CSSValueList* list = toCSSValueList(value);
        if (!list->length())
            return;

        CSSValue* firstValue = list->itemWithoutBoundsCheck(0);
        if (!firstValue->isShadowValue())
            return;
        ShadowValue* item = static_cast<ShadowValue*>(firstValue);
        IntPoint location(item->x->computeLength<int>(state.style(), state.rootElementStyle()),
            item->y->computeLength<int>(state.style(), state.rootElementStyle()));
        int blur = item->blur ? item->blur->computeLength<int>(state.style(), state.rootElementStyle()) : 0;
        Color color;
        if (item->color)
            color = state.document()->textLinkColors().colorFromPrimitiveValue(item->color.get(), state.style()->visitedDependentColor(CSSPropertyColor));

        // -webkit-svg-shadow does should not have a spread or style
        ASSERT(!item->spread);
        ASSERT(!item->style);

        if (!color.isValid())
            color = Color::transparent;
        state.style()->accessSVGStyle()->setShadow(ShadowData::create(location, blur, 0, Normal, color));
        return;
    }
    case CSSPropertyVectorEffect: {
        HANDLE_SVG_INHERIT_AND_INITIAL(vectorEffect, VectorEffect)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setVectorEffect(*primitiveValue);
        break;
    }
    case CSSPropertyBufferedRendering: {
        HANDLE_SVG_INHERIT_AND_INITIAL(bufferedRendering, BufferedRendering)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setBufferedRendering(*primitiveValue);
        break;
    }
    case CSSPropertyMaskType: {
        HANDLE_SVG_INHERIT_AND_INITIAL(maskType, MaskType)
        if (primitiveValue)
            state.style()->accessSVGStyle()->setMaskType(*primitiveValue);
        break;
    }
    }
}

} // namespace WebCore
