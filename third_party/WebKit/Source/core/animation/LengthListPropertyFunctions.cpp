// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthListPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

namespace {

const FillLayer* getFillLayer(CSSPropertyID property, const ComputedStyle& style)
{
    switch (property) {
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
        return &style.backgroundLayers();
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return &style.maskLayers();
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

FillLayer* accessFillLayer(CSSPropertyID property, ComputedStyle& style)
{
    switch (property) {
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
        return &style.accessBackgroundLayers();
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return &style.accessMaskLayers();
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

struct FillLayerMethods {
    FillLayerMethods(CSSPropertyID property)
    {
        switch (property) {
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            isSet = &FillLayer::isXPositionSet;
            get = &FillLayer::xPosition;
            set = &FillLayer::setXPosition;
            clear = &FillLayer::clearXPosition;
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            isSet = &FillLayer::isYPositionSet;
            get = &FillLayer::yPosition;
            set = &FillLayer::setYPosition;
            clear = &FillLayer::clearYPosition;
            break;
        default:
            ASSERT_NOT_REACHED();
            isSet = nullptr;
            get = nullptr;
            set = nullptr;
            clear = nullptr;
            break;
        }
    }

    bool (FillLayer::*isSet)() const;
    const Length& (FillLayer::*get)() const;
    void (FillLayer::*set)(const Length&);
    void (FillLayer::*clear)();
};

} // namespace

ValueRange LengthListPropertyFunctions::valueRange(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyObjectPosition:
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return ValueRangeAll;

    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyStrokeDasharray:
        return ValueRangeNonNegative;

    default:
        ASSERT_NOT_REACHED();
        return ValueRangeAll;
    }
}

Vector<Length> LengthListPropertyFunctions::getInitialLengthList(CSSPropertyID property)
{
    return getLengthList(property, *ComputedStyle::initialStyle());
}

static Vector<Length> toVector(const LengthPoint& point)
{
    Vector<Length> result(2);
    result[0] = point.x();
    result[1] = point.y();
    return result;
}

static Vector<Length> toVector(const LengthSize& size)
{
    Vector<Length> result(2);
    result[0] = size.width();
    result[1] = size.height();
    return result;
}

static Vector<Length> toVector(const TransformOrigin& transformOrigin)
{
    Vector<Length> result(3);
    result[0] = transformOrigin.x();
    result[1] = transformOrigin.y();
    result[2] = Length(transformOrigin.z(), Fixed);
    return result;
}

Vector<Length> LengthListPropertyFunctions::getLengthList(CSSPropertyID property, const ComputedStyle& style)
{
    switch (property) {
    case CSSPropertyStrokeDasharray: {
        if (style.strokeDashArray())
            return style.strokeDashArray()->vector();
        return Vector<Length>();
    }
    case CSSPropertyObjectPosition:
        return toVector(style.objectPosition());
    case CSSPropertyPerspectiveOrigin:
        return toVector(style.perspectiveOrigin());
    case CSSPropertyBorderBottomLeftRadius:
        return toVector(style.borderBottomLeftRadius());
    case CSSPropertyBorderBottomRightRadius:
        return toVector(style.borderBottomRightRadius());
    case CSSPropertyBorderTopLeftRadius:
        return toVector(style.borderTopLeftRadius());
    case CSSPropertyBorderTopRightRadius:
        return toVector(style.borderTopRightRadius());
    case CSSPropertyTransformOrigin:
        return toVector(style.transformOrigin());
    default:
        break;
    }

    Vector<Length> result;
    const FillLayer* fillLayer = getFillLayer(property, style);
    FillLayerMethods fillLayerMethods(property);
    while (fillLayer && (fillLayer->*fillLayerMethods.isSet)()) {
        result.append((fillLayer->*fillLayerMethods.get)());
        fillLayer = fillLayer->next();
    }
    return result;
}

static LengthPoint pointFromVector(const Vector<Length>& list)
{
    ASSERT(list.size() == 2);
    return LengthPoint(list[0], list[1]);
}

static LengthSize sizeFromVector(const Vector<Length>& list)
{
    ASSERT(list.size() == 2);
    return LengthSize(list[0], list[1]);
}

static TransformOrigin transformOriginFromVector(const Vector<Length>& list)
{
    ASSERT(list.size() == 3);
    return TransformOrigin(list[0], list[1], list[2].pixels());
}

void LengthListPropertyFunctions::setLengthList(CSSPropertyID property, ComputedStyle& style, Vector<Length>&& lengthList)
{
    switch (property) {
    case CSSPropertyStrokeDasharray:
        style.setStrokeDashArray(lengthList.isEmpty() ? nullptr : RefVector<Length>::create(std::move(lengthList)));
        return;
    case CSSPropertyObjectPosition:
        style.setObjectPosition(pointFromVector(lengthList));
        return;
    case CSSPropertyPerspectiveOrigin:
        style.setPerspectiveOrigin(pointFromVector(lengthList));
        return;
    case CSSPropertyBorderBottomLeftRadius:
        style.setBorderBottomLeftRadius(sizeFromVector(lengthList));
        return;
    case CSSPropertyBorderBottomRightRadius:
        style.setBorderBottomRightRadius(sizeFromVector(lengthList));
        return;
    case CSSPropertyBorderTopLeftRadius:
        style.setBorderTopLeftRadius(sizeFromVector(lengthList));
        return;
    case CSSPropertyBorderTopRightRadius:
        style.setBorderTopRightRadius(sizeFromVector(lengthList));
        return;
    case CSSPropertyTransformOrigin:
        style.setTransformOrigin(transformOriginFromVector(lengthList));
        return;
    default:
        break;
    }

    FillLayer* fillLayer = accessFillLayer(property, style);
    FillLayer* prev = nullptr;
    FillLayerMethods fillLayerMethods(property);
    for (size_t i = 0; i < lengthList.size(); i++) {
        if (!fillLayer)
            fillLayer = prev->ensureNext();
        (fillLayer->*fillLayerMethods.set)(lengthList[i]);
        prev = fillLayer;
        fillLayer = fillLayer->next();
    }
    while (fillLayer) {
        (fillLayer->*fillLayerMethods.clear)();
        fillLayer = fillLayer->next();
    }
}

} // namespace blink
