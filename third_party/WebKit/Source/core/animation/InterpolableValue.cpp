// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/InterpolableValue.h"

namespace WebCore {

PassOwnPtr<InterpolableValue> InterpolableNumber::interpolate(const InterpolableValue &to, const double progress) const
{
    const InterpolableNumber* toNumber = toInterpolableNumber(&to);
    if (!progress)
        return create(m_value);
    if (progress == 1)
        return create(toNumber->m_value);
    return create(m_value * (1 - progress) + toNumber->m_value * progress);
}

PassOwnPtr<InterpolableValue> InterpolableBool::interpolate(const InterpolableValue &to, const double progress) const
{
    if (progress < 0.5) {
        return clone();
    }
    return to.clone();
}

PassOwnPtr<InterpolableValue> InterpolableList::interpolate(const InterpolableValue &to, const double progress) const
{
    const InterpolableList* toList = toInterpolableList(&to);
    ASSERT(toList->m_size == m_size);

    if (!progress) {
        return create(*this);
    }
    if (progress == 1) {
        return InterpolableList::create(*toList);
    }

    OwnPtr<InterpolableList> result = create(m_size);
    for (size_t i = 0; i < m_size; i++) {
        ASSERT(m_values.get()[i]);
        ASSERT(toList->m_values.get()[i]);
        result->set(i, m_values.get()[i]->interpolate(*(toList->m_values.get()[i]), progress));
    }
    return result.release();
}

}
