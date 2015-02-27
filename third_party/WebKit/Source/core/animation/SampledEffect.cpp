// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SampledEffect.h"

#include "core/animation/StyleInterpolation.h"

namespace blink {

SampledEffect::SampledEffect(Animation* animation, PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > interpolations)
    : m_animation(animation)
    , m_player(animation->player())
    , m_interpolations(interpolations)
    , m_sequenceNumber(animation->player()->sequenceNumber())
    , m_priority(animation->priority())
{
    ASSERT(m_interpolations && !m_interpolations->isEmpty());
}

void SampledEffect::clear()
{
    m_player = nullptr;
    m_animation = nullptr;
    m_interpolations->clear();
}

DEFINE_TRACE(SampledEffect)
{
    visitor->trace(m_animation);
    visitor->trace(m_player);
#if ENABLE(OILPAN)
    visitor->trace(m_interpolations);
#endif
}

} // namespace blink
