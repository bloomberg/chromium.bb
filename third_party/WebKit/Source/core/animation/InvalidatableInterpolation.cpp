// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/InvalidatableInterpolation.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

void InvalidatableInterpolation::interpolate(int, double fraction)
{
    if (fraction == m_currentFraction)
        return;

    if (m_currentFraction == 0 || m_currentFraction == 1 || fraction == 0 || fraction == 1)
        clearCache();

    m_currentFraction = fraction;
    if (m_isCached && m_cachedPairConversion)
        m_cachedPairConversion->interpolateValue(fraction, m_cachedValue);
    // We defer the interpolation to ensureValidInterpolation() if m_cachedPairConversion is null.
}

PassOwnPtr<PairwisePrimitiveInterpolation> InvalidatableInterpolation::maybeConvertPairwise(const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue) const
{
    ASSERT(m_currentFraction != 0 && m_currentFraction != 1);
    for (const auto& interpolationType : m_interpolationTypes) {
        if ((m_startKeyframe->isNeutral() || m_endKeyframe->isNeutral()) && (!underlyingValue || underlyingValue->type() != *interpolationType))
            continue;
        OwnPtr<PairwisePrimitiveInterpolation> pairwiseConversion = interpolationType->maybeConvertPairwise(*m_startKeyframe, *m_endKeyframe, environment, underlyingValue, m_conversionCheckers);
        if (pairwiseConversion) {
            ASSERT(pairwiseConversion->type() == *interpolationType);
            return pairwiseConversion.release();
        }
    }
    return nullptr;
}

PassOwnPtr<InterpolationValue> InvalidatableInterpolation::convertSingleKeyframe(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue) const
{
    if (keyframe.isNeutral() && !underlyingValue)
        return nullptr;
    for (const auto& interpolationType : m_interpolationTypes) {
        if (keyframe.isNeutral() && underlyingValue->type() != *interpolationType)
            continue;
        OwnPtr<InterpolationValue> result = interpolationType->maybeConvertSingle(keyframe, environment, underlyingValue, m_conversionCheckers);
        if (result) {
            ASSERT(result->type() == *interpolationType);
            return result.release();
        }
    }
    ASSERT(keyframe.isNeutral());
    return nullptr;
}

PassOwnPtr<InterpolationValue> InvalidatableInterpolation::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    for (const auto& interpolationType : m_interpolationTypes) {
        OwnPtr<InterpolationValue> result = interpolationType->maybeConvertUnderlyingValue(environment);
        if (result)
            return result.release();
    }
    return nullptr;
}

bool InvalidatableInterpolation::dependsOnUnderlyingValue() const
{
    return (m_startKeyframe->underlyingFraction() != 0 && m_currentFraction != 1) || (m_endKeyframe->underlyingFraction() != 0 && m_currentFraction != 0);
}

bool InvalidatableInterpolation::isNeutralKeyframeActive() const
{
    return (m_startKeyframe->isNeutral() && m_currentFraction != 1) || (m_endKeyframe->isNeutral() && m_currentFraction != 0);
}

void InvalidatableInterpolation::clearCache() const
{
    m_isCached = false;
    m_cachedPairConversion.clear();
    m_conversionCheckers.clear();
    m_cachedValue.clear();
}

bool InvalidatableInterpolation::isCacheValid(const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue) const
{
    if (!m_isCached)
        return false;
    if (isNeutralKeyframeActive()) {
        if (m_cachedPairConversion && m_cachedPairConversion->isFlip())
            return false;
        // Pairwise interpolation can never happen between different InterpolationTypes, neutral values always represent the underlying value.
        if (!underlyingValue || !m_cachedValue || m_cachedValue->type() != underlyingValue->type())
            return false;
    }
    for (const auto& checker : m_conversionCheckers) {
        if (!checker->isValid(environment, underlyingValue))
            return false;
    }
    return true;
}

const InterpolationValue* InvalidatableInterpolation::ensureValidInterpolation(const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue) const
{
    ASSERT(!std::isnan(m_currentFraction));
    if (isCacheValid(environment, underlyingValue))
        return m_cachedValue.get();
    clearCache();
    if (m_currentFraction == 0) {
        m_cachedValue = convertSingleKeyframe(*m_startKeyframe, environment, underlyingValue);
    } else if (m_currentFraction == 1) {
        m_cachedValue = convertSingleKeyframe(*m_endKeyframe, environment, underlyingValue);
    } else {
        OwnPtr<PairwisePrimitiveInterpolation> pairwiseConversion = maybeConvertPairwise(environment, underlyingValue);
        if (pairwiseConversion) {
            m_cachedValue = pairwiseConversion->initialValue();
            m_cachedPairConversion = pairwiseConversion.release();
        } else {
            m_cachedPairConversion = FlipPrimitiveInterpolation::create(
                convertSingleKeyframe(*m_startKeyframe, environment, underlyingValue),
                convertSingleKeyframe(*m_endKeyframe, environment, underlyingValue));
        }
        m_cachedPairConversion->interpolateValue(m_currentFraction, m_cachedValue);
    }
    m_isCached = true;
    return m_cachedValue.get();
}

void InvalidatableInterpolation::setFlagIfInheritUsed(InterpolationEnvironment& environment) const
{
    if (!m_property.isCSSProperty() && !m_property.isPresentationAttribute())
        return;
    if (!environment.state().parentStyle())
        return;
    const CSSValue* startValue = toCSSPropertySpecificKeyframe(m_startKeyframe)->value();
    const CSSValue* endValue = toCSSPropertySpecificKeyframe(m_endKeyframe)->value();
    if ((startValue && startValue->isInheritedValue())
        || (endValue && endValue->isInheritedValue())) {
        environment.state().parentStyle()->setHasExplicitlyInheritedProperties();
    }
}

double InvalidatableInterpolation::underlyingFraction() const
{
    if (m_currentFraction == 0)
        return m_startKeyframe->underlyingFraction();
    if (m_currentFraction == 1)
        return m_endKeyframe->underlyingFraction();
    return m_cachedPairConversion->interpolateUnderlyingFraction(m_startKeyframe->underlyingFraction(), m_endKeyframe->underlyingFraction(), m_currentFraction);
}

void InvalidatableInterpolation::applyStack(const ActiveInterpolations& interpolations, InterpolationEnvironment& environment)
{
    ASSERT(!interpolations.isEmpty());
    size_t startingIndex = 0;

    // Compute the underlying value to composite onto.
    UnderlyingValue underlyingValue;
    const InvalidatableInterpolation& firstInterpolation = toInvalidatableInterpolation(*interpolations.at(startingIndex));
    if (firstInterpolation.dependsOnUnderlyingValue()) {
        underlyingValue.set(firstInterpolation.maybeConvertUnderlyingValue(environment));
    } else {
        const InterpolationValue* firstValue = firstInterpolation.ensureValidInterpolation(environment, UnderlyingValue());
        // Fast path for replace interpolations that are the only one to apply.
        if (interpolations.size() == 1) {
            if (firstValue) {
                firstInterpolation.setFlagIfInheritUsed(environment);
                firstValue->type().apply(firstValue->interpolableValue(), firstValue->nonInterpolableValue(), environment);
            }
            return;
        }
        underlyingValue.set(firstValue);
        startingIndex++;
    }

    // Composite interpolations onto the underlying value.
    bool shouldApply = false;
    for (size_t i = startingIndex; i < interpolations.size(); i++) {
        const InvalidatableInterpolation& currentInterpolation = toInvalidatableInterpolation(*interpolations.at(i));
        ASSERT(currentInterpolation.dependsOnUnderlyingValue());
        const InterpolationValue* currentValue = currentInterpolation.ensureValidInterpolation(environment, underlyingValue);
        if (!currentValue)
            continue;
        shouldApply = true;
        currentInterpolation.setFlagIfInheritUsed(environment);
        double underlyingFraction = currentInterpolation.underlyingFraction();
        if (underlyingFraction == 0 || !underlyingValue || underlyingValue->type() != currentValue->type())
            underlyingValue.set(currentValue);
        else
            currentValue->type().composite(underlyingValue, underlyingFraction, *currentValue);
    }

    if (shouldApply && underlyingValue)
        underlyingValue->type().apply(underlyingValue->interpolableValue(), underlyingValue->nonInterpolableValue(), environment);
}

} // namespace blink
