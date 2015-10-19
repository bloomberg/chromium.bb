// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ColorInterpolationType.h"

#include "core/animation/ColorPropertyFunctions.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/layout/LayoutTheme.h"

namespace blink {

enum InterpolableColorIndex {
    Red,
    Green,
    Blue,
    Alpha,
    Currentcolor,
    WebkitActivelink,
    WebkitLink,
    WebkitText,
    InterpolableColorIndexCount,
};

static PassOwnPtr<InterpolableValue> createInterpolableColorForIndex(InterpolableColorIndex index)
{
    ASSERT(index < InterpolableColorIndexCount);
    OwnPtr<InterpolableList> list = InterpolableList::create(InterpolableColorIndexCount);
    for (int i = 0; i < InterpolableColorIndexCount; i++)
        list->set(i, InterpolableNumber::create(i == index));
    return list.release();
}

PassOwnPtr<InterpolableValue> ColorInterpolationType::createInterpolableColor(const Color& color)
{
    OwnPtr<InterpolableList> list = InterpolableList::create(InterpolableColorIndexCount);
    list->set(Red, InterpolableNumber::create(color.red() * color.alpha()));
    list->set(Green, InterpolableNumber::create(color.green() * color.alpha()));
    list->set(Blue, InterpolableNumber::create(color.blue() * color.alpha()));
    list->set(Alpha, InterpolableNumber::create(color.alpha()));
    list->set(Currentcolor, InterpolableNumber::create(0));
    list->set(WebkitActivelink, InterpolableNumber::create(0));
    list->set(WebkitLink, InterpolableNumber::create(0));
    list->set(WebkitText, InterpolableNumber::create(0));
    return list.release();
}

PassOwnPtr<InterpolableValue> ColorInterpolationType::createInterpolableColor(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueCurrentcolor:
        return createInterpolableColorForIndex(Currentcolor);
    case CSSValueWebkitActivelink:
        return createInterpolableColorForIndex(WebkitActivelink);
    case CSSValueWebkitLink:
        return createInterpolableColorForIndex(WebkitLink);
    case CSSValueWebkitText:
        return createInterpolableColorForIndex(WebkitText);
    case CSSValueWebkitFocusRingColor:
        return createInterpolableColor(LayoutTheme::theme().focusRingColor());
    default:
        ASSERT(CSSPropertyParser::isColorKeyword(keyword));
        return createInterpolableColor(StyleColor::colorFromKeyword(keyword));
    }
}

PassOwnPtr<InterpolableValue> ColorInterpolationType::createInterpolableColor(const StyleColor& color)
{
    if (color.isCurrentColor())
        return createInterpolableColorForIndex(Currentcolor);
    return createInterpolableColor(color.color());
}

PassOwnPtr<InterpolableValue> ColorInterpolationType::maybeCreateInterpolableColor(const CSSValue& value)
{
    if (value.isColorValue())
        return createInterpolableColor(toCSSColorValue(value).value());
    if (!value.isPrimitiveValue())
        return nullptr;
    const CSSPrimitiveValue& primitive = toCSSPrimitiveValue(value);
    if (!primitive.isValueID())
        return nullptr;
    if (!CSSPropertyParser::isColorKeyword(primitive.getValueID()))
        return nullptr;
    return createInterpolableColor(primitive.getValueID());
}

static void addPremultipliedColor(double& red, double& green, double& blue, double& alpha, double fraction, const Color& color)
{
    double colorAlpha = color.alpha();
    red += fraction * color.red() * colorAlpha;
    green += fraction * color.green() * colorAlpha;
    blue += fraction * color.blue() * colorAlpha;
    alpha += fraction * colorAlpha;
}

Color ColorInterpolationType::resolveInterpolableColor(const InterpolableValue& interpolableColor, const StyleResolverState& state, bool isVisited, bool isTextDecoration)
{
    const InterpolableList& list = toInterpolableList(interpolableColor);
    ASSERT(list.length() == InterpolableColorIndexCount);

    double red = toInterpolableNumber(list.get(Red))->value();
    double green = toInterpolableNumber(list.get(Green))->value();
    double blue = toInterpolableNumber(list.get(Blue))->value();
    double alpha = toInterpolableNumber(list.get(Alpha))->value();

    if (double currentcolorFraction = toInterpolableNumber(list.get(Currentcolor))->value()) {
        auto currentColorGetter = isVisited ? ColorPropertyFunctions::getVisitedColor : ColorPropertyFunctions::getUnvisitedColor;
        StyleColor currentStyleColor = StyleColor::currentColor();
        if (isTextDecoration)
            currentStyleColor = currentColorGetter(CSSPropertyWebkitTextFillColor, *state.style());
        if (currentStyleColor.isCurrentColor())
            currentStyleColor = currentColorGetter(CSSPropertyColor, *state.style());
        addPremultipliedColor(red, green, blue, alpha, currentcolorFraction, currentStyleColor.color());
    }
    const TextLinkColors& colors = state.document().textLinkColors();
    if (double webkitActivelinkFraction = toInterpolableNumber(list.get(WebkitActivelink))->value())
        addPremultipliedColor(red, green, blue, alpha, webkitActivelinkFraction, colors.activeLinkColor());
    if (double webkitLinkFraction = toInterpolableNumber(list.get(WebkitLink))->value())
        addPremultipliedColor(red, green, blue, alpha, webkitLinkFraction, isVisited ? colors.visitedLinkColor() : colors.linkColor());
    if (double webkitTextFraction = toInterpolableNumber(list.get(WebkitText))->value())
        addPremultipliedColor(red, green, blue, alpha, webkitTextFraction, colors.textColor());

    alpha = clampTo<double>(alpha, 0, 255);
    if (alpha == 0)
        return Color::transparent;

    return makeRGBA(
        round(red / alpha),
        round(green / alpha),
        round(blue / alpha),
        round(alpha));
}

class ParentColorChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentColorChecker> create(CSSPropertyID property, const StyleColor& color)
    {
        return adoptPtr(new ParentColorChecker(property, color));
    }

private:
    ParentColorChecker(CSSPropertyID property, const StyleColor& color)
        : m_property(property)
        , m_color(color)
    { }

    bool isValid(const StyleResolverState& state) const final
    {
        return m_color == ColorPropertyFunctions::getUnvisitedColor(m_property, *state.parentStyle());
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ConversionChecker::trace(visitor);
    }

    const CSSPropertyID m_property;
    const StyleColor m_color;
};

PassOwnPtr<InterpolationValue> ColorInterpolationType::maybeConvertNeutral() const
{
    return convertStyleColorPair(StyleColor(Color::transparent), StyleColor(Color::transparent));
}

PassOwnPtr<InterpolationValue> ColorInterpolationType::maybeConvertInitial() const
{
    const StyleColor initialColor = ColorPropertyFunctions::getInitialColor(m_property);
    return convertStyleColorPair(initialColor, initialColor);
}

PassOwnPtr<InterpolationValue> ColorInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;
    // Visited color can never explicitly inherit from parent visited color so only use the unvisited color.
    const StyleColor inheritedColor = ColorPropertyFunctions::getUnvisitedColor(m_property, *state->parentStyle());
    conversionCheckers.append(ParentColorChecker::create(m_property, inheritedColor));
    return convertStyleColorPair(inheritedColor, inheritedColor);
}

enum InterpolableColorPairIndex {
    Unvisited,
    Visited,
    InterpolableColorPairIndexCount,
};

PassOwnPtr<InterpolationValue> ColorInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (m_property == CSSPropertyColor && value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueCurrentcolor)
        return maybeConvertInherit(state, conversionCheckers);

    OwnPtr<InterpolableValue> interpolableColor = maybeCreateInterpolableColor(value);
    if (!interpolableColor)
        return nullptr;
    OwnPtr<InterpolableList> colorPair = InterpolableList::create(InterpolableColorPairIndexCount);
    colorPair->set(Unvisited, interpolableColor->clone());
    colorPair->set(Visited, interpolableColor.release());
    return InterpolationValue::create(*this, colorPair.release());
}

PassOwnPtr<InterpolationValue> ColorInterpolationType::convertStyleColorPair(const StyleColor& unvisitedColor, const StyleColor& visitedColor) const
{
    OwnPtr<InterpolableList> colorPair = InterpolableList::create(InterpolableColorPairIndexCount);
    colorPair->set(Unvisited, createInterpolableColor(unvisitedColor));
    colorPair->set(Visited, createInterpolableColor(visitedColor));
    return InterpolationValue::create(*this, colorPair.release());
}

PassOwnPtr<InterpolationValue> ColorInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    return convertStyleColorPair(
        ColorPropertyFunctions::getUnvisitedColor(m_property, *state.style()),
        ColorPropertyFunctions::getVisitedColor(m_property, *state.style()));
}

void ColorInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue*, StyleResolverState& state) const
{
    const InterpolableList& colorPair = toInterpolableList(interpolableValue);
    ASSERT(colorPair.length() == InterpolableColorPairIndexCount);
    ColorPropertyFunctions::setUnvisitedColor(m_property, *state.style(),
        resolveInterpolableColor(*colorPair.get(Unvisited), state, false, m_property == CSSPropertyTextDecorationColor));
    ColorPropertyFunctions::setVisitedColor(m_property, *state.style(),
        resolveInterpolableColor(*colorPair.get(Visited), state, true, m_property == CSSPropertyTextDecorationColor));
}

} // namespace blink
