// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SampledEffect.h"

#include "core/animation/SVGInterpolation.h"
#include "core/animation/StyleInterpolation.h"
#include "core/svg/SVGElement.h"

namespace blink {

SampledEffect::SampledEffect(KeyframeEffect* effect)
    : m_effect(effect)
    , m_animation(effect->animation())
    , m_sequenceNumber(effect->animation()->sequenceNumber())
    , m_priority(effect->priority())
{
}

void SampledEffect::clear()
{
    m_effect = nullptr;
    m_animation = nullptr;
    m_interpolations.clear();
}

DEFINE_TRACE(SampledEffect)
{
    visitor->trace(m_effect);
    visitor->trace(m_animation);
}

void SampledEffect::applySVGUpdate(SVGElement& targetElement)
{
    for (const auto& interpolation : m_interpolations) {
        if (interpolation->isSVGInterpolation()) {
            toSVGInterpolation(interpolation.get())->apply(targetElement);
        }
    }
}

} // namespace blink
