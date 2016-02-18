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
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return ValueRangeAll;

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

Vector<Length> LengthListPropertyFunctions::getLengthList(CSSPropertyID property, const ComputedStyle& style)
{
    Vector<Length> result;

    switch (property) {
    case CSSPropertyStrokeDasharray:
        if (style.strokeDashArray())
            result.appendVector(style.strokeDashArray()->vector());
        return result;
    case CSSPropertyObjectPosition:
        result.append(style.objectPosition().x());
        result.append(style.objectPosition().y());
        return result;
    case CSSPropertyPerspectiveOrigin:
        result.append(style.perspectiveOrigin().x());
        result.append(style.perspectiveOrigin().y());
        return result;
    default:
        break;
    }

    const FillLayer* fillLayer = getFillLayer(property, style);
    FillLayerMethods fillLayerMethods(property);
    while (fillLayer && (fillLayer->*fillLayerMethods.isSet)()) {
        result.append((fillLayer->*fillLayerMethods.get)());
        fillLayer = fillLayer->next();
    }
    return result;
}

void LengthListPropertyFunctions::setLengthList(CSSPropertyID property, ComputedStyle& style, Vector<Length>&& lengthList)
{
    switch (property) {
    case CSSPropertyStrokeDasharray:
        style.setStrokeDashArray(lengthList.isEmpty() ? nullptr : RefVector<Length>::create(std::move(lengthList)));
        return;
    case CSSPropertyObjectPosition:
        style.setObjectPosition(LengthPoint(lengthList[0], lengthList[1]));
        return;
    case CSSPropertyPerspectiveOrigin:
        style.setPerspectiveOrigin(LengthPoint(lengthList[0], lengthList[1]));
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
