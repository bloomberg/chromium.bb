// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/InterpolationEffect.h"

namespace blink {

void InterpolationEffect::getActiveInterpolations(double fraction, double iterationDuration, OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>>& result) const
{
    if (!result)
        result = adoptPtrWillBeNoop(new WillBeHeapVector<RefPtrWillBeMember<Interpolation> >());

    size_t existingSize = result->size();
    size_t resultIndex = 0;

    for (const auto& record : m_interpolations) {
        if (fraction >= record->m_applyFrom && fraction < record->m_applyTo) {
            RefPtrWillBeRawPtr<Interpolation> interpolation = record->m_interpolation;
            double localFraction = (fraction - record->m_start) / (record->m_end - record->m_start);
            if (record->m_easing)
                localFraction = record->m_easing->evaluate(localFraction, accuracyForDuration(iterationDuration));
            interpolation->interpolate(0, localFraction);
            if (resultIndex < existingSize)
                (*result)[resultIndex++] = interpolation;
            else
                result->append(interpolation);
        }
    }
    if (resultIndex < existingSize)
        result->shrink(resultIndex);
}

void InterpolationEffect::InterpolationRecord::trace(Visitor* visitor)
{
    visitor->trace(m_interpolation);
}

void InterpolationEffect::trace(Visitor* visitor)
{
#if ENABLE_OILPAN
    visitor->trace(m_interpolations);
#endif
}

}
