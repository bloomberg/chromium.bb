/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "core/css/ComputedStyleCSSValueMapping.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSValuePool.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleVariableData.h"
#include "platform/LengthFunctions.h"

namespace blink {

inline static bool isFlexOrGrid(const ComputedStyle* style)
{
    return style && style->isDisplayFlexibleOrGridBox();
}

inline static CSSPrimitiveValue* zoomAdjustedPixelValue(double value, const ComputedStyle& style)
{
    return cssValuePool().createValue(adjustFloatForAbsoluteZoom(value, style), CSSPrimitiveValue::UnitType::Pixels);
}

inline static CSSPrimitiveValue* zoomAdjustedNumberValue(double value, const ComputedStyle& style)
{
    return cssValuePool().createValue(value / style.effectiveZoom(), CSSPrimitiveValue::UnitType::Number);
}

static CSSPrimitiveValue* zoomAdjustedPixelValueForLength(const Length& length, const ComputedStyle& style)
{
    if (length.isFixed())
        return zoomAdjustedPixelValue(length.value(), style);
    return cssValuePool().createValue(length, style);
}

static CSSPrimitiveValue* pixelValueForUnzoomedLength(const UnzoomedLength& unzoomedLength, const ComputedStyle& style)
{
    const Length& length = unzoomedLength.length();
    if (length.isFixed())
        return cssValuePool().createValue(length.value(), CSSPrimitiveValue::UnitType::Pixels);
    return cssValuePool().createValue(length, style);
}

static CSSValueList* createPositionListForLayer(CSSPropertyID propertyID, const FillLayer& layer, const ComputedStyle& style)
{
    CSSValueList* positionList = CSSValueList::createSpaceSeparated();
    if (layer.isBackgroundXOriginSet()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList->append(cssValuePool().createValue(layer.backgroundXOrigin()));
    }
    positionList->append(zoomAdjustedPixelValueForLength(layer.xPosition(), style));
    if (layer.isBackgroundYOriginSet()) {
        ASSERT(propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList->append(cssValuePool().createValue(layer.backgroundYOrigin()));
    }
    positionList->append(zoomAdjustedPixelValueForLength(layer.yPosition(), style));
    return positionList;
}

CSSValue* ComputedStyleCSSValueMapping::currentColorOrValidColor(const ComputedStyle& style, const StyleColor& color)
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    return cssValuePool().createColorValue(color.resolve(style.color()).rgb());
}

static CSSValue* valueForFillSize(const FillSize& fillSize, const ComputedStyle& style)
{
    if (fillSize.type == Contain)
        return cssValuePool().createIdentifierValue(CSSValueContain);

    if (fillSize.type == Cover)
        return cssValuePool().createIdentifierValue(CSSValueCover);

    if (fillSize.size.height().isAuto())
        return zoomAdjustedPixelValueForLength(fillSize.size.width(), style);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.width(), style));
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.height(), style));
    return list;
}

static CSSValue* valueForFillRepeat(EFillRepeat xRepeat, EFillRepeat yRepeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    if (xRepeat == yRepeat)
        return cssValuePool().createValue(xRepeat);
    if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
        return cssValuePool().createIdentifierValue(CSSValueRepeatX);
    if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
        return cssValuePool().createIdentifierValue(CSSValueRepeatY);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(cssValuePool().createValue(xRepeat));
    list->append(cssValuePool().createValue(yRepeat));
    return list;
}

static CSSValue* valueForFillSourceType(EMaskSourceType type)
{
    switch (type) {
    case MaskAlpha:
        return cssValuePool().createIdentifierValue(CSSValueAlpha);
    case MaskLuminance:
        return cssValuePool().createIdentifierValue(CSSValueLuminance);
    }

    ASSERT_NOT_REACHED();

    return nullptr;
}

static CSSValue* valueForPositionOffset(const ComputedStyle& style, CSSPropertyID propertyID, const LayoutObject* layoutObject)
{
    Length offset, opposite;
    switch (propertyID) {
    case CSSPropertyLeft:
        offset = style.left();
        opposite = style.right();
        break;
    case CSSPropertyRight:
        offset = style.right();
        opposite = style.left();
        break;
    case CSSPropertyTop:
        offset = style.top();
        opposite = style.bottom();
        break;
    case CSSPropertyBottom:
        offset = style.bottom();
        opposite = style.top();
        break;
    default:
        return nullptr;
    }

    if (offset.hasPercent() && layoutObject && layoutObject->isBox() && layoutObject->isPositioned()) {
        LayoutUnit containingBlockSize = (propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight) ?
            toLayoutBox(layoutObject)->containingBlockLogicalWidthForContent() :
            toLayoutBox(layoutObject)->containingBlockLogicalHeightForGetComputedStyle();
        return zoomAdjustedPixelValue(valueForLength(offset, containingBlockSize), style);
    }

    if (offset.isAuto() && layoutObject) {
        // If the property applies to a positioned element and the resolved value of the display
        // property is not none, the resolved value is the used value.
        if (layoutObject->isInFlowPositioned()) {
            // If e.g. left is auto and right is not auto, then left's computed value is negative right.
            // So we get the opposite length unit and see if it is auto.
            if (opposite.isAuto())
                return cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Pixels);

            if (opposite.hasPercent()) {
                LayoutUnit containingBlockSize =
                    (propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight) ?
                    toLayoutBox(layoutObject)->containingBlockLogicalWidthForContent() :
                    toLayoutBox(layoutObject)->containingBlockLogicalHeightForGetComputedStyle();
                return zoomAdjustedPixelValue(-floatValueForLength(opposite, containingBlockSize), style);
            }
            return zoomAdjustedPixelValue(-opposite.pixels(), style);
        }

        if (layoutObject->isOutOfFlowPositioned()) {
            // For fixed and absolute positioned elements, the top, left, bottom, and right
            // are defined relative to the corresponding sides of the containing block.
            LayoutBlock* container = layoutObject->containingBlock();
            const LayoutBox* layoutBox = toLayoutBox(layoutObject);

            // clientOffset is the distance from this object's border edge to the container's
            // padding edge. Thus it includes margins which we subtract below.
            const LayoutSize clientOffset =
                layoutBox->locationOffset() - LayoutSize(container->clientLeft(), container->clientTop());
            LayoutUnit position;

            switch (propertyID) {
            case CSSPropertyLeft:
                position = clientOffset.width() - layoutBox->marginLeft();
                break;
            case CSSPropertyTop:
                position = clientOffset.height() - layoutBox->marginTop();
                break;
            case CSSPropertyRight:
                position = container->clientWidth() - layoutBox->marginRight() -
                    (layoutBox->offsetWidth() + clientOffset.width());
                break;
            case CSSPropertyBottom:
                position = container->clientHeight() - layoutBox->marginBottom() -
                    (layoutBox->offsetHeight() + clientOffset.height());
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            return zoomAdjustedPixelValue(position, style);
        }
    }

    if (offset.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);

    return zoomAdjustedPixelValueForLength(offset, style);
}

static CSSBorderImageSliceValue* valueForNinePieceImageSlice(const NinePieceImage& image)
{
    // Create the slices.
    CSSPrimitiveValue* top = nullptr;
    CSSPrimitiveValue* right = nullptr;
    CSSPrimitiveValue* bottom = nullptr;
    CSSPrimitiveValue* left = nullptr;

    // TODO(alancutter): Make this code aware of calc lengths.
    if (image.imageSlices().top().hasPercent())
        top = cssValuePool().createValue(image.imageSlices().top().value(), CSSPrimitiveValue::UnitType::Percentage);
    else
        top = cssValuePool().createValue(image.imageSlices().top().value(), CSSPrimitiveValue::UnitType::Number);

    if (image.imageSlices().right() == image.imageSlices().top() && image.imageSlices().bottom() == image.imageSlices().top()
        && image.imageSlices().left() == image.imageSlices().top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (image.imageSlices().right().hasPercent())
            right = cssValuePool().createValue(image.imageSlices().right().value(), CSSPrimitiveValue::UnitType::Percentage);
        else
            right = cssValuePool().createValue(image.imageSlices().right().value(), CSSPrimitiveValue::UnitType::Number);

        if (image.imageSlices().bottom() == image.imageSlices().top() && image.imageSlices().right() == image.imageSlices().left()) {
            bottom = top;
            left = right;
        } else {
            if (image.imageSlices().bottom().hasPercent())
                bottom = cssValuePool().createValue(image.imageSlices().bottom().value(), CSSPrimitiveValue::UnitType::Percentage);
            else
                bottom = cssValuePool().createValue(image.imageSlices().bottom().value(), CSSPrimitiveValue::UnitType::Number);

            if (image.imageSlices().left() == image.imageSlices().right()) {
                left = right;
            } else {
                if (image.imageSlices().left().hasPercent())
                    left = cssValuePool().createValue(image.imageSlices().left().value(), CSSPrimitiveValue::UnitType::Percentage);
                else
                    left = cssValuePool().createValue(image.imageSlices().left().value(), CSSPrimitiveValue::UnitType::Number);
            }
        }
    }

    return CSSBorderImageSliceValue::create(CSSQuadValue::create(top, right, bottom, left, CSSQuadValue::SerializeAsQuad), image.fill());
}

static CSSQuadValue* valueForNinePieceImageQuad(const BorderImageLengthBox& box, const ComputedStyle& style)
{
    // Create the slices.
    CSSPrimitiveValue* top = nullptr;
    CSSPrimitiveValue* right = nullptr;
    CSSPrimitiveValue* bottom = nullptr;
    CSSPrimitiveValue* left = nullptr;

    if (box.top().isNumber())
        top = cssValuePool().createValue(box.top().number(), CSSPrimitiveValue::UnitType::Number);
    else
        top = cssValuePool().createValue(box.top().length(), style);

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isNumber())
            right = cssValuePool().createValue(box.right().number(), CSSPrimitiveValue::UnitType::Number);
        else
            right = cssValuePool().createValue(box.right().length(), style);

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isNumber())
                bottom = cssValuePool().createValue(box.bottom().number(), CSSPrimitiveValue::UnitType::Number);
            else
                bottom = cssValuePool().createValue(box.bottom().length(), style);

            if (box.left() == box.right()) {
                left = right;
            } else {
                if (box.left().isNumber())
                    left = cssValuePool().createValue(box.left().number(), CSSPrimitiveValue::UnitType::Number);
                else
                    left = cssValuePool().createValue(box.left().length(), style);
            }
        }
    }

    return CSSQuadValue::create(top, right, bottom, left, CSSQuadValue::SerializeAsQuad);
}

static CSSValueID valueForRepeatRule(int rule)
{
    switch (rule) {
    case RepeatImageRule:
        return CSSValueRepeat;
    case RoundImageRule:
        return CSSValueRound;
    case SpaceImageRule:
        return CSSValueSpace;
    default:
        return CSSValueStretch;
    }
}

static CSSValue* valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    CSSPrimitiveValue* horizontalRepeat = nullptr;
    CSSPrimitiveValue* verticalRepeat = nullptr;

    horizontalRepeat = cssValuePool().createIdentifierValue(valueForRepeatRule(image.horizontalRule()));
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat;
    else
        verticalRepeat = cssValuePool().createIdentifierValue(valueForRepeatRule(image.verticalRule()));
    return CSSValuePair::create(horizontalRepeat, verticalRepeat, CSSValuePair::DropIdenticalValues);
}

static CSSValue* valueForNinePieceImage(const NinePieceImage& image, const ComputedStyle& style)
{
    if (!image.hasImage())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    // Image first.
    CSSValue* imageValue = nullptr;
    if (image.image())
        imageValue = image.image()->computedCSSValue();

    // Create the image slice.
    CSSBorderImageSliceValue* imageSlices = valueForNinePieceImageSlice(image);

    // Create the border area slices.
    CSSValue* borderSlices = valueForNinePieceImageQuad(image.borderSlices(), style);

    // Create the border outset.
    CSSValue* outset = valueForNinePieceImageQuad(image.outset(), style);

    // Create the repeat rules.
    CSSValue* repeat = valueForNinePieceImageRepeat(image);

    return createBorderImageValue(imageValue, imageSlices, borderSlices, outset, repeat);
}

static CSSValue* valueForReflection(const StyleReflection* reflection, const ComputedStyle& style)
{
    if (!reflection)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    CSSPrimitiveValue* offset = nullptr;
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (reflection->offset().hasPercent())
        offset = cssValuePool().createValue(reflection->offset().percent(), CSSPrimitiveValue::UnitType::Percentage);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

    CSSPrimitiveValue* direction = nullptr;
    switch (reflection->direction()) {
    case ReflectionBelow:
        direction = cssValuePool().createIdentifierValue(CSSValueBelow);
        break;
    case ReflectionAbove:
        direction = cssValuePool().createIdentifierValue(CSSValueAbove);
        break;
    case ReflectionLeft:
        direction = cssValuePool().createIdentifierValue(CSSValueLeft);
        break;
    case ReflectionRight:
        direction = cssValuePool().createIdentifierValue(CSSValueRight);
        break;
    }

    return CSSReflectValue::create(direction, offset, valueForNinePieceImage(reflection->mask(), style));
}

static ItemPosition resolveAlignmentAuto(ItemPosition position, const ComputedStyle* style)
{
    if (position != ItemPositionAuto)
        return position;

    if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
        return ItemPositionStretch;

    return isFlexOrGrid(style) ? ItemPositionStretch : ItemPositionStart;
}

static CSSValueList* valueForItemPositionWithOverflowAlignment(ItemPosition itemPosition, OverflowAlignment overflowAlignment, ItemPositionType positionType)
{
    CSSValueList* result = CSSValueList::createSpaceSeparated();
    if (positionType == LegacyPosition)
        result->append(CSSPrimitiveValue::createIdentifier(CSSValueLegacy));
    result->append(CSSPrimitiveValue::create(itemPosition));
    if (itemPosition >= ItemPositionCenter && overflowAlignment != OverflowAlignmentDefault)
        result->append(CSSPrimitiveValue::create(overflowAlignment));
    ASSERT(result->length() <= 2);
    return result;
}

static CSSValueList* valuesForGridShorthand(const StylePropertyShorthand& shorthand, const ComputedStyle& style, const LayoutObject* layoutObject, Node* styledNode, bool allowVisitedStyle)
{
    CSSValueList* list = CSSValueList::createSlashSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        CSSValue* value = ComputedStyleCSSValueMapping::get(shorthand.properties()[i], style, layoutObject, styledNode, allowVisitedStyle);
        ASSERT(value);
        list->append(value);
    }
    return list;
}

static CSSValueList* valuesForShorthandProperty(const StylePropertyShorthand& shorthand, const ComputedStyle& style, const LayoutObject* layoutObject, Node* styledNode, bool allowVisitedStyle)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        CSSValue* value = ComputedStyleCSSValueMapping::get(shorthand.properties()[i], style, layoutObject, styledNode, allowVisitedStyle);
        ASSERT(value);
        list->append(value);
    }
    return list;
}

static CSSValueList* valuesForBackgroundShorthand(const ComputedStyle& style, const LayoutObject* layoutObject, Node* styledNode, bool allowVisitedStyle)
{
    CSSValueList* ret = CSSValueList::createCommaSeparated();
    const FillLayer* currLayer = &style.backgroundLayers();
    for (; currLayer; currLayer = currLayer->next()) {
        CSSValueList* list = CSSValueList::createSlashSeparated();
        CSSValueList* beforeSlash = CSSValueList::createSpaceSeparated();
        if (!currLayer->next()) { // color only for final layer
            CSSValue* value = ComputedStyleCSSValueMapping::get(CSSPropertyBackgroundColor, style, layoutObject, styledNode, allowVisitedStyle);
            ASSERT(value);
            beforeSlash->append(value);
        }
        beforeSlash->append(currLayer->image() ? currLayer->image()->computedCSSValue() : cssValuePool().createIdentifierValue(CSSValueNone));
        beforeSlash->append(valueForFillRepeat(currLayer->repeatX(), currLayer->repeatY()));
        beforeSlash->append(cssValuePool().createValue(currLayer->attachment()));
        beforeSlash->append(createPositionListForLayer(CSSPropertyBackgroundPosition, *currLayer, style));
        list->append(beforeSlash);
        CSSValueList* afterSlash = CSSValueList::createSpaceSeparated();
        afterSlash->append(valueForFillSize(currLayer->size(), style));
        afterSlash->append(cssValuePool().createValue(currLayer->origin()));
        afterSlash->append(cssValuePool().createValue(currLayer->clip()));
        list->append(afterSlash);
        ret->append(list);
    }
    return ret;
}

static CSSValueList* valueForContentPositionAndDistributionWithOverflowAlignment(const StyleContentAlignmentData& data)
{
    CSSValueList* result = CSSValueList::createSpaceSeparated();
    if (data.distribution() != ContentDistributionDefault)
        result->append(CSSPrimitiveValue::create(data.distribution()));
    if (data.distribution() == ContentDistributionDefault || data.position() != ContentPositionNormal)
        result->append(CSSPrimitiveValue::create(data.position()));
    if ((data.position() >= ContentPositionCenter || data.distribution() != ContentDistributionDefault) && data.overflow() != OverflowAlignmentDefault)
        result->append(CSSPrimitiveValue::create(data.overflow()));
    ASSERT(result->length() > 0);
    ASSERT(result->length() <= 3);
    return result;
}

static CSSPrimitiveValue* valueForLineHeight(const ComputedStyle& style)
{
    Length length = style.lineHeight();
    if (length.isNegative())
        return cssValuePool().createIdentifierValue(CSSValueNormal);

    return zoomAdjustedPixelValue(floatValueForLength(length, style.getFontDescription().computedSize()), style);
}

static CSSValueID identifierForFamily(const AtomicString& family)
{
    if (family == FontFamilyNames::webkit_cursive)
        return CSSValueCursive;
    if (family == FontFamilyNames::webkit_fantasy)
        return CSSValueFantasy;
    if (family == FontFamilyNames::webkit_monospace)
        return CSSValueMonospace;
    if (family == FontFamilyNames::webkit_pictograph)
        return CSSValueWebkitPictograph;
    if (family == FontFamilyNames::webkit_sans_serif)
        return CSSValueSansSerif;
    if (family == FontFamilyNames::webkit_serif)
        return CSSValueSerif;
    return CSSValueInvalid;
}

static CSSValue* valueForFamily(const AtomicString& family)
{
    if (CSSValueID familyIdentifier = identifierForFamily(family))
        return cssValuePool().createIdentifierValue(familyIdentifier);
    return cssValuePool().createFontFamilyValue(family.getString());
}

static CSSValueList* valueForFontFamily(const ComputedStyle& style)
{
    const FontFamily& firstFamily = style.getFontDescription().family();
    CSSValueList* list = CSSValueList::createCommaSeparated();
    for (const FontFamily* family = &firstFamily; family; family = family->next())
        list->append(valueForFamily(family->family()));
    return list;
}

static CSSPrimitiveValue* valueForFontSize(const ComputedStyle& style)
{
    return zoomAdjustedPixelValue(style.getFontDescription().computedSize(), style);
}

static CSSPrimitiveValue* valueForFontStretch(const ComputedStyle& style)
{
    return cssValuePool().createValue(style.getFontDescription().stretch());
}

static CSSPrimitiveValue* valueForFontStyle(const ComputedStyle& style)
{
    return cssValuePool().createValue(style.getFontDescription().style());
}

static CSSPrimitiveValue* valueForFontVariant(const ComputedStyle& style)
{
    return cssValuePool().createValue(style.getFontDescription().variant());
}

static CSSPrimitiveValue* valueForFontWeight(const ComputedStyle& style)
{
    return cssValuePool().createValue(style.getFontDescription().weight());
}

static CSSValue* specifiedValueForGridTrackBreadth(const GridLength& trackBreadth, const ComputedStyle& style)
{
    if (!trackBreadth.isLength())
        return cssValuePool().createValue(trackBreadth.flex(), CSSPrimitiveValue::UnitType::Fraction);

    const Length& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);
    return zoomAdjustedPixelValueForLength(trackBreadthLength, style);
}

static CSSValue* specifiedValueForGridTrackSize(const GridTrackSize& trackSize, const ComputedStyle& style)
{
    switch (trackSize.type()) {
    case LengthTrackSizing:
        return specifiedValueForGridTrackBreadth(trackSize.length(), style);
    case MinMaxTrackSizing:
        CSSFunctionValue* minMaxTrackBreadths = CSSFunctionValue::create(CSSValueMinmax);
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style));
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.maxTrackBreadth(), style));
        return minMaxTrackBreadths;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static void addValuesForNamedGridLinesAtIndex(const OrderedNamedGridLines& orderedNamedGridLines, size_t i, CSSValueList& list)
{
    const Vector<String>& namedGridLines = orderedNamedGridLines.get(i);
    if (namedGridLines.isEmpty())
        return;

    CSSGridLineNamesValue* lineNames = CSSGridLineNamesValue::create();
    for (size_t j = 0; j < namedGridLines.size(); ++j)
        lineNames->append(CSSCustomIdentValue::create(namedGridLines[j]));
    list.append(lineNames);
}

static CSSValue* valueForGridTrackList(GridTrackSizingDirection direction, const LayoutObject* layoutObject, const ComputedStyle& style)
{
    bool isRowAxis = direction == ForColumns;
    const Vector<GridTrackSize>& trackSizes = isRowAxis ? style.gridTemplateColumns() : style.gridTemplateRows();
    const Vector<GridTrackSize>& autoRepeatTrackSizes = isRowAxis ? style.gridAutoRepeatColumns() : style.gridAutoRepeatRows();
    const OrderedNamedGridLines& orderedNamedGridLines = isRowAxis ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines();

    bool isLayoutGrid = layoutObject && layoutObject->isLayoutGrid();

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty() && autoRepeatTrackSizes.isEmpty();
    if (isLayoutGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly created. If we don't have
        // any explicit track and there are no children then there are no implicit tracks. We cannot simply check the
        // number of rows/columns in our internal grid representation because it's always at least 1x1 (see r143331).
        trackListIsEmpty = !toLayoutBlock(layoutObject)->firstChild();
    }

    if (trackListIsEmpty) {
        ASSERT(orderedNamedGridLines.isEmpty());
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    size_t insertionIndex;
    if (isLayoutGrid) {
        const auto* grid = toLayoutGrid(layoutObject);
        const Vector<LayoutUnit>& trackPositions = direction == ForColumns ? grid->columnPositions() : grid->rowPositions();
        // There are at least #tracks + 1 grid lines (trackPositions). Apart from that, the grid container can generate implicit grid tracks,
        // so we'll have more trackPositions than trackSizes as the latter only contain the explicit grid.
        ASSERT(trackPositions.size() - 1 >= trackSizes.size());

        size_t i;
        LayoutUnit gutterSize = grid->guttersSize(direction, 2);
        LayoutUnit offsetBetweenTracks = grid->offsetBetweenTracks(direction);
        for (i = 0; i < trackPositions.size() - 2; ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, *list);
            list->append(zoomAdjustedPixelValue(trackPositions[i + 1] - trackPositions[i] - gutterSize - offsetBetweenTracks, style));
        }
        // Last track line does not have any gutter or distribution offset.
        addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, *list);
        list->append(zoomAdjustedPixelValue(trackPositions[i + 1] - trackPositions[i], style));
        insertionIndex = trackPositions.size() - 1;
    } else {
        for (size_t i = 0; i < trackSizes.size(); ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, *list);
            list->append(specifiedValueForGridTrackSize(trackSizes[i], style));
        }
        insertionIndex = trackSizes.size();
    }
    // Those are the trailing <string>* allowed in the syntax.
    addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, insertionIndex, *list);
    return list;
}

static CSSValue* valueForGridPosition(const GridPosition& position)
{
    if (position.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);

    if (position.isNamedGridArea())
        return CSSCustomIdentValue::create(position.namedGridLine());

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (position.isSpan()) {
        list->append(cssValuePool().createIdentifierValue(CSSValueSpan));
        list->append(cssValuePool().createValue(position.spanPosition(), CSSPrimitiveValue::UnitType::Number));
    } else {
        list->append(cssValuePool().createValue(position.integerPosition(), CSSPrimitiveValue::UnitType::Number));
    }

    if (!position.namedGridLine().isNull())
        list->append(CSSCustomIdentValue::create(position.namedGridLine()));
    return list;
}

static LayoutRect sizingBox(const LayoutObject* layoutObject)
{
    if (!layoutObject->isBox())
        return LayoutRect();

    const LayoutBox* box = toLayoutBox(layoutObject);
    return box->style()->boxSizing() == BoxSizingBorderBox ? box->borderBoxRect() : box->computedCSSContentBoxRect();
}

static CSSValue* renderTextDecorationFlagsToCSSValue(int textDecoration)
{
    // Blink value is ignored.
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (textDecoration & TextDecorationUnderline)
        list->append(cssValuePool().createIdentifierValue(CSSValueUnderline));
    if (textDecoration & TextDecorationOverline)
        list->append(cssValuePool().createIdentifierValue(CSSValueOverline));
    if (textDecoration & TextDecorationLineThrough)
        list->append(cssValuePool().createIdentifierValue(CSSValueLineThrough));

    if (!list->length())
        return cssValuePool().createIdentifierValue(CSSValueNone);
    return list;
}

static CSSValue* valueForTextDecorationStyle(TextDecorationStyle textDecorationStyle)
{
    switch (textDecorationStyle) {
    case TextDecorationStyleSolid:
        return cssValuePool().createIdentifierValue(CSSValueSolid);
    case TextDecorationStyleDouble:
        return cssValuePool().createIdentifierValue(CSSValueDouble);
    case TextDecorationStyleDotted:
        return cssValuePool().createIdentifierValue(CSSValueDotted);
    case TextDecorationStyleDashed:
        return cssValuePool().createIdentifierValue(CSSValueDashed);
    case TextDecorationStyleWavy:
        return cssValuePool().createIdentifierValue(CSSValueWavy);
    }

    ASSERT_NOT_REACHED();
    return cssValuePool().createExplicitInitialValue();
}

static CSSValue* touchActionFlagsToCSSValue(TouchAction touchAction)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (touchAction == TouchActionAuto) {
        list->append(cssValuePool().createIdentifierValue(CSSValueAuto));
    } else if (touchAction == TouchActionNone) {
        list->append(cssValuePool().createIdentifierValue(CSSValueNone));
    } else if (touchAction == TouchActionManipulation) {
        list->append(cssValuePool().createIdentifierValue(CSSValueManipulation));
    } else {
        if ((touchAction & TouchActionPanX) == TouchActionPanX)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanX));
        else if (touchAction & TouchActionPanLeft)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanLeft));
        else if (touchAction & TouchActionPanRight)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanRight));

        if ((touchAction & TouchActionPanY) == TouchActionPanY)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanY));
        else if (touchAction & TouchActionPanUp)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanUp));
        else if (touchAction & TouchActionPanDown)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanDown));
    }
    ASSERT(list->length());
    return list;
}

static CSSValue* valueForWillChange(const Vector<CSSPropertyID>& willChangeProperties, bool willChangeContents, bool willChangeScrollPosition)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    if (willChangeContents)
        list->append(cssValuePool().createIdentifierValue(CSSValueContents));
    if (willChangeScrollPosition)
        list->append(cssValuePool().createIdentifierValue(CSSValueScrollPosition));
    for (size_t i = 0; i < willChangeProperties.size(); ++i)
        list->append(cssValuePool().createIdentifierValue(willChangeProperties[i]));
    if (!list->length())
        list->append(cssValuePool().createIdentifierValue(CSSValueAuto));
    return list;
}

static CSSValue* valueForAnimationDelay(const CSSTimingData* timingData)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    if (timingData) {
        for (size_t i = 0; i < timingData->delayList().size(); ++i)
            list->append(cssValuePool().createValue(timingData->delayList()[i], CSSPrimitiveValue::UnitType::Seconds));
    } else {
        list->append(cssValuePool().createValue(CSSTimingData::initialDelay(), CSSPrimitiveValue::UnitType::Seconds));
    }
    return list;
}

static CSSValue* valueForAnimationDirection(Timing::PlaybackDirection direction)
{
    switch (direction) {
    case Timing::PlaybackDirectionNormal:
        return cssValuePool().createIdentifierValue(CSSValueNormal);
    case Timing::PlaybackDirectionAlternate:
        return cssValuePool().createIdentifierValue(CSSValueAlternate);
    case Timing::PlaybackDirectionReverse:
        return cssValuePool().createIdentifierValue(CSSValueReverse);
    case Timing::PlaybackDirectionAlternateReverse:
        return cssValuePool().createIdentifierValue(CSSValueAlternateReverse);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

static CSSValue* valueForAnimationDuration(const CSSTimingData* timingData)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    if (timingData) {
        for (size_t i = 0; i < timingData->durationList().size(); ++i)
            list->append(cssValuePool().createValue(timingData->durationList()[i], CSSPrimitiveValue::UnitType::Seconds));
    } else {
        list->append(cssValuePool().createValue(CSSTimingData::initialDuration(), CSSPrimitiveValue::UnitType::Seconds));
    }
    return list;
}

static CSSValue* valueForAnimationFillMode(Timing::FillMode fillMode)
{
    switch (fillMode) {
    case Timing::FillModeNone:
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case Timing::FillModeForwards:
        return cssValuePool().createIdentifierValue(CSSValueForwards);
    case Timing::FillModeBackwards:
        return cssValuePool().createIdentifierValue(CSSValueBackwards);
    case Timing::FillModeBoth:
        return cssValuePool().createIdentifierValue(CSSValueBoth);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

static CSSValue* valueForAnimationIterationCount(double iterationCount)
{
    if (iterationCount == std::numeric_limits<double>::infinity())
        return cssValuePool().createIdentifierValue(CSSValueInfinite);
    return cssValuePool().createValue(iterationCount, CSSPrimitiveValue::UnitType::Number);
}

static CSSValue* valueForAnimationPlayState(EAnimPlayState playState)
{
    if (playState == AnimPlayStatePlaying)
        return cssValuePool().createIdentifierValue(CSSValueRunning);
    ASSERT(playState == AnimPlayStatePaused);
    return cssValuePool().createIdentifierValue(CSSValuePaused);
}

static CSSValue* createTimingFunctionValue(const TimingFunction* timingFunction)
{
    switch (timingFunction->type()) {
    case TimingFunction::kCubicBezierFunction:
        {
            const CubicBezierTimingFunction* bezierTimingFunction = toCubicBezierTimingFunction(timingFunction);
            if (bezierTimingFunction->subType() != CubicBezierTimingFunction::Custom) {
                CSSValueID valueId = CSSValueInvalid;
                switch (bezierTimingFunction->subType()) {
                case CubicBezierTimingFunction::Ease:
                    valueId = CSSValueEase;
                    break;
                case CubicBezierTimingFunction::EaseIn:
                    valueId = CSSValueEaseIn;
                    break;
                case CubicBezierTimingFunction::EaseOut:
                    valueId = CSSValueEaseOut;
                    break;
                case CubicBezierTimingFunction::EaseInOut:
                    valueId = CSSValueEaseInOut;
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    return nullptr;
                }
                return cssValuePool().createIdentifierValue(valueId);
            }
            return CSSCubicBezierTimingFunctionValue::create(bezierTimingFunction->x1(), bezierTimingFunction->y1(), bezierTimingFunction->x2(), bezierTimingFunction->y2());
        }

    case TimingFunction::kStepsFunction:
        {
            const StepsTimingFunction* stepsTimingFunction = toStepsTimingFunction(timingFunction);
            StepsTimingFunction::StepAtPosition position = stepsTimingFunction->getStepAtPosition();
            int steps = stepsTimingFunction->numberOfSteps();
            ASSERT(position == StepsTimingFunction::Start || position == StepsTimingFunction::End);

            if (steps > 1)
                return CSSStepsTimingFunctionValue::create(steps, position);
            CSSValueID valueId = position == StepsTimingFunction::Start ? CSSValueStepStart : CSSValueStepEnd;
            return cssValuePool().createIdentifierValue(valueId);
        }

    default:
        return cssValuePool().createIdentifierValue(CSSValueLinear);
    }
}

static CSSValue* valueForAnimationTimingFunction(const CSSTimingData* timingData)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    if (timingData) {
        for (size_t i = 0; i < timingData->timingFunctionList().size(); ++i)
            list->append(createTimingFunctionValue(timingData->timingFunctionList()[i].get()));
    } else {
        list->append(createTimingFunctionValue(CSSTimingData::initialTimingFunction().get()));
    }
    return list;
}

static CSSValueList* valuesForBorderRadiusCorner(LengthSize radius, const ComputedStyle& style)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    if (radius.width().type() == Percent)
        list->append(cssValuePool().createValue(radius.width().percent(), CSSPrimitiveValue::UnitType::Percentage));
    else
        list->append(zoomAdjustedPixelValueForLength(radius.width(), style));
    if (radius.height().type() == Percent)
        list->append(cssValuePool().createValue(radius.height().percent(), CSSPrimitiveValue::UnitType::Percentage));
    else
        list->append(zoomAdjustedPixelValueForLength(radius.height(), style));
    return list;
}

static CSSValue* valueForBorderRadiusCorner(LengthSize radius, const ComputedStyle& style)
{
    CSSValueList* list = valuesForBorderRadiusCorner(radius, style);
    if (list->item(0)->equals(*list->item(1)))
        return list->item(0);
    return list;
}

static CSSFunctionValue* valueForMatrixTransform(const TransformationMatrix& transform, const ComputedStyle& style)
{
    CSSFunctionValue* transformValue = nullptr;
    if (transform.isAffine()) {
        transformValue = CSSFunctionValue::create(CSSValueMatrix);

        transformValue->append(cssValuePool().createValue(transform.a(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.b(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.c(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.d(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(zoomAdjustedNumberValue(transform.e(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.f(), style));
    } else {
        transformValue = CSSFunctionValue::create(CSSValueMatrix3d);

        transformValue->append(cssValuePool().createValue(transform.m11(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m12(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m13(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m14(), CSSPrimitiveValue::UnitType::Number));

        transformValue->append(cssValuePool().createValue(transform.m21(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m22(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m23(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m24(), CSSPrimitiveValue::UnitType::Number));

        transformValue->append(cssValuePool().createValue(transform.m31(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m32(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m33(), CSSPrimitiveValue::UnitType::Number));
        transformValue->append(cssValuePool().createValue(transform.m34(), CSSPrimitiveValue::UnitType::Number));

        transformValue->append(zoomAdjustedNumberValue(transform.m41(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m42(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m43(), style));
        transformValue->append(cssValuePool().createValue(transform.m44(), CSSPrimitiveValue::UnitType::Number));
    }

    return transformValue;
}

static CSSValue* computedTransform(const LayoutObject* layoutObject, const ComputedStyle& style)
{
    if (!layoutObject || !style.hasTransform())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    IntRect box;
    if (layoutObject->isBox())
        box = pixelSnappedIntRect(toLayoutBox(layoutObject)->borderBoxRect());

    TransformationMatrix transform;
    style.applyTransform(transform, LayoutSize(box.size()), ComputedStyle::ExcludeTransformOrigin, ComputedStyle::ExcludeMotionPath, ComputedStyle::ExcludeIndependentTransformProperties);

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(valueForMatrixTransform(transform, style));

    return list;
}

static CSSValue* createTransitionPropertyValue(const CSSTransitionData::TransitionProperty& property)
{
    if (property.propertyType == CSSTransitionData::TransitionNone)
        return cssValuePool().createIdentifierValue(CSSValueNone);
    if (property.propertyType == CSSTransitionData::TransitionUnknownProperty)
        return CSSCustomIdentValue::create(property.propertyString);
    ASSERT(property.propertyType == CSSTransitionData::TransitionKnownProperty);
    return CSSCustomIdentValue::create(getPropertyNameString(property.unresolvedProperty));
}

static CSSValue* valueForTransitionProperty(const CSSTransitionData* transitionData)
{
    CSSValueList* list = CSSValueList::createCommaSeparated();
    if (transitionData) {
        for (size_t i = 0; i < transitionData->propertyList().size(); ++i)
            list->append(createTransitionPropertyValue(transitionData->propertyList()[i]));
    } else {
        list->append(cssValuePool().createIdentifierValue(CSSValueAll));
    }
    return list;
}

CSSValueID valueForQuoteType(const QuoteType quoteType)
{
    switch (quoteType) {
    case NO_OPEN_QUOTE:
        return CSSValueNoOpenQuote;
    case NO_CLOSE_QUOTE:
        return CSSValueNoCloseQuote;
    case CLOSE_QUOTE:
        return CSSValueCloseQuote;
    case OPEN_QUOTE:
        return CSSValueOpenQuote;
    }
    ASSERT_NOT_REACHED();
    return CSSValueInvalid;
}

static CSSValue* valueForContentData(const ComputedStyle& style)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    for (const ContentData* contentData = style.contentData(); contentData; contentData = contentData->next()) {
        if (contentData->isCounter()) {
            const CounterContent* counter = toCounterContentData(contentData)->counter();
            ASSERT(counter);
            CSSCustomIdentValue* identifier = CSSCustomIdentValue::create(counter->identifier());
            CSSCustomIdentValue* separator = CSSCustomIdentValue::create(counter->separator());
            CSSValueID listStyleIdent = CSSValueNone;
            if (counter->listStyle() != NoneListStyle)
                listStyleIdent = static_cast<CSSValueID>(CSSValueDisc + counter->listStyle());
            CSSPrimitiveValue* listStyle = cssValuePool().createIdentifierValue(listStyleIdent);
            list->append(CSSCounterValue::create(identifier, listStyle, separator));
        } else if (contentData->isImage()) {
            const StyleImage* image = toImageContentData(contentData)->image();
            ASSERT(image);
            list->append(image->computedCSSValue());
        } else if (contentData->isText()) {
            list->append(CSSStringValue::create(toTextContentData(contentData)->text()));
        } else if (contentData->isQuote()) {
            const QuoteType quoteType = toQuoteContentData(contentData)->quote();
            list->append(cssValuePool().createIdentifierValue(valueForQuoteType(quoteType)));
        } else {
            ASSERT_NOT_REACHED();
        }
    }
    return list;
}

static CSSValue* valueForCounterDirectives(const ComputedStyle& style, CSSPropertyID propertyID)
{
    const CounterDirectiveMap* map = style.counterDirectives();
    if (!map)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    for (const auto& item : *map) {
        bool isValidCounterValue = propertyID == CSSPropertyCounterIncrement ? item.value.isIncrement() : item.value.isReset();
        if (!isValidCounterValue)
            continue;

        list->append(CSSCustomIdentValue::create(item.key));
        short number = propertyID == CSSPropertyCounterIncrement ? item.value.incrementValue() : item.value.resetValue();
        list->append(cssValuePool().createValue((double)number, CSSPrimitiveValue::UnitType::Integer));
    }

    if (!list->length())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    return list;
}

static CSSValue* valueForShape(const ComputedStyle& style, ShapeValue* shapeValue)
{
    if (!shapeValue)
        return cssValuePool().createIdentifierValue(CSSValueNone);
    if (shapeValue->type() == ShapeValue::Box)
        return cssValuePool().createValue(shapeValue->cssBox());
    if (shapeValue->type() == ShapeValue::Image) {
        if (shapeValue->image())
            return shapeValue->image()->computedCSSValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Shape);

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(valueForBasicShape(style, shapeValue->shape()));
    if (shapeValue->cssBox() != BoxMissing)
        list->append(cssValuePool().createValue(shapeValue->cssBox()));
    return list;
}

static CSSValueList* valuesForSidesShorthand(const StylePropertyShorthand& shorthand, const ComputedStyle& style, const LayoutObject* layoutObject, Node* styledNode, bool allowVisitedStyle)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    // Assume the properties are in the usual order top, right, bottom, left.
    CSSValue* topValue = ComputedStyleCSSValueMapping::get(shorthand.properties()[0], style, layoutObject, styledNode, allowVisitedStyle);
    CSSValue* rightValue = ComputedStyleCSSValueMapping::get(shorthand.properties()[1], style, layoutObject, styledNode, allowVisitedStyle);
    CSSValue* bottomValue = ComputedStyleCSSValueMapping::get(shorthand.properties()[2], style, layoutObject, styledNode, allowVisitedStyle);
    CSSValue* leftValue = ComputedStyleCSSValueMapping::get(shorthand.properties()[3], style, layoutObject, styledNode, allowVisitedStyle);

    // All 4 properties must be specified.
    if (!topValue || !rightValue || !bottomValue || !leftValue)
        return nullptr;

    bool showLeft = !compareCSSValuePtr(rightValue, leftValue);
    bool showBottom = !compareCSSValuePtr(topValue, bottomValue) || showLeft;
    bool showRight = !compareCSSValuePtr(topValue, rightValue) || showBottom;

    list->append(topValue);
    if (showRight)
        list->append(rightValue);
    if (showBottom)
        list->append(bottomValue);
    if (showLeft)
        list->append(leftValue);

    return list;
}

static CSSValueList* valueForBorderRadiusShorthand(const ComputedStyle& style)
{
    CSSValueList* list = CSSValueList::createSlashSeparated();

    bool showHorizontalBottomLeft = style.borderTopRightRadius().width() != style.borderBottomLeftRadius().width();
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style.borderBottomRightRadius().width() != style.borderTopLeftRadius().width());
    bool showHorizontalTopRight = showHorizontalBottomRight || (style.borderTopRightRadius().width() != style.borderTopLeftRadius().width());

    bool showVerticalBottomLeft = style.borderTopRightRadius().height() != style.borderBottomLeftRadius().height();
    bool showVerticalBottomRight = showVerticalBottomLeft || (style.borderBottomRightRadius().height() != style.borderTopLeftRadius().height());
    bool showVerticalTopRight = showVerticalBottomRight || (style.borderTopRightRadius().height() != style.borderTopLeftRadius().height());

    CSSValueList* topLeftRadius = valuesForBorderRadiusCorner(style.borderTopLeftRadius(), style);
    CSSValueList* topRightRadius = valuesForBorderRadiusCorner(style.borderTopRightRadius(), style);
    CSSValueList* bottomRightRadius = valuesForBorderRadiusCorner(style.borderBottomRightRadius(), style);
    CSSValueList* bottomLeftRadius = valuesForBorderRadiusCorner(style.borderBottomLeftRadius(), style);

    CSSValueList* horizontalRadii = CSSValueList::createSpaceSeparated();
    horizontalRadii->append(topLeftRadius->item(0));
    if (showHorizontalTopRight)
        horizontalRadii->append(topRightRadius->item(0));
    if (showHorizontalBottomRight)
        horizontalRadii->append(bottomRightRadius->item(0));
    if (showHorizontalBottomLeft)
        horizontalRadii->append(bottomLeftRadius->item(0));

    list->append(horizontalRadii);

    CSSValueList* verticalRadii = CSSValueList::createSpaceSeparated();
    verticalRadii->append(topLeftRadius->item(1));
    if (showVerticalTopRight)
        verticalRadii->append(topRightRadius->item(1));
    if (showVerticalBottomRight)
        verticalRadii->append(bottomRightRadius->item(1));
    if (showVerticalBottomLeft)
        verticalRadii->append(bottomLeftRadius->item(1));

    if (!verticalRadii->equals(*toCSSValueList(list->item(0))))
        list->append(verticalRadii);

    return list;
}

static CSSValue* strokeDashArrayToCSSValueList(const SVGDashArray& dashes, const ComputedStyle& style)
{
    if (dashes.isEmpty())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    CSSValueList* list = CSSValueList::createCommaSeparated();
    for (const Length& dashLength : dashes.vector())
        list->append(zoomAdjustedPixelValueForLength(dashLength, style));

    return list;
}

static CSSValue* paintOrderToCSSValueList(const SVGComputedStyle& svgStyle)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    for (int i = 0; i < 3; i++) {
        EPaintOrderType paintOrderType = svgStyle.paintOrderType(i);
        switch (paintOrderType) {
        case PT_FILL:
        case PT_STROKE:
        case PT_MARKERS:
            list->append(CSSPrimitiveValue::create(paintOrderType));
            break;
        case PT_NONE:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return list;
}

static CSSValue* adjustSVGPaintForCurrentColor(SVGPaintType paintType, const String& url, const Color& color, const Color& currentColor)
{
    if (paintType >= SVG_PAINTTYPE_URI_NONE) {
        CSSValueList* values = CSSValueList::createSpaceSeparated();
        values->append(CSSURIValue::create(url));
        if (paintType == SVG_PAINTTYPE_URI_NONE)
            values->append(CSSPrimitiveValue::createIdentifier(CSSValueNone));
        else if (paintType == SVG_PAINTTYPE_URI_CURRENTCOLOR)
            values->append(CSSColorValue::create(currentColor.rgb()));
        else if (paintType == SVG_PAINTTYPE_URI_RGBCOLOR)
            values->append(CSSColorValue::create(color.rgb()));
        return values;
    }
    if (paintType == SVG_PAINTTYPE_NONE)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    if (paintType == SVG_PAINTTYPE_CURRENTCOLOR)
        return CSSColorValue::create(currentColor.rgb());

    return CSSColorValue::create(color.rgb());
}

static inline String serializeAsFragmentIdentifier(const AtomicString& resource)
{
    return "#" + resource;
}

CSSValue* ComputedStyleCSSValueMapping::valueForShadowData(const ShadowData& shadow, const ComputedStyle& style, bool useSpread)
{
    CSSPrimitiveValue* x = zoomAdjustedPixelValue(shadow.x(), style);
    CSSPrimitiveValue* y = zoomAdjustedPixelValue(shadow.y(), style);
    CSSPrimitiveValue* blur = zoomAdjustedPixelValue(shadow.blur(), style);
    CSSPrimitiveValue* spread = useSpread ? zoomAdjustedPixelValue(shadow.spread(), style) : nullptr;
    CSSPrimitiveValue* shadowStyle = shadow.style() == Normal ? nullptr : cssValuePool().createIdentifierValue(CSSValueInset);
    CSSValue* color = currentColorOrValidColor(style, shadow.color());
    return CSSShadowValue::create(x, y, blur, spread, shadowStyle, color);
}

CSSValue* ComputedStyleCSSValueMapping::valueForShadowList(const ShadowList* shadowList, const ComputedStyle& style, bool useSpread)
{
    if (!shadowList)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    CSSValueList* list = CSSValueList::createCommaSeparated();
    size_t shadowCount = shadowList->shadows().size();
    for (size_t i = 0; i < shadowCount; ++i)
        list->append(valueForShadowData(shadowList->shadows()[i], style, useSpread));
    return list;
}

CSSValue* ComputedStyleCSSValueMapping::valueForFilter(const ComputedStyle& style, const FilterOperations& filterOperations)
{
    if (filterOperations.operations().isEmpty())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    CSSValueList* list = CSSValueList::createSpaceSeparated();

    CSSFunctionValue* filterValue = nullptr;

    for (const auto& operation : filterOperations.operations()) {
        FilterOperation* filterOperation = operation.get();
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE:
            filterValue = CSSFunctionValue::create(CSSValueUrl);
            filterValue->append(CSSStringValue::create(toReferenceFilterOperation(filterOperation)->url()));
            break;
        case FilterOperation::GRAYSCALE:
            filterValue = CSSFunctionValue::create(CSSValueGrayscale);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::SEPIA:
            filterValue = CSSFunctionValue::create(CSSValueSepia);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::SATURATE:
            filterValue = CSSFunctionValue::create(CSSValueSaturate);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::HUE_ROTATE:
            filterValue = CSSFunctionValue::create(CSSValueHueRotate);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Degrees));
            break;
        case FilterOperation::INVERT:
            filterValue = CSSFunctionValue::create(CSSValueInvert);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::OPACITY:
            filterValue = CSSFunctionValue::create(CSSValueOpacity);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::BRIGHTNESS:
            filterValue = CSSFunctionValue::create(CSSValueBrightness);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::CONTRAST:
            filterValue = CSSFunctionValue::create(CSSValueContrast);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::UnitType::Number));
            break;
        case FilterOperation::BLUR:
            filterValue = CSSFunctionValue::create(CSSValueBlur);
            filterValue->append(zoomAdjustedPixelValue(toBlurFilterOperation(filterOperation)->stdDeviation().value(), style));
            break;
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = toDropShadowFilterOperation(filterOperation);
            filterValue = CSSFunctionValue::create(CSSValueDropShadow);
            // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
            ShadowData shadow(dropShadowOperation->location(), dropShadowOperation->stdDeviation(), 0, Normal, StyleColor(dropShadowOperation->getColor()));
            filterValue->append(valueForShadowData(shadow, style, false));
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        list->append(filterValue);
    }

    return list;
}

CSSValue* ComputedStyleCSSValueMapping::valueForFont(const ComputedStyle& style)
{
    // Add a slash between size and line-height.
    CSSValueList* sizeAndLineHeight = CSSValueList::createSlashSeparated();
    sizeAndLineHeight->append(valueForFontSize(style));
    sizeAndLineHeight->append(valueForLineHeight(style));

    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(valueForFontStyle(style));
    list->append(valueForFontVariant(style));
    list->append(valueForFontWeight(style));
    list->append(valueForFontStretch(style));
    list->append(sizeAndLineHeight);
    list->append(valueForFontFamily(style));

    return list;
}

static CSSValue* valueForScrollSnapDestination(const LengthPoint& destination, const ComputedStyle& style)
{
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(destination.x(), style));
    list->append(zoomAdjustedPixelValueForLength(destination.y(), style));
    return list;
}

static CSSValue* valueForScrollSnapPoints(const ScrollSnapPoints& points, const ComputedStyle& style)
{
    if (points.hasRepeat) {
        CSSFunctionValue* repeat = CSSFunctionValue::create(CSSValueRepeat);
        repeat->append(zoomAdjustedPixelValueForLength(points.repeatOffset, style));
        return repeat;
    }

    return cssValuePool().createIdentifierValue(CSSValueNone);
}

static CSSValue* valueForScrollSnapCoordinate(const Vector<LengthPoint>& coordinates, const ComputedStyle& style)
{
    if (coordinates.isEmpty())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    CSSValueList* list = CSSValueList::createCommaSeparated();

    for (auto& coordinate : coordinates) {
        auto pair = CSSValueList::createSpaceSeparated();
        pair->append(zoomAdjustedPixelValueForLength(coordinate.x(), style));
        pair->append(zoomAdjustedPixelValueForLength(coordinate.y(), style));
        list->append(pair);
    }

    return list;
}

static EBreak mapToPageBreakValue(EBreak genericBreakValue)
{
    switch (genericBreakValue) {
    case BreakAvoidColumn:
    case BreakColumn:
    case BreakRecto:
    case BreakVerso:
        return BreakAuto;
    case BreakPage:
        return BreakAlways;
    case BreakAvoidPage:
        return BreakAvoid;
    default:
        return genericBreakValue;
    }
}

static EBreak mapToColumnBreakValue(EBreak genericBreakValue)
{
    switch (genericBreakValue) {
    case BreakAvoidPage:
    case BreakLeft:
    case BreakPage:
    case BreakRecto:
    case BreakRight:
    case BreakVerso:
        return BreakAuto;
    case BreakColumn:
        return BreakAlways;
    case BreakAvoidColumn:
        return BreakAvoid;
    default:
        return genericBreakValue;
    }
}

CSSValue* ComputedStyleCSSValueMapping::get(const AtomicString customPropertyName, const ComputedStyle& style)
{
    StyleVariableData* variables = style.variables();
    if (!variables)
        return nullptr;

    CSSVariableData* data = variables->getVariable(customPropertyName);
    if (!data)
        return nullptr;

    return CSSCustomPropertyDeclaration::create(customPropertyName, data);
}

std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> ComputedStyleCSSValueMapping::getVariables(const ComputedStyle& style)
{
    StyleVariableData* variables = style.variables();
    if (variables)
        return variables->getVariables();
    return nullptr;
}

CSSValue* ComputedStyleCSSValueMapping::get(CSSPropertyID propertyID, const ComputedStyle& style, const LayoutObject* layoutObject, Node* styledNode, bool allowVisitedStyle)
{
    const SVGComputedStyle& svgStyle = style.svgStyle();
    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style.direction(), style.getWritingMode());
    switch (propertyID) {
    case CSSPropertyInvalid:
        return nullptr;

    case CSSPropertyBackgroundColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyBackgroundColor).rgb()) : currentColorOrValidColor(style, style.backgroundColor());
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskImage ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next()) {
            if (currLayer->image())
                list->append(currLayer->image()->computedCSSValue());
            else
                list->append(cssValuePool().createIdentifierValue(CSSValueNone));
        }
        return list;
    }
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskSize ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next())
            list->append(valueForFillSize(currLayer->size(), style));
        return list;
    }
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskRepeat ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next())
            list->append(valueForFillRepeat(currLayer->repeatX(), currLayer->repeatY()));
        return list;
    }
    case CSSPropertyMaskSourceType: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        for (const FillLayer* currLayer = &style.maskLayers(); currLayer; currLayer = currLayer->next())
            list->append(valueForFillSourceType(currLayer->maskSourceType()));
        return list;
    }
    case CSSPropertyWebkitMaskComposite: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskComposite ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next())
            list->append(cssValuePool().createValue(currLayer->composite()));
        return list;
    }
    case CSSPropertyBackgroundAttachment: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        for (const FillLayer* currLayer = &style.backgroundLayers(); currLayer; currLayer = currLayer->next())
            list->append(cssValuePool().createValue(currLayer->attachment()));
        return list;
    }
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskOrigin: {
        bool isClip = propertyID == CSSPropertyBackgroundClip || propertyID == CSSPropertyWebkitBackgroundClip || propertyID == CSSPropertyWebkitMaskClip;
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = (propertyID == CSSPropertyWebkitMaskClip || propertyID == CSSPropertyWebkitMaskOrigin) ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next()) {
            EFillBox box = isClip ? currLayer->clip() : currLayer->origin();
            list->append(cssValuePool().createValue(box));
        }
        return list;
    }
    case CSSPropertyBackgroundPosition:
    case CSSPropertyWebkitMaskPosition: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskPosition ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next())
            list->append(createPositionListForLayer(propertyID, *currLayer, style));
        return list;
    }
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskPositionX ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next())
            list->append(zoomAdjustedPixelValueForLength(currLayer->xPosition(), style));
        return list;
    }
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskPositionY ? &style.maskLayers() : &style.backgroundLayers();
        for (; currLayer; currLayer = currLayer->next())
            list->append(zoomAdjustedPixelValueForLength(currLayer->yPosition(), style));
        return list;
    }
    case CSSPropertyBorderCollapse:
        if (style.borderCollapse())
            return cssValuePool().createIdentifierValue(CSSValueCollapse);
        return cssValuePool().createIdentifierValue(CSSValueSeparate);
    case CSSPropertyBorderSpacing: {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        list->append(zoomAdjustedPixelValue(style.horizontalBorderSpacing(), style));
        list->append(zoomAdjustedPixelValue(style.verticalBorderSpacing(), style));
        return list;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
        return zoomAdjustedPixelValue(style.horizontalBorderSpacing(), style);
    case CSSPropertyWebkitBorderVerticalSpacing:
        return zoomAdjustedPixelValue(style.verticalBorderSpacing(), style);
    case CSSPropertyBorderImageSource:
        if (style.borderImageSource())
            return style.borderImageSource()->computedCSSValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyBorderTopColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyBorderTopColor).rgb()) : currentColorOrValidColor(style, style.borderTopColor());
    case CSSPropertyBorderRightColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyBorderRightColor).rgb()) : currentColorOrValidColor(style, style.borderRightColor());
    case CSSPropertyBorderBottomColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyBorderBottomColor).rgb()) : currentColorOrValidColor(style, style.borderBottomColor());
    case CSSPropertyBorderLeftColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyBorderLeftColor).rgb()) : currentColorOrValidColor(style, style.borderLeftColor());
    case CSSPropertyBorderTopStyle:
        return cssValuePool().createValue(style.borderTopStyle());
    case CSSPropertyBorderRightStyle:
        return cssValuePool().createValue(style.borderRightStyle());
    case CSSPropertyBorderBottomStyle:
        return cssValuePool().createValue(style.borderBottomStyle());
    case CSSPropertyBorderLeftStyle:
        return cssValuePool().createValue(style.borderLeftStyle());
    case CSSPropertyBorderTopWidth:
        return zoomAdjustedPixelValue(style.borderTopWidth(), style);
    case CSSPropertyBorderRightWidth:
        return zoomAdjustedPixelValue(style.borderRightWidth(), style);
    case CSSPropertyBorderBottomWidth:
        return zoomAdjustedPixelValue(style.borderBottomWidth(), style);
    case CSSPropertyBorderLeftWidth:
        return zoomAdjustedPixelValue(style.borderLeftWidth(), style);
    case CSSPropertyBottom:
        return valueForPositionOffset(style, CSSPropertyBottom, layoutObject);
    case CSSPropertyWebkitBoxAlign:
        return cssValuePool().createValue(style.boxAlign());
    case CSSPropertyWebkitBoxDecorationBreak:
        if (style.boxDecorationBreak() == BoxDecorationBreakSlice)
            return cssValuePool().createIdentifierValue(CSSValueSlice);
    return cssValuePool().createIdentifierValue(CSSValueClone);
    case CSSPropertyWebkitBoxDirection:
        return cssValuePool().createValue(style.boxDirection());
    case CSSPropertyWebkitBoxFlex:
        return cssValuePool().createValue(style.boxFlex(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWebkitBoxFlexGroup:
        return cssValuePool().createValue(style.boxFlexGroup(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWebkitBoxLines:
        return cssValuePool().createValue(style.boxLines());
    case CSSPropertyWebkitBoxOrdinalGroup:
        return cssValuePool().createValue(style.boxOrdinalGroup(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWebkitBoxOrient:
        return cssValuePool().createValue(style.boxOrient());
    case CSSPropertyWebkitBoxPack:
        return cssValuePool().createValue(style.boxPack());
    case CSSPropertyWebkitBoxReflect:
        return valueForReflection(style.boxReflect(), style);
    case CSSPropertyBoxShadow:
        return valueForShadowList(style.boxShadow(), style, true);
    case CSSPropertyCaptionSide:
        return cssValuePool().createValue(style.captionSide());
    case CSSPropertyClear:
        return cssValuePool().createValue(style.clear());
    case CSSPropertyColor:
        return cssValuePool().createColorValue(allowVisitedStyle ? style.visitedDependentColor(CSSPropertyColor).rgb() : style.color().rgb());
    case CSSPropertyWebkitPrintColorAdjust:
        return cssValuePool().createValue(style.getPrintColorAdjust());
    case CSSPropertyColumnCount:
        if (style.hasAutoColumnCount())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        return cssValuePool().createValue(style.columnCount(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyColumnFill:
        ASSERT(RuntimeEnabledFeatures::columnFillEnabled());
        return cssValuePool().createValue(style.getColumnFill());
    case CSSPropertyColumnGap:
        if (style.hasNormalColumnGap())
            return cssValuePool().createIdentifierValue(CSSValueNormal);
        return zoomAdjustedPixelValue(style.columnGap(), style);
    case CSSPropertyColumnRuleColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style, style.columnRuleColor());
    case CSSPropertyColumnRuleStyle:
        return cssValuePool().createValue(style.columnRuleStyle());
    case CSSPropertyColumnRuleWidth:
        return zoomAdjustedPixelValue(style.columnRuleWidth(), style);
    case CSSPropertyColumnSpan:
        return cssValuePool().createIdentifierValue(style.getColumnSpan() ? CSSValueAll : CSSValueNone);
    case CSSPropertyWebkitColumnBreakAfter:
        return cssValuePool().createValue(mapToColumnBreakValue(style.breakAfter()));
    case CSSPropertyWebkitColumnBreakBefore:
        return cssValuePool().createValue(mapToColumnBreakValue(style.breakBefore()));
    case CSSPropertyWebkitColumnBreakInside:
        return cssValuePool().createValue(mapToColumnBreakValue(style.breakInside()));
    case CSSPropertyColumnWidth:
        if (style.hasAutoColumnWidth())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        return zoomAdjustedPixelValue(style.columnWidth(), style);
    case CSSPropertyTabSize:
        return cssValuePool().createValue(
            style.getTabSize().getPixelSize(1.0), style.getTabSize().isSpaces() ? CSSPrimitiveValue::UnitType::Number : CSSPrimitiveValue::UnitType::Pixels);
    case CSSPropertyCursor: {
        CSSValueList* list = nullptr;
        CursorList* cursors = style.cursors();
        if (cursors && cursors->size() > 0) {
            list = CSSValueList::createCommaSeparated();
            for (unsigned i = 0; i < cursors->size(); ++i) {
                if (StyleImage* image = cursors->at(i).image())
                    list->append(image->computedCSSValue());
            }
        }
        CSSValue* value = cssValuePool().createValue(style.cursor());
        if (list) {
            list->append(value);
            return list;
        }
        return value;
    }
    case CSSPropertyDirection:
        return cssValuePool().createValue(style.direction());
    case CSSPropertyDisplay:
        return cssValuePool().createValue(style.display());
    case CSSPropertyEmptyCells:
        return cssValuePool().createValue(style.emptyCells());
    case CSSPropertyAlignContent:
        return valueForContentPositionAndDistributionWithOverflowAlignment(style.alignContent());
    case CSSPropertyAlignItems:
        return valueForItemPositionWithOverflowAlignment(resolveAlignmentAuto(style.alignItemsPosition(), &style), style.alignItemsOverflowAlignment(), NonLegacyPosition);
    case CSSPropertyAlignSelf: {
        ItemPosition position = style.alignSelfPosition();
        if (position == ItemPositionAuto) {
            // TODO(lajava): This code doesn't work for ShadowDOM (see Node::parentComputedStyle)
            const ComputedStyle* parentStyle = styledNode->parentNode() ? styledNode->parentNode()->ensureComputedStyle() : nullptr;
            position = parentStyle ? ComputedStyle::resolveAlignment(*parentStyle, style, resolveAlignmentAuto(parentStyle->alignItemsPosition(), parentStyle)) : ItemPositionStart;
        }
        return valueForItemPositionWithOverflowAlignment(position, style.alignSelfOverflowAlignment(), NonLegacyPosition);
    }
    case CSSPropertyFlex:
        return valuesForShorthandProperty(flexShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyFlexBasis:
        return zoomAdjustedPixelValueForLength(style.flexBasis(), style);
    case CSSPropertyFlexDirection:
        return cssValuePool().createValue(style.flexDirection());
    case CSSPropertyFlexFlow:
        return valuesForShorthandProperty(flexFlowShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyFlexGrow:
        return cssValuePool().createValue(style.flexGrow(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFlexShrink:
        return cssValuePool().createValue(style.flexShrink(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFlexWrap:
        return cssValuePool().createValue(style.flexWrap());
    case CSSPropertyJustifyContent:
        return valueForContentPositionAndDistributionWithOverflowAlignment(style.justifyContent());
    case CSSPropertyOrder:
        return cssValuePool().createValue(style.order(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFloat:
        if (style.display() != NONE && style.hasOutOfFlowPosition())
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return cssValuePool().createValue(style.floating());
    case CSSPropertyFont:
        return valueForFont(style);
    case CSSPropertyFontFamily:
        return valueForFontFamily(style);
    case CSSPropertyFontSize:
        return valueForFontSize(style);
    case CSSPropertyFontSizeAdjust:
        if (style.hasFontSizeAdjust())
            return cssValuePool().createValue(style.fontSizeAdjust(), CSSPrimitiveValue::UnitType::Number);
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyFontStretch:
        return valueForFontStretch(style);
    case CSSPropertyFontStyle:
        return valueForFontStyle(style);
    case CSSPropertyFontVariant:
        return valueForFontVariant(style);
    case CSSPropertyFontWeight:
        return valueForFontWeight(style);
    case CSSPropertyFontFeatureSettings: {
        const FontFeatureSettings* featureSettings = style.getFontDescription().featureSettings();
        if (!featureSettings || !featureSettings->size())
            return cssValuePool().createIdentifierValue(CSSValueNormal);
        CSSValueList* list = CSSValueList::createCommaSeparated();
        for (unsigned i = 0; i < featureSettings->size(); ++i) {
            const FontFeature& feature = featureSettings->at(i);
            CSSFontFeatureValue* featureValue = CSSFontFeatureValue::create(feature.tag(), feature.value());
            list->append(featureValue);
        }
        return list;
    }
    case CSSPropertyGridAutoFlow: {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        switch (style.getGridAutoFlow()) {
        case AutoFlowRow:
        case AutoFlowRowDense:
            list->append(cssValuePool().createIdentifierValue(CSSValueRow));
            break;
        case AutoFlowColumn:
        case AutoFlowColumnDense:
            list->append(cssValuePool().createIdentifierValue(CSSValueColumn));
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        switch (style.getGridAutoFlow()) {
        case AutoFlowRowDense:
        case AutoFlowColumnDense:
            list->append(cssValuePool().createIdentifierValue(CSSValueDense));
            break;
        default:
            // Do nothing.
            break;
        }

        return list;
    }
    // Specs mention that getComputedStyle() should return the used value of the property instead of the computed
    // one for grid-definition-{rows|columns} but not for the grid-auto-{rows|columns} as things like
    // grid-auto-columns: 2fr; cannot be resolved to a value in pixels as the '2fr' means very different things
    // depending on the size of the explicit grid or the number of implicit tracks added to the grid. See
    // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
    case CSSPropertyGridAutoColumns:
        return specifiedValueForGridTrackSize(style.gridAutoColumns(), style);
    case CSSPropertyGridAutoRows:
        return specifiedValueForGridTrackSize(style.gridAutoRows(), style);

    case CSSPropertyGridTemplateColumns:
        return valueForGridTrackList(ForColumns, layoutObject, style);
    case CSSPropertyGridTemplateRows:
        return valueForGridTrackList(ForRows, layoutObject, style);

    case CSSPropertyGridColumnStart:
        return valueForGridPosition(style.gridColumnStart());
    case CSSPropertyGridColumnEnd:
        return valueForGridPosition(style.gridColumnEnd());
    case CSSPropertyGridRowStart:
        return valueForGridPosition(style.gridRowStart());
    case CSSPropertyGridRowEnd:
        return valueForGridPosition(style.gridRowEnd());
    case CSSPropertyGridColumn:
        return valuesForGridShorthand(gridColumnShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyGridRow:
        return valuesForGridShorthand(gridRowShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyGridArea:
        return valuesForGridShorthand(gridAreaShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyGridTemplate:
        return valuesForGridShorthand(gridTemplateShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyGrid:
        return valuesForGridShorthand(gridShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyGridTemplateAreas:
        if (!style.namedGridAreaRowCount()) {
            ASSERT(!style.namedGridAreaColumnCount());
            return cssValuePool().createIdentifierValue(CSSValueNone);
        }

        return CSSGridTemplateAreasValue::create(style.namedGridArea(), style.namedGridAreaRowCount(), style.namedGridAreaColumnCount());
    case CSSPropertyGridColumnGap:
        return zoomAdjustedPixelValueForLength(style.gridColumnGap(), style);
    case CSSPropertyGridRowGap:
        return zoomAdjustedPixelValueForLength(style.gridRowGap(), style);
    case CSSPropertyGridGap:
        return valuesForShorthandProperty(gridGapShorthand(), style, layoutObject, styledNode, allowVisitedStyle);

    case CSSPropertyHeight:
        if (layoutObject) {
            // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
            // the "height" property does not apply for non-atomic inline elements.
            if (!layoutObject->isAtomicInlineLevel() && layoutObject->isInline())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(sizingBox(layoutObject).height(), style);
        }
        return zoomAdjustedPixelValueForLength(style.height(), style);
    case CSSPropertyWebkitHighlight:
        if (style.highlight() == nullAtom)
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return CSSStringValue::create(style.highlight());
    case CSSPropertyWebkitHyphenateCharacter:
        if (style.hyphenationString().isNull())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        return CSSStringValue::create(style.hyphenationString());
    case CSSPropertyImageRendering:
        return CSSPrimitiveValue::create(style.imageRendering());
    case CSSPropertyImageOrientation:
        if (style.respectImageOrientation() == RespectImageOrientation)
            return cssValuePool().createIdentifierValue(CSSValueFromImage);
        return cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Degrees);
    case CSSPropertyIsolation:
        return cssValuePool().createValue(style.isolation());
    case CSSPropertyJustifyItems:
        return valueForItemPositionWithOverflowAlignment(resolveAlignmentAuto(style.justifyItemsPosition(), &style), style.justifyItemsOverflowAlignment(), style.justifyItemsPositionType());
    case CSSPropertyJustifySelf: {
        Node* parent = styledNode->parentNode();
        return valueForItemPositionWithOverflowAlignment(resolveAlignmentAuto(style.justifySelfPosition(), parent ? parent->ensureComputedStyle() : nullptr), style.justifySelfOverflowAlignment(), NonLegacyPosition);
    }
    case CSSPropertyLeft:
        return valueForPositionOffset(style, CSSPropertyLeft, layoutObject);
    case CSSPropertyLetterSpacing:
        if (!style.letterSpacing())
            return cssValuePool().createIdentifierValue(CSSValueNormal);
        return zoomAdjustedPixelValue(style.letterSpacing(), style);
    case CSSPropertyWebkitLineClamp:
        if (style.lineClamp().isNone())
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return cssValuePool().createValue(style.lineClamp().value(), style.lineClamp().isPercentage() ? CSSPrimitiveValue::UnitType::Percentage : CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyLineHeight:
        return valueForLineHeight(style);
    case CSSPropertyListStyleImage:
        if (style.listStyleImage())
            return style.listStyleImage()->computedCSSValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyListStylePosition:
        return cssValuePool().createValue(style.listStylePosition());
    case CSSPropertyListStyleType:
        return cssValuePool().createValue(style.listStyleType());
    case CSSPropertyWebkitLocale:
        if (style.locale().isNull())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        return CSSStringValue::create(style.locale());
    case CSSPropertyMarginTop: {
        Length marginTop = style.marginTop();
        if (marginTop.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(marginTop, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->marginTop(), style);
    }
    case CSSPropertyMarginRight: {
        Length marginRight = style.marginRight();
        if (marginRight.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(marginRight, style);
        float value;
        if (marginRight.hasPercent()) {
            // LayoutBox gives a marginRight() that is the distance between the right-edge of the child box
            // and the right-edge of the containing box, when display == BLOCK. Let's calculate the absolute
            // value of the specified margin-right % instead of relying on LayoutBox's marginRight() value.
            value = minimumValueForLength(marginRight, toLayoutBox(layoutObject)->containingBlockLogicalWidthForContent()).toFloat();
        } else {
            value = toLayoutBox(layoutObject)->marginRight().toFloat();
        }
        return zoomAdjustedPixelValue(value, style);
    }
    case CSSPropertyMarginBottom: {
        Length marginBottom = style.marginBottom();
        if (marginBottom.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(marginBottom, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->marginBottom(), style);
    }
    case CSSPropertyMarginLeft: {
        Length marginLeft = style.marginLeft();
        if (marginLeft.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(marginLeft, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->marginLeft(), style);
    }
    case CSSPropertyWebkitUserModify:
        return cssValuePool().createValue(style.userModify());
    case CSSPropertyMaxHeight: {
        const Length& maxHeight = style.maxHeight();
        if (maxHeight.isMaxSizeNone())
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return zoomAdjustedPixelValueForLength(maxHeight, style);
    }
    case CSSPropertyMaxWidth: {
        const Length& maxWidth = style.maxWidth();
        if (maxWidth.isMaxSizeNone())
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return zoomAdjustedPixelValueForLength(maxWidth, style);
    }
    case CSSPropertyMinHeight:
        if (style.minHeight().isAuto()) {
            Node* parent = styledNode->parentNode();
            if (isFlexOrGrid(parent ? parent->ensureComputedStyle() : nullptr))
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(0, style);
        }
        return zoomAdjustedPixelValueForLength(style.minHeight(), style);
    case CSSPropertyMinWidth:
        if (style.minWidth().isAuto()) {
            Node* parent = styledNode->parentNode();
            if (isFlexOrGrid(parent ? parent->ensureComputedStyle() : nullptr))
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(0, style);
        }
        return zoomAdjustedPixelValueForLength(style.minWidth(), style);
    case CSSPropertyObjectFit:
        return cssValuePool().createValue(style.getObjectFit());
    case CSSPropertyObjectPosition:
        return CSSValuePair::create(
            zoomAdjustedPixelValueForLength(style.objectPosition().x(), style),
            zoomAdjustedPixelValueForLength(style.objectPosition().y(), style),
            CSSValuePair::KeepIdenticalValues);
    case CSSPropertyOpacity:
        return cssValuePool().createValue(style.opacity(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyOrphans:
        return cssValuePool().createValue(style.orphans(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyOutlineColor:
        return allowVisitedStyle ? cssValuePool().createColorValue(style.visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style, style.outlineColor());
    case CSSPropertyOutlineOffset:
        return zoomAdjustedPixelValue(style.outlineOffset(), style);
    case CSSPropertyOutlineStyle:
        if (style.outlineStyleIsAuto())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        return cssValuePool().createValue(style.outlineStyle());
    case CSSPropertyOutlineWidth:
        return zoomAdjustedPixelValue(style.outlineWidth(), style);
    case CSSPropertyOverflow:
        return cssValuePool().createValue(max(style.overflowX(), style.overflowY()));
    case CSSPropertyOverflowWrap:
        return cssValuePool().createValue(style.overflowWrap());
    case CSSPropertyOverflowX:
        return cssValuePool().createValue(style.overflowX());
    case CSSPropertyOverflowY:
        return cssValuePool().createValue(style.overflowY());
    case CSSPropertyPaddingTop: {
        Length paddingTop = style.paddingTop();
        if (paddingTop.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(paddingTop, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->computedCSSPaddingTop(), style);
    }
    case CSSPropertyPaddingRight: {
        Length paddingRight = style.paddingRight();
        if (paddingRight.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(paddingRight, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->computedCSSPaddingRight(), style);
    }
    case CSSPropertyPaddingBottom: {
        Length paddingBottom = style.paddingBottom();
        if (paddingBottom.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(paddingBottom, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->computedCSSPaddingBottom(), style);
    }
    case CSSPropertyPaddingLeft: {
        Length paddingLeft = style.paddingLeft();
        if (paddingLeft.isFixed() || !layoutObject || !layoutObject->isBox())
            return zoomAdjustedPixelValueForLength(paddingLeft, style);
        return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->computedCSSPaddingLeft(), style);
    }
    case CSSPropertyBreakAfter:
        return cssValuePool().createValue(style.breakAfter());
    case CSSPropertyBreakBefore:
        return cssValuePool().createValue(style.breakBefore());
    case CSSPropertyBreakInside:
        return cssValuePool().createValue(style.breakInside());
    case CSSPropertyPageBreakAfter:
        return cssValuePool().createValue(mapToPageBreakValue(style.breakAfter()));
    case CSSPropertyPageBreakBefore:
        return cssValuePool().createValue(mapToPageBreakValue(style.breakBefore()));
    case CSSPropertyPageBreakInside:
        return cssValuePool().createValue(mapToPageBreakValue(style.breakInside()));
    case CSSPropertyPosition:
        return cssValuePool().createValue(style.position());
    case CSSPropertyQuotes:
        if (!style.quotes()) {
            // TODO(ramya.v): We should return the quote values that we're actually using.
            return nullptr;
        }
        if (style.quotes()->size()) {
            CSSValueList* list = CSSValueList::createSpaceSeparated();
            for (int i = 0; i < style.quotes()->size(); i++) {
                list->append(CSSStringValue::create(style.quotes()->getOpenQuote(i)));
                list->append(CSSStringValue::create(style.quotes()->getCloseQuote(i)));
            }
            return list;
        }
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyRight:
        return valueForPositionOffset(style, CSSPropertyRight, layoutObject);
    case CSSPropertyWebkitRubyPosition:
        return cssValuePool().createValue(style.getRubyPosition());
    case CSSPropertyScrollBehavior:
        return cssValuePool().createValue(style.getScrollBehavior());
    case CSSPropertyTableLayout:
        return cssValuePool().createValue(style.tableLayout());
    case CSSPropertyTextAlign:
        return cssValuePool().createValue(style.textAlign());
    case CSSPropertyTextAlignLast:
        return cssValuePool().createValue(style.getTextAlignLast());
    case CSSPropertyTextDecoration:
        if (RuntimeEnabledFeatures::css3TextDecorationsEnabled())
            return valuesForShorthandProperty(textDecorationShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
        // Fall through.
    case CSSPropertyTextDecorationLine:
        return renderTextDecorationFlagsToCSSValue(style.getTextDecoration());
    case CSSPropertyTextDecorationStyle:
        return valueForTextDecorationStyle(style.getTextDecorationStyle());
    case CSSPropertyTextDecorationColor:
        return currentColorOrValidColor(style, style.textDecorationColor());
    case CSSPropertyTextJustify:
        return cssValuePool().createValue(style.getTextJustify());
    case CSSPropertyTextUnderlinePosition:
        return cssValuePool().createValue(style.getTextUnderlinePosition());
    case CSSPropertyWebkitTextDecorationsInEffect:
        return renderTextDecorationFlagsToCSSValue(style.textDecorationsInEffect());
    case CSSPropertyWebkitTextFillColor:
        return currentColorOrValidColor(style, style.textFillColor());
    case CSSPropertyWebkitTextEmphasisColor:
        return currentColorOrValidColor(style, style.textEmphasisColor());
    case CSSPropertyWebkitTextEmphasisPosition:
        return cssValuePool().createValue(style.getTextEmphasisPosition());
    case CSSPropertyWebkitTextEmphasisStyle:
        switch (style.getTextEmphasisMark()) {
        case TextEmphasisMarkNone:
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case TextEmphasisMarkCustom:
            return CSSStringValue::create(style.textEmphasisCustomMark());
        case TextEmphasisMarkAuto:
            ASSERT_NOT_REACHED();
            // Fall through
        case TextEmphasisMarkDot:
        case TextEmphasisMarkCircle:
        case TextEmphasisMarkDoubleCircle:
        case TextEmphasisMarkTriangle:
        case TextEmphasisMarkSesame: {
            CSSValueList* list = CSSValueList::createSpaceSeparated();
            list->append(cssValuePool().createValue(style.getTextEmphasisFill()));
            list->append(cssValuePool().createValue(style.getTextEmphasisMark()));
            return list;
        }
        }
    case CSSPropertyTextIndent: {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        list->append(zoomAdjustedPixelValueForLength(style.textIndent(), style));
        if (RuntimeEnabledFeatures::css3TextEnabled() && (style.getTextIndentLine() == TextIndentEachLine || style.getTextIndentType() == TextIndentHanging)) {
            if (style.getTextIndentLine() == TextIndentEachLine)
                list->append(cssValuePool().createIdentifierValue(CSSValueEachLine));
            if (style.getTextIndentType() == TextIndentHanging)
                list->append(cssValuePool().createIdentifierValue(CSSValueHanging));
        }
        return list;
    }
    case CSSPropertyTextShadow:
        return valueForShadowList(style.textShadow(), style, false);
    case CSSPropertyTextRendering:
        return cssValuePool().createValue(style.getFontDescription().textRendering());
    case CSSPropertyTextOverflow:
        if (style.getTextOverflow())
            return cssValuePool().createIdentifierValue(CSSValueEllipsis);
        return cssValuePool().createIdentifierValue(CSSValueClip);
    case CSSPropertyWebkitTextSecurity:
        return cssValuePool().createValue(style.textSecurity());
    case CSSPropertyWebkitTextStrokeColor:
        return currentColorOrValidColor(style, style.textStrokeColor());
    case CSSPropertyWebkitTextStrokeWidth:
        return zoomAdjustedPixelValue(style.textStrokeWidth(), style);
    case CSSPropertyTextTransform:
        return cssValuePool().createValue(style.textTransform());
    case CSSPropertyTop:
        return valueForPositionOffset(style, CSSPropertyTop, layoutObject);
    case CSSPropertyTouchAction:
        return touchActionFlagsToCSSValue(style.getTouchAction());
    case CSSPropertyUnicodeBidi:
        return cssValuePool().createValue(style.unicodeBidi());
    case CSSPropertyVerticalAlign:
        switch (style.verticalAlign()) {
        case VerticalAlignBaseline:
            return cssValuePool().createIdentifierValue(CSSValueBaseline);
        case VerticalAlignMiddle:
            return cssValuePool().createIdentifierValue(CSSValueMiddle);
        case VerticalAlignSub:
            return cssValuePool().createIdentifierValue(CSSValueSub);
        case VerticalAlignSuper:
            return cssValuePool().createIdentifierValue(CSSValueSuper);
        case VerticalAlignTextTop:
            return cssValuePool().createIdentifierValue(CSSValueTextTop);
        case VerticalAlignTextBottom:
            return cssValuePool().createIdentifierValue(CSSValueTextBottom);
        case VerticalAlignTop:
            return cssValuePool().createIdentifierValue(CSSValueTop);
        case VerticalAlignBottom:
            return cssValuePool().createIdentifierValue(CSSValueBottom);
        case VerticalAlignBaselineMiddle:
            return cssValuePool().createIdentifierValue(CSSValueWebkitBaselineMiddle);
        case VerticalAlignLength:
            return zoomAdjustedPixelValueForLength(style.getVerticalAlignLength(), style);
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    case CSSPropertyVisibility:
        return cssValuePool().createValue(style.visibility());
    case CSSPropertyWhiteSpace:
        return cssValuePool().createValue(style.whiteSpace());
    case CSSPropertyWidows:
        return cssValuePool().createValue(style.widows(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWidth:
        if (layoutObject) {
            // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
            // the "width" property does not apply for non-atomic inline elements.
            if (!layoutObject->isAtomicInlineLevel() && layoutObject->isInline())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(sizingBox(layoutObject).width(), style);
        }
        return zoomAdjustedPixelValueForLength(style.width(), style);
    case CSSPropertyWillChange:
        return valueForWillChange(style.willChangeProperties(), style.willChangeContents(), style.willChangeScrollPosition());
    case CSSPropertyWordBreak:
        return cssValuePool().createValue(style.wordBreak());
    case CSSPropertyWordSpacing:
        return zoomAdjustedPixelValue(style.wordSpacing(), style);
    case CSSPropertyWordWrap:
        return cssValuePool().createValue(style.overflowWrap());
    case CSSPropertyWebkitLineBreak:
        return cssValuePool().createValue(style.getLineBreak());
    case CSSPropertyResize:
        return cssValuePool().createValue(style.resize());
    case CSSPropertyFontKerning:
        return cssValuePool().createValue(style.getFontDescription().getKerning());
    case CSSPropertyWebkitFontSmoothing:
        return cssValuePool().createValue(style.getFontDescription().fontSmoothing());
    case CSSPropertyFontVariantLigatures: {
        FontDescription::LigaturesState commonLigaturesState = style.getFontDescription().commonLigaturesState();
        FontDescription::LigaturesState discretionaryLigaturesState = style.getFontDescription().discretionaryLigaturesState();
        FontDescription::LigaturesState historicalLigaturesState = style.getFontDescription().historicalLigaturesState();
        FontDescription::LigaturesState contextualLigaturesState = style.getFontDescription().contextualLigaturesState();
        if (commonLigaturesState == FontDescription::NormalLigaturesState && discretionaryLigaturesState == FontDescription::NormalLigaturesState
            && historicalLigaturesState == FontDescription::NormalLigaturesState && contextualLigaturesState == FontDescription::NormalLigaturesState)
            return cssValuePool().createIdentifierValue(CSSValueNormal);

        CSSValueList* valueList = CSSValueList::createSpaceSeparated();
        if (commonLigaturesState != FontDescription::NormalLigaturesState)
            valueList->append(cssValuePool().createIdentifierValue(commonLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoCommonLigatures : CSSValueCommonLigatures));
        if (discretionaryLigaturesState != FontDescription::NormalLigaturesState)
            valueList->append(cssValuePool().createIdentifierValue(discretionaryLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoDiscretionaryLigatures : CSSValueDiscretionaryLigatures));
        if (historicalLigaturesState != FontDescription::NormalLigaturesState)
            valueList->append(cssValuePool().createIdentifierValue(historicalLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoHistoricalLigatures : CSSValueHistoricalLigatures));
        if (contextualLigaturesState != FontDescription::NormalLigaturesState)
            valueList->append(cssValuePool().createIdentifierValue(contextualLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoContextual : CSSValueContextual));
        return valueList;
    }
    case CSSPropertyFontVariantCaps: {
        FontDescription::FontVariantCaps variantCaps = style.getFontDescription().variantCaps();
        switch (variantCaps) {
        case FontDescription::CapsNormal:
            return cssValuePool().createIdentifierValue(CSSValueNormal);
        case FontDescription::SmallCaps:
            return cssValuePool().createIdentifierValue(CSSValueSmallCaps);
        case FontDescription::AllSmallCaps:
            return cssValuePool().createIdentifierValue(CSSValueAllSmallCaps);
        case FontDescription::PetiteCaps:
            return cssValuePool().createIdentifierValue(CSSValuePetiteCaps);
        case FontDescription::AllPetiteCaps:
            return cssValuePool().createIdentifierValue(CSSValueAllPetiteCaps);
        case FontDescription::Unicase:
            return cssValuePool().createIdentifierValue(CSSValueUnicase);
        case FontDescription::TitlingCaps:
            return cssValuePool().createIdentifierValue(CSSValueTitlingCaps);
        }
    }
    case CSSPropertyZIndex:
        if (style.hasAutoZIndex())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        return cssValuePool().createValue(style.zIndex(), CSSPrimitiveValue::UnitType::Integer);
    case CSSPropertyZoom:
        return cssValuePool().createValue(style.zoom(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyBoxSizing:
        if (style.boxSizing() == BoxSizingContentBox)
            return cssValuePool().createIdentifierValue(CSSValueContentBox);
        return cssValuePool().createIdentifierValue(CSSValueBorderBox);
    case CSSPropertyWebkitAppRegion:
        return cssValuePool().createIdentifierValue(style.getDraggableRegionMode() == DraggableRegionDrag ? CSSValueDrag : CSSValueNoDrag);
    case CSSPropertyAnimationDelay:
        return valueForAnimationDelay(style.animations());
    case CSSPropertyAnimationDirection: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const CSSAnimationData* animationData = style.animations();
        if (animationData) {
            for (size_t i = 0; i < animationData->directionList().size(); ++i)
                list->append(valueForAnimationDirection(animationData->directionList()[i]));
        } else {
            list->append(cssValuePool().createIdentifierValue(CSSValueNormal));
        }
        return list;
    }
    case CSSPropertyAnimationDuration:
        return valueForAnimationDuration(style.animations());
    case CSSPropertyAnimationFillMode: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const CSSAnimationData* animationData = style.animations();
        if (animationData) {
            for (size_t i = 0; i < animationData->fillModeList().size(); ++i)
                list->append(valueForAnimationFillMode(animationData->fillModeList()[i]));
        } else {
            list->append(cssValuePool().createIdentifierValue(CSSValueNone));
        }
        return list;
    }
    case CSSPropertyAnimationIterationCount: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const CSSAnimationData* animationData = style.animations();
        if (animationData) {
            for (size_t i = 0; i < animationData->iterationCountList().size(); ++i)
                list->append(valueForAnimationIterationCount(animationData->iterationCountList()[i]));
        } else {
            list->append(cssValuePool().createValue(CSSAnimationData::initialIterationCount(), CSSPrimitiveValue::UnitType::Number));
        }
        return list;
    }
    case CSSPropertyAnimationName: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const CSSAnimationData* animationData = style.animations();
        if (animationData) {
            for (size_t i = 0; i < animationData->nameList().size(); ++i)
                list->append(CSSCustomIdentValue::create(animationData->nameList()[i]));
        } else {
            list->append(cssValuePool().createIdentifierValue(CSSValueNone));
        }
        return list;
    }
    case CSSPropertyAnimationPlayState: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        const CSSAnimationData* animationData = style.animations();
        if (animationData) {
            for (size_t i = 0; i < animationData->playStateList().size(); ++i)
                list->append(valueForAnimationPlayState(animationData->playStateList()[i]));
        } else {
            list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
        }
        return list;
    }
    case CSSPropertyAnimationTimingFunction:
        return valueForAnimationTimingFunction(style.animations());
    case CSSPropertyAnimation: {
        const CSSAnimationData* animationData = style.animations();
        if (animationData) {
            CSSValueList* animationsList = CSSValueList::createCommaSeparated();
            for (size_t i = 0; i < animationData->nameList().size(); ++i) {
                CSSValueList* list = CSSValueList::createSpaceSeparated();
                list->append(CSSCustomIdentValue::create(animationData->nameList()[i]));
                list->append(cssValuePool().createValue(CSSTimingData::getRepeated(animationData->durationList(), i), CSSPrimitiveValue::UnitType::Seconds));
                list->append(createTimingFunctionValue(CSSTimingData::getRepeated(animationData->timingFunctionList(), i).get()));
                list->append(cssValuePool().createValue(CSSTimingData::getRepeated(animationData->delayList(), i), CSSPrimitiveValue::UnitType::Seconds));
                list->append(valueForAnimationIterationCount(CSSTimingData::getRepeated(animationData->iterationCountList(), i)));
                list->append(valueForAnimationDirection(CSSTimingData::getRepeated(animationData->directionList(), i)));
                list->append(valueForAnimationFillMode(CSSTimingData::getRepeated(animationData->fillModeList(), i)));
                list->append(valueForAnimationPlayState(CSSTimingData::getRepeated(animationData->playStateList(), i)));
                animationsList->append(list);
            }
            return animationsList;
        }

        CSSValueList* list = CSSValueList::createSpaceSeparated();
        // animation-name default value.
        list->append(cssValuePool().createIdentifierValue(CSSValueNone));
        list->append(cssValuePool().createValue(CSSAnimationData::initialDuration(), CSSPrimitiveValue::UnitType::Seconds));
        list->append(createTimingFunctionValue(CSSAnimationData::initialTimingFunction().get()));
        list->append(cssValuePool().createValue(CSSAnimationData::initialDelay(), CSSPrimitiveValue::UnitType::Seconds));
        list->append(cssValuePool().createValue(CSSAnimationData::initialIterationCount(), CSSPrimitiveValue::UnitType::Number));
        list->append(valueForAnimationDirection(CSSAnimationData::initialDirection()));
        list->append(valueForAnimationFillMode(CSSAnimationData::initialFillMode()));
        // Initial animation-play-state.
        list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
        return list;
    }
    case CSSPropertyWebkitAppearance:
        return cssValuePool().createValue(style.appearance());
    case CSSPropertyBackfaceVisibility:
        return cssValuePool().createIdentifierValue((style.backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
    case CSSPropertyWebkitBorderImage:
        return valueForNinePieceImage(style.borderImage(), style);
    case CSSPropertyBorderImageOutset:
        return valueForNinePieceImageQuad(style.borderImage().outset(), style);
    case CSSPropertyBorderImageRepeat:
        return valueForNinePieceImageRepeat(style.borderImage());
    case CSSPropertyBorderImageSlice:
        return valueForNinePieceImageSlice(style.borderImage());
    case CSSPropertyBorderImageWidth:
        return valueForNinePieceImageQuad(style.borderImage().borderSlices(), style);
    case CSSPropertyWebkitMaskBoxImage:
        return valueForNinePieceImage(style.maskBoxImage(), style);
    case CSSPropertyWebkitMaskBoxImageOutset:
        return valueForNinePieceImageQuad(style.maskBoxImage().outset(), style);
    case CSSPropertyWebkitMaskBoxImageRepeat:
        return valueForNinePieceImageRepeat(style.maskBoxImage());
    case CSSPropertyWebkitMaskBoxImageSlice:
        return valueForNinePieceImageSlice(style.maskBoxImage());
    case CSSPropertyWebkitMaskBoxImageWidth:
        return valueForNinePieceImageQuad(style.maskBoxImage().borderSlices(), style);
    case CSSPropertyWebkitMaskBoxImageSource:
        if (style.maskBoxImageSource())
            return style.maskBoxImageSource()->computedCSSValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyWebkitFontSizeDelta:
        // Not a real style property -- used by the editing engine -- so has no computed value.
        return nullptr;
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
        return cssValuePool().createValue(style.marginAfterCollapse());
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
        return cssValuePool().createValue(style.marginBeforeCollapse());
    case CSSPropertyPerspective:
        if (!style.hasPerspective())
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return zoomAdjustedPixelValue(style.perspective(), style);
    case CSSPropertyPerspectiveOrigin: {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        if (layoutObject) {
            LayoutRect box;
            if (layoutObject->isBox())
                box = toLayoutBox(layoutObject)->borderBoxRect();

            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.perspectiveOriginX(), box.width()), style));
            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.perspectiveOriginY(), box.height()), style));
        } else {
            list->append(zoomAdjustedPixelValueForLength(style.perspectiveOriginX(), style));
            list->append(zoomAdjustedPixelValueForLength(style.perspectiveOriginY(), style));
        }
        return list;
    }
    case CSSPropertyWebkitRtlOrdering:
        return cssValuePool().createIdentifierValue(style.rtlOrdering() ? CSSValueVisual : CSSValueLogical);
    case CSSPropertyWebkitTapHighlightColor:
        return currentColorOrValidColor(style, style.tapHighlightColor());
    case CSSPropertyWebkitUserDrag:
        return cssValuePool().createValue(style.userDrag());
    case CSSPropertyWebkitUserSelect:
        return cssValuePool().createValue(style.userSelect());
    case CSSPropertyBorderBottomLeftRadius:
        return valueForBorderRadiusCorner(style.borderBottomLeftRadius(), style);
    case CSSPropertyBorderBottomRightRadius:
        return valueForBorderRadiusCorner(style.borderBottomRightRadius(), style);
    case CSSPropertyBorderTopLeftRadius:
        return valueForBorderRadiusCorner(style.borderTopLeftRadius(), style);
    case CSSPropertyBorderTopRightRadius:
        return valueForBorderRadiusCorner(style.borderTopRightRadius(), style);
    case CSSPropertyClip: {
        if (style.hasAutoClip())
            return cssValuePool().createIdentifierValue(CSSValueAuto);
        CSSPrimitiveValue* top = style.clip().top().isAuto()
            ? cssValuePool().createIdentifierValue(CSSValueAuto)
            : zoomAdjustedPixelValue(style.clip().top().value(), style);
        CSSPrimitiveValue* right = style.clip().right().isAuto()
            ? cssValuePool().createIdentifierValue(CSSValueAuto)
            : zoomAdjustedPixelValue(style.clip().right().value(), style);
        CSSPrimitiveValue* bottom = style.clip().bottom().isAuto()
            ? cssValuePool().createIdentifierValue(CSSValueAuto)
            : zoomAdjustedPixelValue(style.clip().bottom().value(), style);
        CSSPrimitiveValue* left = style.clip().left().isAuto()
            ? cssValuePool().createIdentifierValue(CSSValueAuto)
            : zoomAdjustedPixelValue(style.clip().left().value(), style);
        return CSSQuadValue::create(top, right, bottom, left, CSSQuadValue::SerializeAsRect);
    }
    case CSSPropertySpeak:
        return cssValuePool().createValue(style.speak());
    case CSSPropertyTransform:
        return computedTransform(layoutObject, style);
    case CSSPropertyTransformOrigin: {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        if (layoutObject) {
            LayoutRect box;
            if (layoutObject->isBox())
                box = toLayoutBox(layoutObject)->borderBoxRect();

            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.transformOriginX(), box.width()), style));
            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.transformOriginY(), box.height()), style));
            if (style.transformOriginZ() != 0)
                list->append(zoomAdjustedPixelValue(style.transformOriginZ(), style));
        } else {
            list->append(zoomAdjustedPixelValueForLength(style.transformOriginX(), style));
            list->append(zoomAdjustedPixelValueForLength(style.transformOriginY(), style));
            if (style.transformOriginZ() != 0)
                list->append(zoomAdjustedPixelValue(style.transformOriginZ(), style));
        }
        return list;
    }
    case CSSPropertyTransformStyle:
        return cssValuePool().createIdentifierValue((style.transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
    case CSSPropertyTransitionDelay:
        return valueForAnimationDelay(style.transitions());
    case CSSPropertyTransitionDuration:
        return valueForAnimationDuration(style.transitions());
    case CSSPropertyTransitionProperty:
        return valueForTransitionProperty(style.transitions());
    case CSSPropertyTransitionTimingFunction:
        return valueForAnimationTimingFunction(style.transitions());
    case CSSPropertyTransition: {
        const CSSTransitionData* transitionData = style.transitions();
        if (transitionData) {
            CSSValueList* transitionsList = CSSValueList::createCommaSeparated();
            for (size_t i = 0; i < transitionData->propertyList().size(); ++i) {
                CSSValueList* list = CSSValueList::createSpaceSeparated();
                list->append(createTransitionPropertyValue(transitionData->propertyList()[i]));
                list->append(cssValuePool().createValue(CSSTimingData::getRepeated(transitionData->durationList(), i), CSSPrimitiveValue::UnitType::Seconds));
                list->append(createTimingFunctionValue(CSSTimingData::getRepeated(transitionData->timingFunctionList(), i).get()));
                list->append(cssValuePool().createValue(CSSTimingData::getRepeated(transitionData->delayList(), i), CSSPrimitiveValue::UnitType::Seconds));
                transitionsList->append(list);
            }
            return transitionsList;
        }

        CSSValueList* list = CSSValueList::createSpaceSeparated();
        // transition-property default value.
        list->append(cssValuePool().createIdentifierValue(CSSValueAll));
        list->append(cssValuePool().createValue(CSSTransitionData::initialDuration(), CSSPrimitiveValue::UnitType::Seconds));
        list->append(createTimingFunctionValue(CSSTransitionData::initialTimingFunction().get()));
        list->append(cssValuePool().createValue(CSSTransitionData::initialDelay(), CSSPrimitiveValue::UnitType::Seconds));
        return list;
    }
    case CSSPropertyPointerEvents:
        return cssValuePool().createValue(style.pointerEvents());
    case CSSPropertyWritingMode:
    case CSSPropertyWebkitWritingMode:
        return cssValuePool().createValue(style.getWritingMode());
    case CSSPropertyWebkitTextCombine:
        if (style.getTextCombine() == TextCombineAll)
            return CSSPrimitiveValue::createIdentifier(CSSValueHorizontal);
    case CSSPropertyTextCombineUpright:
        return cssValuePool().createValue(style.getTextCombine());
    case CSSPropertyWebkitTextOrientation:
        if (style.getTextOrientation() == TextOrientationMixed)
            return CSSPrimitiveValue::createIdentifier(CSSValueVerticalRight);
    case CSSPropertyTextOrientation:
        return CSSPrimitiveValue::create(style.getTextOrientation());
    case CSSPropertyContent:
        if (style.styleType() == PseudoIdNone)
            return cssValuePool().createIdentifierValue(CSSValueNormal);
        if (!style.hasContent() && (style.styleType() == PseudoIdBefore || style.styleType() == PseudoIdAfter))
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return valueForContentData(style);
    case CSSPropertyCounterIncrement:
        return valueForCounterDirectives(style, propertyID);
    case CSSPropertyCounterReset:
        return valueForCounterDirectives(style, propertyID);
    case CSSPropertyWebkitClipPath:
        if (ClipPathOperation* operation = style.clipPath()) {
            if (operation->type() == ClipPathOperation::SHAPE)
                return valueForBasicShape(style, toShapeClipPathOperation(operation)->basicShape());
            if (operation->type() == ClipPathOperation::REFERENCE)
                return CSSURIValue::create(toReferenceClipPathOperation(operation)->url());
        }
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyShapeMargin:
        return cssValuePool().createValue(style.shapeMargin(), style);
    case CSSPropertyShapeImageThreshold:
        return cssValuePool().createValue(style.shapeImageThreshold(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyShapeOutside:
        return valueForShape(style, style.shapeOutside());
    case CSSPropertyWebkitFilter:
        return valueForFilter(style, style.filter());
    case CSSPropertyBackdropFilter:
        return valueForFilter(style, style.backdropFilter());
    case CSSPropertyMixBlendMode:
        return cssValuePool().createValue(style.blendMode());

    case CSSPropertyBackgroundBlendMode: {
        CSSValueList* list = CSSValueList::createCommaSeparated();
        for (const FillLayer* currLayer = &style.backgroundLayers(); currLayer; currLayer = currLayer->next())
            list->append(cssValuePool().createValue(currLayer->blendMode()));
        return list;
    }
    case CSSPropertyBackground:
        return valuesForBackgroundShorthand(style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorder: {
        CSSValue* value = get(CSSPropertyBorderTop, style, layoutObject, styledNode, allowVisitedStyle);
        const CSSPropertyID properties[] = {
            CSSPropertyBorderRight,
            CSSPropertyBorderBottom,
            CSSPropertyBorderLeft
        };
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
            if (!compareCSSValuePtr<CSSValue>(value, get(properties[i], style, layoutObject, styledNode, allowVisitedStyle)))
                return nullptr;
        }
        return value;
    }
    case CSSPropertyBorderBottom:
        return valuesForShorthandProperty(borderBottomShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorderColor:
        return valuesForSidesShorthand(borderColorShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorderLeft:
        return valuesForShorthandProperty(borderLeftShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorderImage:
        return valueForNinePieceImage(style.borderImage(), style);
    case CSSPropertyBorderRadius:
        return valueForBorderRadiusShorthand(style);
    case CSSPropertyBorderRight:
        return valuesForShorthandProperty(borderRightShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorderStyle:
        return valuesForSidesShorthand(borderStyleShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorderTop:
        return valuesForShorthandProperty(borderTopShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyBorderWidth:
        return valuesForSidesShorthand(borderWidthShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyColumnRule:
        return valuesForShorthandProperty(columnRuleShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyColumns:
        return valuesForShorthandProperty(columnsShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyListStyle:
        return valuesForShorthandProperty(listStyleShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyMargin:
        return valuesForSidesShorthand(marginShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyOutline:
        return valuesForShorthandProperty(outlineShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    case CSSPropertyPadding:
        return valuesForSidesShorthand(paddingShorthand(), style, layoutObject, styledNode, allowVisitedStyle);
    // Individual properties not part of the spec.
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
        return nullptr;

    case CSSPropertyMotion:
        return valuesForShorthandProperty(motionShorthand(), style, layoutObject, styledNode, allowVisitedStyle);

    case CSSPropertyMotionPath:
        if (const StylePath* styleMotionPath = style.motionPath())
            return styleMotionPath->computedCSSValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);

    case CSSPropertyMotionOffset:
        return zoomAdjustedPixelValueForLength(style.motionOffset(), style);

    case CSSPropertyMotionRotation: {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        if (style.motionRotation().type == MotionRotationAuto)
            list->append(cssValuePool().createIdentifierValue(CSSValueAuto));
        list->append(cssValuePool().createValue(style.motionRotation().angle, CSSPrimitiveValue::UnitType::Degrees));
        return list;
    }

    // Unimplemented CSS 3 properties (including CSS3 shorthand properties).
    case CSSPropertyWebkitTextEmphasis:
        return nullptr;

    // Directional properties are resolved by resolveDirectionAwareProperty() before the switch.
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
        ASSERT_NOT_REACHED();
        return nullptr;

    // Unimplemented @font-face properties.
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
        return nullptr;

    // Other unimplemented properties.
    case CSSPropertyPage: // for @page
    case CSSPropertySize: // for @page
        return nullptr;

    // Unimplemented -webkit- properties.
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
        return nullptr;

    // @viewport rule properties.
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
        return nullptr;

    // SVG properties.
    case CSSPropertyClipRule:
        return CSSPrimitiveValue::create(svgStyle.clipRule());
    case CSSPropertyFloodOpacity:
        return CSSPrimitiveValue::create(svgStyle.floodOpacity(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyStopOpacity:
        return CSSPrimitiveValue::create(svgStyle.stopOpacity(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyColorInterpolation:
        return CSSPrimitiveValue::create(svgStyle.colorInterpolation());
    case CSSPropertyColorInterpolationFilters:
        return CSSPrimitiveValue::create(svgStyle.colorInterpolationFilters());
    case CSSPropertyFillOpacity:
        return CSSPrimitiveValue::create(svgStyle.fillOpacity(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFillRule:
        return CSSPrimitiveValue::create(svgStyle.fillRule());
    case CSSPropertyColorRendering:
        return CSSPrimitiveValue::create(svgStyle.colorRendering());
    case CSSPropertyShapeRendering:
        return CSSPrimitiveValue::create(svgStyle.shapeRendering());
    case CSSPropertyStrokeLinecap:
        return CSSPrimitiveValue::create(svgStyle.capStyle());
    case CSSPropertyStrokeLinejoin:
        return CSSPrimitiveValue::create(svgStyle.joinStyle());
    case CSSPropertyStrokeMiterlimit:
        return CSSPrimitiveValue::create(svgStyle.strokeMiterLimit(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyStrokeOpacity:
        return CSSPrimitiveValue::create(svgStyle.strokeOpacity(), CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyAlignmentBaseline:
        return CSSPrimitiveValue::create(svgStyle.alignmentBaseline());
    case CSSPropertyDominantBaseline:
        return CSSPrimitiveValue::create(svgStyle.dominantBaseline());
    case CSSPropertyTextAnchor:
        return CSSPrimitiveValue::create(svgStyle.textAnchor());
    case CSSPropertyClipPath:
        if (!svgStyle.clipperResource().isEmpty())
            return CSSURIValue::create(serializeAsFragmentIdentifier(svgStyle.clipperResource()));
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyMask:
        if (!svgStyle.maskerResource().isEmpty())
            return CSSURIValue::create(serializeAsFragmentIdentifier(svgStyle.maskerResource()));
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyFilter:
        if (!svgStyle.filterResource().isEmpty())
            return CSSURIValue::create(serializeAsFragmentIdentifier(svgStyle.filterResource()));
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyFloodColor:
        return currentColorOrValidColor(style, svgStyle.floodColor());
    case CSSPropertyLightingColor:
        return currentColorOrValidColor(style, svgStyle.lightingColor());
    case CSSPropertyStopColor:
        return currentColorOrValidColor(style, svgStyle.stopColor());
    case CSSPropertyFill:
        return adjustSVGPaintForCurrentColor(svgStyle.fillPaintType(), svgStyle.fillPaintUri(), svgStyle.fillPaintColor(), style.color());
    case CSSPropertyMarkerEnd:
        if (!svgStyle.markerEndResource().isEmpty())
            return CSSURIValue::create(serializeAsFragmentIdentifier(svgStyle.markerEndResource()));
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyMarkerMid:
        if (!svgStyle.markerMidResource().isEmpty())
            return CSSURIValue::create(serializeAsFragmentIdentifier(svgStyle.markerMidResource()));
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyMarkerStart:
        if (!svgStyle.markerStartResource().isEmpty())
            return CSSURIValue::create(serializeAsFragmentIdentifier(svgStyle.markerStartResource()));
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyStroke:
        return adjustSVGPaintForCurrentColor(svgStyle.strokePaintType(), svgStyle.strokePaintUri(), svgStyle.strokePaintColor(), style.color());
    case CSSPropertyStrokeDasharray:
        return strokeDashArrayToCSSValueList(*svgStyle.strokeDashArray(), style);
    case CSSPropertyStrokeDashoffset:
        return zoomAdjustedPixelValueForLength(svgStyle.strokeDashOffset(), style);
    case CSSPropertyStrokeWidth:
        return pixelValueForUnzoomedLength(svgStyle.strokeWidth(), style);
    case CSSPropertyBaselineShift: {
        switch (svgStyle.baselineShift()) {
        case BS_SUPER:
            return CSSPrimitiveValue::createIdentifier(CSSValueSuper);
        case BS_SUB:
            return CSSPrimitiveValue::createIdentifier(CSSValueSub);
        case BS_LENGTH:
            return zoomAdjustedPixelValueForLength(svgStyle.baselineShiftValue(), style);
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    case CSSPropertyBufferedRendering:
        return CSSPrimitiveValue::create(svgStyle.bufferedRendering());
    case CSSPropertyPaintOrder:
        return paintOrderToCSSValueList(svgStyle);
    case CSSPropertyVectorEffect:
        return CSSPrimitiveValue::create(svgStyle.vectorEffect());
    case CSSPropertyMaskType:
        return CSSPrimitiveValue::create(svgStyle.maskType());
    case CSSPropertyMarker:
        // the above properties are not yet implemented in the engine
        return nullptr;
    case CSSPropertyD:
        if (const StylePath* stylePath = svgStyle.d())
            return stylePath->computedCSSValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case CSSPropertyCx:
        return zoomAdjustedPixelValueForLength(svgStyle.cx(), style);
    case CSSPropertyCy:
        return zoomAdjustedPixelValueForLength(svgStyle.cy(), style);
    case CSSPropertyX:
        return zoomAdjustedPixelValueForLength(svgStyle.x(), style);
    case CSSPropertyY:
        return zoomAdjustedPixelValueForLength(svgStyle.y(), style);
    case CSSPropertyR:
        return zoomAdjustedPixelValueForLength(svgStyle.r(), style);
    case CSSPropertyRx:
        return zoomAdjustedPixelValueForLength(svgStyle.rx(), style);
    case CSSPropertyRy:
        return zoomAdjustedPixelValueForLength(svgStyle.ry(), style);
    case CSSPropertyScrollSnapType:
        return cssValuePool().createValue(style.getScrollSnapType());
    case CSSPropertyScrollSnapPointsX:
        return valueForScrollSnapPoints(style.scrollSnapPointsX(), style);
    case CSSPropertyScrollSnapPointsY:
        return valueForScrollSnapPoints(style.scrollSnapPointsY(), style);
    case CSSPropertyScrollSnapCoordinate:
        return valueForScrollSnapCoordinate(style.scrollSnapCoordinate(), style);
    case CSSPropertyScrollSnapDestination:
        return valueForScrollSnapDestination(style.scrollSnapDestination(), style);
    case CSSPropertyTranslate: {
        if (!style.translate())
            return cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Pixels);

        CSSValueList* list = CSSValueList::createSpaceSeparated();
        if (layoutObject && layoutObject->isBox()) {
            LayoutRect box = toLayoutBox(layoutObject)->borderBoxRect();
            list->append(zoomAdjustedPixelValue(floatValueForLength(style.translate()->x(), box.width().toFloat()), style));

            if (!style.translate()->y().isZero() || style.translate()->z() != 0)
                list->append(zoomAdjustedPixelValue(floatValueForLength(style.translate()->y(), box.height().toFloat()), style));

        } else {
            // No box to resolve the percentage values
            list->append(zoomAdjustedPixelValueForLength(style.translate()->x(), style));

            if (!style.translate()->y().isZero() || style.translate()->z() != 0)
                list->append(zoomAdjustedPixelValueForLength(style.translate()->y(), style));
        }

        if (style.translate()->z() != 0)
            list->append(zoomAdjustedPixelValue(style.translate()->z(), style));

        return list;
    }
    case CSSPropertyRotate: {
        if (!style.rotate())
            return cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Degrees);

        CSSValueList* list = CSSValueList::createSpaceSeparated();
        list->append(cssValuePool().createValue(style.rotate()->angle(), CSSPrimitiveValue::UnitType::Degrees));
        if (style.rotate()->x() != 0 || style.rotate()->y() != 0 || style.rotate()->z() != 1) {
            list->append(cssValuePool().createValue(style.rotate()->x(), CSSPrimitiveValue::UnitType::Number));
            list->append(cssValuePool().createValue(style.rotate()->y(), CSSPrimitiveValue::UnitType::Number));
            list->append(cssValuePool().createValue(style.rotate()->z(), CSSPrimitiveValue::UnitType::Number));
        }
        return list;
    }
    case CSSPropertyScale: {
        if (!style.scale())
            return cssValuePool().createValue(1, CSSPrimitiveValue::UnitType::Number);
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        list->append(cssValuePool().createValue(style.scale()->x(), CSSPrimitiveValue::UnitType::Number));
        if (style.scale()->y() == 1 && style.scale()->z() == 1)
            return list;
        list->append(cssValuePool().createValue(style.scale()->y(), CSSPrimitiveValue::UnitType::Number));
        if (style.scale()->z() != 1)
            list->append(cssValuePool().createValue(style.scale()->z(), CSSPrimitiveValue::UnitType::Number));
        return list;
    }
    case CSSPropertyContain: {
        if (!style.contain())
            return cssValuePool().createIdentifierValue(CSSValueNone);
        if (style.contain() == ContainsStrict)
            return cssValuePool().createIdentifierValue(CSSValueStrict);
        if (style.contain() == ContainsContent)
            return cssValuePool().createIdentifierValue(CSSValueContent);

        CSSValueList* list = CSSValueList::createSpaceSeparated();
        if (style.containsStyle())
            list->append(cssValuePool().createIdentifierValue(CSSValueStyle));
        if (style.contain() & ContainsLayout)
            list->append(cssValuePool().createIdentifierValue(CSSValueLayout));
        if (style.containsPaint())
            list->append(cssValuePool().createIdentifierValue(CSSValuePaint));
        if (style.containsSize())
            list->append(cssValuePool().createIdentifierValue(CSSValueSize));
        ASSERT(list->length());
        return list;
    }
    case CSSPropertySnapHeight: {
        if (!style.snapHeightUnit())
            return cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Pixels);
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        list->append(cssValuePool().createValue(style.snapHeightUnit(), CSSPrimitiveValue::UnitType::Pixels));
        if (style.snapHeightPosition())
            list->append(cssValuePool().createValue(style.snapHeightPosition(), CSSPrimitiveValue::UnitType::Integer));
        return list;
    }
    case CSSPropertyVariable:
        // Variables are retrieved via get(AtomicString).
        ASSERT_NOT_REACHED();
        return nullptr;
    case CSSPropertyAll:
        return nullptr;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace blink
