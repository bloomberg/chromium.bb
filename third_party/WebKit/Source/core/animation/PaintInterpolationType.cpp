// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/PaintInterpolationType.h"

#include "core/animation/ColorInterpolationType.h"
#include "core/animation/PaintPropertyFunctions.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

PassOwnPtr<InterpolationValue> PaintInterpolationType::maybeConvertNeutral() const
{
    return InterpolationValue::create(*this, ColorInterpolationType::createInterpolableColor(Color::transparent));
}

PassOwnPtr<InterpolationValue> PaintInterpolationType::maybeConvertInitial() const
{
    StyleColor initialColor;
    if (!PaintPropertyFunctions::getInitialColor(m_property, initialColor))
        return nullptr;
    return InterpolationValue::create(*this, ColorInterpolationType::createInterpolableColor(initialColor));
}

class ParentPaintChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentPaintChecker> create(CSSPropertyID property, const StyleColor& color)
    {
        return adoptPtr(new ParentPaintChecker(property, color));
    }
    static PassOwnPtr<ParentPaintChecker> create(CSSPropertyID property)
    {
        return adoptPtr(new ParentPaintChecker(property));
    }

private:
    ParentPaintChecker(CSSPropertyID property)
        : m_property(property)
        , m_validColor(false)
    { }
    ParentPaintChecker(CSSPropertyID property, const StyleColor& color)
        : m_property(property)
        , m_validColor(true)
        , m_color(color)
    { }

    bool isValid(const StyleResolverState& state) const final
    {
        StyleColor parentColor;
        if (!PaintPropertyFunctions::getColor(m_property, *state.parentStyle(), parentColor))
            return !m_validColor;
        return m_validColor && parentColor == m_color;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ConversionChecker::trace(visitor);
    }

    const CSSPropertyID m_property;
    const bool m_validColor;
    const StyleColor m_color;
};

PassOwnPtr<InterpolationValue> PaintInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;
    StyleColor parentColor;
    if (!PaintPropertyFunctions::getColor(m_property, *state->parentStyle(), parentColor)) {
        conversionCheckers.append(ParentPaintChecker::create(m_property));
        return nullptr;
    }
    conversionCheckers.append(ParentPaintChecker::create(m_property, parentColor));
    return InterpolationValue::create(*this, ColorInterpolationType::createInterpolableColor(parentColor));
}

PassOwnPtr<InterpolationValue> PaintInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState*, ConversionCheckers&) const
{
    OwnPtr<InterpolableValue> interpolableColor = ColorInterpolationType::maybeCreateInterpolableColor(value);
    if (!interpolableColor)
        return nullptr;
    return InterpolationValue::create(*this, interpolableColor.release());
}

PassOwnPtr<InterpolationValue> PaintInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    // TODO(alancutter): Support capturing and animating with the visited paint color.
    StyleColor underlyingColor;
    if (!PaintPropertyFunctions::getColor(m_property, *state.style(), underlyingColor))
        return nullptr;
    return InterpolationValue::create(*this, ColorInterpolationType::createInterpolableColor(underlyingColor));
}

void PaintInterpolationType::apply(const InterpolableValue& interpolableColor, const NonInterpolableValue*, StyleResolverState& state) const
{
    PaintPropertyFunctions::setColor(m_property, *state.style(), ColorInterpolationType::resolveInterpolableColor(interpolableColor, state));
}

} // namespace blink
