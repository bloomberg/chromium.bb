// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/IdleCallbackDeadline.h"

#include "core/loader/DocumentLoadTiming.h"
#include "wtf/CurrentTime.h"

namespace blink {

IdleCallbackDeadline::IdleCallbackDeadline(double deadlineMillis, CallbackType callbackType, const DocumentLoadTiming& timing)
    : m_deadlineMillis(deadlineMillis)
    , m_callbackType(callbackType)
    , m_timing(timing)
{
}

double IdleCallbackDeadline::timeRemaining() const
{
    double timeRemaining = m_deadlineMillis - (1000 * m_timing.monotonicTimeToZeroBasedDocumentTime(monotonicallyIncreasingTime()));
    if (timeRemaining < 0)
        timeRemaining = 0;
    return timeRemaining;
}

} // namespace blink
